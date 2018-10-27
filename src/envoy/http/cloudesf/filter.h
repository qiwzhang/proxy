#pragma once

#include "common/common/logger.h"
#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include <string>

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

// The Envoy filter for Cloud ESF service control client.
class Filter : public Http::StreamDecoderFilter,
                       public Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(Upstream::ClusterManager& cm) : cm_(cm) {}

  void onDestroy() override {}
  
  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap& headers,
                                          bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::HeaderMap&) override;
  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks& callbacks) override;

 private:
  Upstream::ClusterManager& cm_;
  // The callback funcion.
  Http::StreamDecoderFilterCallbacks* decoder_callbacks_;

  // The state of the request
  enum State { Init, Calling, Responded, Complete };
  State state_ = Init;
  // Mark if request has been stopped.
  bool stopped_ = false;
};

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
