#include "src/envoy/http/cloudesf/http_call.h"

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

// const char KApplicationProto[] = "application/x-protobuf";
// TODO: (qiwzhang) somehow using x-protobuf occasionally return 400
// need to figure out why, before that, use json.
const char KApplicationProto[] = "application/json";

class HttpCallImpl : public HttpCall,
                     public Logger::Loggable<Logger::Id::filter>,
                     public Http::AsyncClient::Callbacks {
 public:
  HttpCallImpl(Upstream::ClusterManager& cm, const HttpUri& uri)
      : cm_(cm), http_uri_(uri) {
    ENVOY_LOG(trace, "{}", __func__);
  }

  ~HttpCallImpl() {}

  void cancel() override {
    if (request_) {
      request_->cancel();
      ENVOY_LOG(debug, "Http call [uri = {}]: canceled", uri_);
      reset();
    }
    delete this;
  }

  Http::MessagePtr prepareHeaders(const std::string& suffix_url,
                                  const std::string& token,
                                  const Protobuf::Message& body) {
    uri_ = http_uri_.uri() + suffix_url;
    absl::string_view host, path;
    Http::Utility::extractHostPathFromUri(uri_, host, path);

    Http::MessagePtr message(new Http::RequestMessageImpl());
    message->headers().insertPath().value(path.data(), path.size());
    message->headers().insertHost().value(host.data(), host.size());

    message->headers().insertMethod().value().setReference(
        Http::Headers::get().MethodValues.Post);

    //    std::string str_body = body.SerializeAsString();
    std::string str_body =
        MessageUtil::getJsonStringFromMessage(body, false, false);
    message->body().reset(
        new Buffer::OwnedImpl(str_body.data(), str_body.size()));
    message->headers().insertContentLength().value(message->body()->length());

    std::string token_value = "Bearer " + token;
    message->headers().insertAuthorization().value(token_value.data(),
                                                   token_value.size());

    message->headers().insertContentType().value(KApplicationProto,
                                                 sizeof(KApplicationProto));
    return message;
  }

  void call(const std::string& suffix_url, const std::string& token,
            const Protobuf::Message& body,
            HttpCall::DoneFunc on_done) override {
    ENVOY_LOG(trace, "{}", __func__);

    ASSERT(!on_done_);
    on_done_ = on_done;

    Http::MessagePtr message = prepareHeaders(suffix_url, token, body);
    ENVOY_LOG(debug, "http call from [uri = {}]: start", uri_);
    request_ =
        cm_.httpAsyncClientForCluster(http_uri_.cluster())
            .send(
                std::move(message), *this,
                std::chrono::milliseconds(
                    DurationUtil::durationToMilliseconds(http_uri_.timeout())));
  }

  // HTTP async receive methods
  void onSuccess(Http::MessagePtr&& response) {
    ENVOY_LOG(trace, "{}", __func__);
    const uint64_t status_code =
        Http::Utility::getResponseStatus(response->headers());
    std::string body;
    if (response->body()) {
      const auto len = response->body()->length();
      body = std::string(static_cast<char*>(response->body()->linearize(len)),
                         len);
    }
    if (status_code == enumToInt(Http::Code::OK)) {
      if (!body.empty()) {
        ENVOY_LOG(debug, "http call [uri = {}]: success with body {}", uri_,
                  body);
        on_done_(Status::OK, body);
      } else {
        ENVOY_LOG(debug, "http call [uri = {}]: empty response", uri_);
        on_done_(Status(Code::INTERNAL, "Failed to call service control"),
                 body);
      }
    } else {
      ENVOY_LOG(debug, "http call response status code: {}, body: {}",
                status_code, body);
      on_done_(Status(Code::INTERNAL, "Failed to call service control"), body);
    }
    reset();
    delete this;
  }

  void onFailure(Http::AsyncClient::FailureReason reason) {
    ENVOY_LOG(debug, "http call network error {}", enumToInt(reason));
    on_done_(Status(Code::INTERNAL, "Failed to call service control"),
             std::string());
    reset();
    delete this;
  }

 private:
  Upstream::ClusterManager& cm_;
  const HttpUri& http_uri_;
  std::string uri_;
  Http::AsyncClient::Request* request_{};
  HttpCall::DoneFunc on_done_;

  void reset() { request_ = nullptr; }
};

}  // namespace

HttpCall* HttpCall::create(Upstream::ClusterManager& cm,
                           const HttpUri& http_uri) {
  return new HttpCallImpl(cm, http_uri);
}

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
