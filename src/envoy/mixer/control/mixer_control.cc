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

#include "src/envoy/mixer/mixer_control.h"
#include "include/attributes_builder.h"

#include "common/common/base64.h"
#include "common/common/utility.h"

#include <arpa/inet.h>

using ::google::protobuf::util::Status;
using StatusCode = ::google::protobuf::util::error::Code;
using ::istio::mixer::v1::Attributes;
using ::istio::mixer_client::AttributesBuilder;
using ::istio::mixer_client::CheckOptions;
using ::istio::mixer_client::DoneFunc;
using ::istio::mixer_client::MixerClientOptions;
using ::istio::mixer_client::ReportOptions;
using ::istio::mixer_client::QuotaOptions;

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {


// Pilot mesh attributes with the suffix will be treated as ipv4.
// They will use BYTES attribute type.
const std::string kIPSuffix = ".ip";

// Keys to well-known headers
const LowerCaseString kRefererHeaderKey("referer");

void SetStringAttribute(const std::string& name, const std::string& value,
                        Attributes* attr) {
  if (!value.empty()) {
    AttributesBuilder(attr).AddString(name, value);
  }
}

void SetInt64Attribute(const std::string& name, uint64_t value,
                       Attributes* attr) {
  AttributesBuilder(attr).AddInt64(name, value);
}

std::map<std::string, std::string> ExtractHeaders(const HeaderMap& header_map) {
}


void FillRequestInfoAttributes(const AccessLog::RequestInfo& info,
                               int check_status_code, Attributes* attr) {
}

void SetIPAttribute(const std::string& name, const Network::Address::Ip& ip,
                    Attributes* attr) {
  AttributesBuilder builder(attr);
  if (ip.ipv4()) {
    uint32_t ipv4 = ip.ipv4()->address();
    builder.AddBytes(
        name, std::string(reinterpret_cast<const char*>(&ipv4), sizeof(ipv4)));
  } else if (ip.ipv6()) {
    std::array<uint8_t, 16> ipv6 = ip.ipv6()->address();
    builder.AddBytes(
        name, std::string(reinterpret_cast<const char*>(ipv6.data()), 16));
  }
}

// A class to wrap envoy timer for mixer client timer.
class EnvoyTimer : public ::istio::mixer_client::Timer {
 public:
  EnvoyTimer(Event::TimerPtr timer) : timer_(std::move(timer)) {}

  void Stop() override { timer_->disableTimer(); }
  void Start(int interval_ms) override {
    timer_->enableTimer(std::chrono::milliseconds(interval_ms));
  }

 private:
  Event::TimerPtr timer_;
};

}  // namespace

MixerControl::MixerControl(const MixerConfig& mixer_config,
                           Upstream::ClusterManager& cm,
                           Event::Dispatcher& dispatcher,
                           Runtime::RandomGenerator& random)
    : cm_(cm), mixer_config_(mixer_config) {
  FactoryData options(mixer_config);
  
  options.check_transport = CheckTransport::GetFunc(cm, nullptr);
  options.report_transport = ReportTransport::GetFunc(cm);

  options.timer_create_func = [&dispatcher](std::function<void()> timer_cb)
      -> std::unique_ptr<::istio::mixer_client::Timer> {
        return std::unique_ptr<::istio::mixer_client::Timer>(
            new EnvoyTimer(dispatcher.createTimer(timer_cb)));
      };

  options.uuid_generate_func = [&random]() -> std::string {
    return random.uuid();
  };

  mixer_controller_ = ::istio::mixer_client::Controller::Create(options);
}

void MixerControl::ForwardAttributes(
    HeaderMap& headers, const Utils::StringMap& route_attributes) const {
  if (mixer_config_.forward_attributes.empty() && route_attributes.empty()) {
    return;
  }
  std::string serialized_str = Utils::SerializeTwoStringMaps(
      mixer_config_.forward_attributes, route_attributes);
  std::string base64 =
      Base64::encode(serialized_str.c_str(), serialized_str.size());
  ENVOY_LOG(debug, "Mixer forward attributes set: {}", base64);
  headers.addReferenceKey(Utils::kIstioAttributeHeader, base64);
}

