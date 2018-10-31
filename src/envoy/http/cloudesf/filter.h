#pragma once

#include "common/common/logger.h"
#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"
#include "src/envoy/http/cloudesf/filter_config.h"
#include "src/envoy/http/cloudesf/token_fetcher.h"

#include <string>

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

// The Envoy filter for Cloud ESF service control client.
class Filter : public Http::StreamDecoderFilter,
               public TokenFetcher::TokenReceiver,
               public Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(FilterConfigSharedPtr config) : config_(config) {}

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

 private:
  // The callback funcion.
  Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  FilterConfigSharedPtr config_;

  // The state of the request
  enum State { Init, Calling, Responded, Complete };
  State state_ = Init;
  // Mark if request has been stopped.
  bool stopped_ = false;

  TokenFetcherPtr token_fetcher_;
};

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
