#pragma once

#include "common/common/logger.h"
#include "envoy/access_log/access_log.h"
#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"
#include "src/envoy/http/cloudesf/filter_config.h"
#include "src/envoy/http/cloudesf/http_call.h"
#include "src/envoy/http/cloudesf/token_fetcher.h"

#include <string>

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

// The Envoy filter for Cloud ESF service control client.
class Filter : public Http::StreamDecoderFilter,
               public AccessLog::Instance,
               public TokenFetcher::TokenReceiver,
               public Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(FilterConfigSharedPtr config) : config_(config) {}

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap& headers,
                                          bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::HeaderMap&) override;
  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks& callbacks) override;

  // Implmeneted functions for TokenFetcher::TokenReceiver
  void onTokenSuccess(const std::string& token, int expires_in) override;
  void onTokenError(TokenFetcher::TokenReceiver::Failure reason) override;

  // Called when the request is completed.
  void log(const Http::HeaderMap* request_headers,
           const Http::HeaderMap* response_headers,
           const Http::HeaderMap* response_trailers,
           const StreamInfo::StreamInfo& stream_info) override;

 private:
  void onCheckResponse(const ::google::protobuf::util::Status& status,
                       const std::string& response_json);

  // The callback funcion.
  Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  FilterConfigSharedPtr config_;

  void ExtractRequestInfo(const Http::HeaderMap&);

  // The state of the request
  enum State { Init, Calling, Responded, Complete };
  State state_ = Init;
  // Mark if request has been stopped.
  bool stopped_ = false;

  TokenFetcherPtr token_fetcher_;
  std::string token_;
  std::string uuid_;
  std::string operation_name_;
  std::string api_key_;
  std::string http_method_;

  ::google::service_control::CheckResponseInfo check_response_info_;
  ::google::protobuf::util::Status check_status_;
  HttpCall* check_call_{};
};

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
