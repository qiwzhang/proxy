#pragma once

#include "envoy/common/pure.h"
#include "envoy/upstream/cluster_manager.h"
#include "google/protobuf/stubs/status.h"
#include "src/envoy/http/cloudesf/config.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

class HttpFetcher;
typedef std::unique_ptr<HttpFetcher> HttpFetcherPtr;

class HttpFetcher {
 public:
  using doneFunc = std::function<void(const ::google::protobuf::util::Status& status,
				    const std::string& response_body)>;
  /*
   * Cancel any in-flight request.
   */
  virtual void cancel() PURE;

  virtual void fetch(const std::string& suffix_url, const std::string& token,
		     const std::string& body, doneFunc on_done) PURE;

  /*
   * Factory method for creating a HttpFetcher.
   * @param cm the cluster manager to use during Token retrieval
   * @return a HttpFetcher instance
   */
  static HttpFetcherPtr create(Upstream::ClusterManager& cm,
      const ::envoy::config::filter::http::cloudesf::HttpUri& uri,
			       
};

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
