
#include "src/envoy/http/cloudesf/config.pb.h"
#include "src/envoy/http/cloudesf/config.pb.validate.h"
#include "src/envoy/http/cloudesf/filter.h"

#include "envoy/registry/registry.h"
#include "extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

const std::string FilterName = "envoy.filters.http.cloud_esf";

/**
 * Config registration for cloudESF service control filter.
 */
class FilterFactory
    : public Common::FactoryBase<
          ::envoy::config::filter::http::cloudesf::FilterConfig> {
 public:
  FilterFactory() : FactoryBase(FilterName) {}

 private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const ::envoy::config::filter::http::cloudesf::FilterConfig&,
      const std::string&,
      Server::Configuration::FactoryContext& context) override {
    return [&context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamDecoderFilter(
          std::make_shared<Filter>(context.clusterManager()));
    };
  }
};

/**
 * Static registration for the rate limit filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<
    FilterFactory, Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
