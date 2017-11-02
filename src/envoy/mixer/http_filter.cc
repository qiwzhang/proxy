/* Copyright 2017 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common/common/base64.h"
#include "common/common/logger.h"
#include "common/http/headers.h"
#include "common/http/utility.h"
#include "control/include/utils/status.h"
#include "envoy/registry/registry.h"
#include "envoy/ssl/connection.h"
#include "envoy/thread_local/thread_local.h"
#include "server/config/network/http_connection_manager.h"
#include "src/envoy/mixer/config.h"
#include "src/envoy/mixer/grpc_transport.h"
#include "src/envoy/mixer/http_check_data.h"
#include "src/envoy/mixer/http_report_data.h"
#include "src/envoy/mixer/mixer_control.h"
#include "src/envoy/mixer/utils.h"

#include <map>
#include <mutex>
#include <thread>

using ::google::protobuf::util::Status;

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

// Switch to turn off mixer both check and report
// They can be overrided by "mixer_check" and "mixer_report" flags.
const std::string kJsonNameMixerControl("mixer_control");

// Switch to turn on/off mixer check only.
const std::string kJsonNameMixerCheck("mixer_check");

// Switch to turn on/off mixer report only.
const std::string kJsonNameMixerReport("mixer_report");

// The prefix in route opaque data to define
// a sub string map of mixer attributes passed to mixer for the route.
const std::string kPrefixMixerAttributes("mixer_attributes.");

}  // namespace

class Config {
 private:
  Upstream::ClusterManager& cm_;
  MixerConfig mixer_config_;
  ThreadLocal::SlotPtr tls_;

 public:
  Config(const Json::Object& config,
         Server::Configuration::FactoryContext& context)
      : cm_(context.clusterManager()),
        tls_(context.threadLocal().allocateSlot()) {
    mixer_config_.Load(config);
    Runtime::RandomGenerator& random = context.random();
    tls_->set(
        [this, &random](Event::Dispatcher& dispatcher)
            -> ThreadLocal::ThreadLocalObjectSharedPtr {
              return ThreadLocal::ThreadLocalObjectSharedPtr(
                  new MixerControl(mixer_config_, cm_, dispatcher, random));
            });
  }

  MixerControl& mixer_control() { return tls_->getTyped<MixerControl>(); }
};

typedef std::shared_ptr<Config> ConfigPtr;

class Instance : public Http::StreamDecoderFilter,
                 public Http::AccessLog::Instance,
                 public Logger::Loggable<Logger::Id::http> {
 private:
  MixerControl& mixer_control_;
  std::unique_ptr<::istio::mixer_control::HttpRequestHandler> handler_;
  istio::mixer_client::CancelFunc cancel_check_;

  enum State { NotStarted, Calling, Complete, Responded };
  State state_;

  StreamDecoderFilterCallbacks* decoder_callbacks_;

  bool initiating_call_;

  bool mixer_check_disabled_;
  bool mixer_report_disabled_;

  // check mixer on/off flags in route opaque data
  void check_mixer_route_flags() {
    // Both check and report are disabled by default.
    mixer_check_disabled_ = true;
    mixer_report_disabled_ = true;
    auto route = decoder_callbacks_->route();
    if (route != nullptr) {
      auto entry = route->routeEntry();
      if (entry != nullptr) {
        auto control_key = entry->opaqueConfig().find(kJsonNameMixerControl);
        if (control_key != entry->opaqueConfig().end() &&
            control_key->second == "on") {
          mixer_check_disabled_ = false;
          mixer_report_disabled_ = false;
        }
        auto check_key = entry->opaqueConfig().find(kJsonNameMixerCheck);
        if (check_key != entry->opaqueConfig().end() &&
            check_key->second == "on") {
          mixer_check_disabled_ = false;
        }
        auto report_key = entry->opaqueConfig().find(kJsonNameMixerReport);
        if (report_key != entry->opaqueConfig().end() &&
            report_key->second == "on") {
          mixer_report_disabled_ = false;
        }
      }
    }
  }

  // Extract a prefixed string map from route opaque config.
  // Route opaque config only supports flat name value pair, have to use
  // prefix to create a sub string map. such as:
  //    prefix.key1 = value1
  std::map<std::string, std::string> GetRouteStringMap(
      const std::string& prefix) {
    std::map<std::string, std::string> attrs;
    auto route = decoder_callbacks_->route();
    if (route != nullptr) {
      auto entry = route->routeEntry();
      if (entry != nullptr) {
        for (const auto& it : entry->opaqueConfig()) {
          if (it.first.substr(0, prefix.size()) == prefix) {
            attrs[it.first.substr(prefix.size(), std::string::npos)] =
                it.second;
          }
        }
      }
    }
    return attrs;
  }

 public:
  Instance(ConfigPtr config)
      : mixer_control_(config->mixer_control()),
        state_(NotStarted),
        initiating_call_(false) {
    ENVOY_LOG(debug, "Called Mixer::Instance : {}", __func__);
  }

  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override {
    ENVOY_LOG(debug, "Called Mixer::Instance : {}", __func__);

    check_mixer_route_flags();

    auto data = std::unique_ptr<::istio::mixer_control::HttpCheckData>(
        new HttpCheckData(headers, decoder_callbacks_->connection()));
    auto per_route_config = MixerConfig::CreatePerRouteConfig(
        mixer_check_disabled_, mixer_report_disabled_,
        GetRouteStringMap(kPrefixMixerAttributes));
    handler_ = mixer_control_.controller()->CreateHttpRequestHandler(
        std::move(data), std::move(per_route_config));

    state_ = Calling;
    initiating_call_ = true;
    cancel_check_ = handler_->Check(
        CheckTransport::GetFunc(mixer_control_.cm(), &headers),
        [this](const Status& status) { completeCheck(status); });
    initiating_call_ = false;

    if (state_ == Complete) {
      return FilterHeadersStatus::Continue;
    }
    ENVOY_LOG(debug, "Called Mixer::Instance : {} Stop", __func__);
    return FilterHeadersStatus::StopIteration;
  }

  FilterDataStatus decodeData(Buffer::Instance& data,
                              bool end_stream) override {
    if (mixer_check_disabled_) {
      return FilterDataStatus::Continue;
    }

    ENVOY_LOG(debug, "Called Mixer::Instance : {} ({}, {})", __func__,
              data.length(), end_stream);
    if (state_ == Calling) {
      return FilterDataStatus::StopIterationAndBuffer;
    }
    return FilterDataStatus::Continue;
  }

  FilterTrailersStatus decodeTrailers(HeaderMap&) override {
    if (mixer_check_disabled_) {
      return FilterTrailersStatus::Continue;
    }

    ENVOY_LOG(debug, "Called Mixer::Instance : {}", __func__);
    if (state_ == Calling) {
      return FilterTrailersStatus::StopIteration;
    }
    return FilterTrailersStatus::Continue;
  }

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override {
    ENVOY_LOG(debug, "Called Mixer::Instance : {}", __func__);
    decoder_callbacks_ = &callbacks;
  }

  void completeCheck(const Status& status) {
    ENVOY_LOG(debug, "Called Mixer::Instance : check complete {}",
              status.ToString());
    // This stream has been reset, abort the callback.
    if (state_ == Responded) {
      return;
    }
    if (!status.ok() && state_ != Responded) {
      state_ = Responded;
      int status_code =
          ::istio::mixer_control::utils::StatusHttpCode(status.error_code());
      Utility::sendLocalReply(*decoder_callbacks_, false,
                              Code(status_code), status.ToString());
      return;
    }

    state_ = Complete;
    if (!initiating_call_) {
      decoder_callbacks_->continueDecoding();
    }
  }

  void onDestroy() override {
    ENVOY_LOG(debug, "Called Mixer::Instance : {} state: {}", __func__, state_);
    if (state_ != Calling) {
      cancel_check_ = nullptr;
    }
    state_ = Responded;
    if (cancel_check_) {
      ENVOY_LOG(debug, "Cancelling check call");
      cancel_check_();
      cancel_check_ = nullptr;
    }
  }

  virtual void log(const HeaderMap*, const HeaderMap* response_headers,
                   const AccessLog::RequestInfo& request_info) override {
    ENVOY_LOG(debug, "Called Mixer::Instance : {}", __func__);
    if (!handler_) return;
    auto report_data = std::unique_ptr<::istio::mixer_control::HttpReportData>(
        new HttpReportData(response_headers, request_info));
    handler_->Report(std::move(report_data));
  }
};

}  // namespace Mixer
}  // namespace Http

namespace Server {
namespace Configuration {

class MixerConfigFactory : public NamedHttpFilterConfigFactory {
 public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object& config,
                                          const std::string&,
                                          FactoryContext& context) override {
    Http::Mixer::ConfigPtr mixer_config(
        new Http::Mixer::Config(config, context));
    return
        [mixer_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
          std::shared_ptr<Http::Mixer::Instance> instance =
              std::make_shared<Http::Mixer::Instance>(mixer_config);
          callbacks.addStreamDecoderFilter(
              Http::StreamDecoderFilterSharedPtr(instance));
          callbacks.addAccessLogHandler(
              Http::AccessLog::InstanceSharedPtr(instance));
        };
  }
  std::string name() override { return "mixer"; }
};

static Registry::RegisterFactory<MixerConfigFactory,
                                 NamedHttpFilterConfigFactory>
    register_;

}  // namespace Configuration
}  // namespace Server
}  // namespace Envoy
