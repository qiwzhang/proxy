#pragma once

#include "common/common/logger.h"
#include "src/envoy/http/cloudesf/config.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

// The Envoy filter config for Cloud ESF service control client.
class FilterConfig : public Logger::Loggable<Logger::Id::filter> {
 public:
  FilterConfig(
      const ::envoy::config::filter::http::cloudesf::FilterConfig& proto_config,
      Upstream::ClusterManager& cm)
      : proto_config_(proto_config), cm_(cm) {}

  const ::envoy::config::filter::http::cloudesf::FilterConfig& config() const {
    return proto_config_;
  }

  Upstream::ClusterManager& cm() { return cm_; }

 private:
  // The proto config.
  ::envoy::config::filter::http::cloudesf::FilterConfig proto_config_;
  Upstream::ClusterManager& cm_;
};

typedef std::shared_ptr<FilterConfig> FilterConfigSharedPtr;

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