void MixerControl::BuildHttpCheck(
    HttpRequestDataPtr request_data, HeaderMap& headers,
    const ::istio::mixer::v1::Attributes_StringMap& map_pb,
    const std::string& source_user, const Utils::StringMap& route_attributes,
    const Network::Connection* connection) const {

}

void MixerControl::BuildHttpReport(HttpRequestDataPtr request_data,
                                   const HeaderMap* response_headers,
                                   const AccessLog::RequestInfo& request_info,
                                   int check_status) const {
}

void MixerControl::BuildTcpCheck(HttpRequestDataPtr request_data,
                                 Network::Connection& connection,
                                 const std::string& source_user) const {
  SetStringAttribute(kSourceUser, source_user, &request_data->attributes);

  const Network::Address::Ip* remote_ip = connection.remoteAddress().ip();
  if (remote_ip) {
    SetIPAttribute(kSourceIp, *remote_ip, &request_data->attributes);
    SetInt64Attribute(kSourcePort, remote_ip->port(),
                      &request_data->attributes);
  }

  AttributesBuilder(&request_data->attributes)
      .AddTimestamp(kContextTime, std::chrono::system_clock::now());
  SetStringAttribute(kContextProtocol, "tcp", &request_data->attributes);

  mixer_config_.ExtractQuotaAttributes(&request_data->attributes);
  for (const auto& attribute : mixer_config_.mixer_attributes) {
    SetMeshAttribute(attribute.first, attribute.second,
                     &request_data->attributes);
  }
}

void MixerControl::BuildTcpReport(
    HttpRequestDataPtr request_data, uint64_t received_bytes,
    uint64_t send_bytes, int check_status_code,
    std::chrono::nanoseconds duration,
    Upstream::HostDescriptionConstSharedPtr upstreamHost) const {
  AttributesBuilder builder(&request_data->attributes);
  builder.AddInt64(kConnectionReceviedBytes, received_bytes);
  builder.AddInt64(kConnectionReceviedTotalBytes, received_bytes);
  builder.AddInt64(kConnectionSendBytes, send_bytes);
  builder.AddInt64(kConnectionSendTotalBytes, send_bytes);
  builder.AddDuration(kConnectionDuration, duration);
  builder.AddInt64(kCheckStatusCode, check_status_code);

  if (upstreamHost && upstreamHost->address()) {
    const Network::Address::Ip* destination_ip = upstreamHost->address()->ip();
    if (destination_ip) {
      SetIPAttribute(kDestinationIp, *destination_ip,
                     &request_data->attributes);
      SetInt64Attribute(kDestinationPort, destination_ip->port(),
                        &request_data->attributes);
    }
  }

  builder.AddTimestamp(kContextTime, std::chrono::system_clock::now());
}

// Mesh attributes from Pilot are all string type.
// The attributes with ".ip" suffix will be treated
// as ipv4 and use BYTES attribute type.
void MixerControl::SetMeshAttribute(const std::string& name,
                                    const std::string& value,
                                    Attributes* attr) const {
  // Check with ".ip" suffix,
  if (name.length() <= kIPSuffix.length() ||
      name.compare(name.length() - kIPSuffix.length(), kIPSuffix.length(),
                   kIPSuffix) != 0) {
    AttributesBuilder(attr).AddString(name, value);
    return;
  }

  in_addr ipv4_bytes;
  if (inet_pton(AF_INET, value.c_str(), &ipv4_bytes) == 1) {
    AttributesBuilder(attr).AddBytes(
        name, std::string(reinterpret_cast<const char*>(&ipv4_bytes),
                          sizeof(ipv4_bytes)));
    return;
  }

  in6_addr ipv6_bytes;
  if (inet_pton(AF_INET6, value.c_str(), &ipv6_bytes) == 1) {
    AttributesBuilder(attr).AddBytes(
        name, std::string(reinterpret_cast<const char*>(&ipv6_bytes),
                          sizeof(ipv6_bytes)));
    return;
  }

  ENVOY_LOG(warn, "Could not convert to ip: {}: {}", name, value);
  AttributesBuilder(attr).AddString(name, value);
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
