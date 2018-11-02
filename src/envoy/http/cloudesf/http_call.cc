#include "src/envoy/http/cloudesf/token_fetcher.h"

#include "common/common/enum_to_int.h"
#include "common/http/headers.h"
#include "common/http/message_impl.h"
#include "common/http/utility.h"

using ::envoy::config::filter::http::cloudesf::HttpUri;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {
namespace {

const char KApplicationProto[] = "application/x-protobuf";
  
Http::MessagePtr PrepareHeaders(const std::string& uri) {
  absl::string_view host, path;
  Http::Utility::extractHostPathFromUri(uri, host, path);

  Http::MessagePtr message(new Http::RequestMessageImpl());
  message->headers().insertPath().value(path.data(), path.size());
  message->headers().insertHost().value(host.data(), host.size());

  return message;
}

class HttpFetcherImpl : public HttpFetcher,
                         public Logger::Loggable<Logger::Id::filter>,
                         public Http::AsyncClient::Callbacks {
 public:
  HttpFetcherImpl(Upstream::ClusterManager& cm, const HttpUri& uri) : cm_(cm), http_uri_(uri) {
    ENVOY_LOG(trace, "{}", __func__);
  }

  ~HttpFetcherImpl() { cancel(); }

  void cancel() override {
    if (request_ && !complete_) {
      request_->cancel();
      ENVOY_LOG(debug, "Http call [uri = {}]: canceled", uri_->uri());
    }
    reset();
  }

  void fetch(const std::string& suffix_url, const std::string& token,
	     const std::string& body, doneFunc on_done) override {
    ENVOY_LOG(trace, "{}", __func__);
    
    ASSERT(!on_done_);
    complete_ = false;
    on_done_ = on_done;

    uri_ = http_uri_.uri() + suffix_url;
    
    Http::MessagePtr message = PrepareHeaders(uri_);
    message->headers().insertMethod().value().setReference(
        Http::Headers::get().MethodValues.Post);

    message->body().reset(new Buffer::OwnedImpl(body.data(), body.size()));
    message->headers().insertContentLength().value(body.size());
    
    std::string token_value = "Bearer " + token;
    message->headers().insertAuthorization().value(token_value.data(), token_value.size());

    message->headers().insertContentType().value(KApplicationProto, sizeof(KApplicationProto);
    ENVOY_LOG(debug, "http call from [uri = {}]: start", uri_);
    request_ =
        cm_.httpAsyncClientForCluster(http_uri_.cluster())
            .send(std::move(message), *this,
                  std::chrono::milliseconds(
                      DurationUtil::durationToMilliseconds(http_uri_.timeout())));
  }

  // HTTP async receive methods
  void onSuccess(Http::MessagePtr&& response) {
    ENVOY_LOG(trace, "{}", __func__);
    complete_ = true;
    const uint64_t status_code =
        Http::Utility::getResponseStatus(response->headers());
    if (status_code == enumToInt(Http::Code::OK)) {
      ENVOY_LOG(debug, "http call [uri = {}]: success", uri_);
      if (response->body()) {
        const auto len = response->body()->length();
        const auto body = std::string(
            static_cast<char*>(response->body()->linearize(len)), len);
	on_done(Status::OK, body);
      } else {
	ENVOY_LOG(debug, "http call [uri = {}]: empty response", uri_);
	on_done(Status(Code::INTERNAL, "Failed to call service control"), std::string());
      }
    } else {
      ENVOY_LOG(debug, "fetch access_token: response status code {}",
                status_code);
      on_done(Status(Code::INTERNAL, "Failed to call service control"), std::string());
    }
    reset();
  }

  void onFailure(Http::AsyncClient::FailureReason reason) {
    ENVOY_LOG(debug, "fetch access_token: network error {}", enumToInt(reason));
    complete_ = true;    
    on_done(Status(Code::INTERNAL, "Failed to call service control"), std::string());
    reset();
  }

 private:
  Upstream::ClusterManager& cm_;
  const HttpUri& http_uri_;
  std::string uri_;
  bool complete_{};
  HttpFetcher::TokenReceiver* receiver_{};
  const HttpUri* uri_{};
  Http::AsyncClient::Request* request_{};

  void reset() {
    request_ = nullptr;
  }
};
}  // namespace

HttpFetcherPtr HttpFetcher::create(Upstream::ClusterManager& cm, const HttpUri& http_uri) {
  return std::make_unique<HttpFetcherImpl>(cm, http_uri);
}

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
