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

#include "control.h"

#include "common/http/message_impl.h"
#include "envoy/http/async_client.h"

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

// The authorization HTTP header.
const LowerCaseString kAuthorizationHeaderKey("authorization");

// The autorization bearer prefix.
const std::string kBearerPrefix = "Bearer ";

// The HTTP header to pass verified token payload.
const LowerCaseString kAuthorizedHeaderKey("sec-istio-auth-userinfo");

// Extract host and path from a URI
void ExtractUriHostPath(const std::string& uri, std::string* host,
                        std::string* path) {
  // Example:
  // uri  = "https://example.com/certs"
  // pos  :          ^
  // pos1 :                     ^
  // host = "example.com"
  // path = "/certs"
  auto pos = uri.find("://");
  pos = pos == std::string::npos ? 0 : pos + 3;  // Start position of host
  auto pos1 = uri.find("/", pos);
  if (pos1 == std::string::npos) pos1 = uri.length();
  *host = uri.substr(pos, pos1 - pos);
  *path = "/" + uri.substr(pos1 + 1);
}

// Callback class for AsyncClient.
// It is used by Envoy to make remote HTTP call.
class AsyncClientCallbacks : public AsyncClient::Callbacks,
                             public Logger::Loggable<Logger::Id::http> {
 public:
  AsyncClientCallbacks(Upstream::ClusterManager& cm, const std::string& uri,
                       const std::string& cluster, HttpDoneFunc cb)
      : uri_(uri), cb_(cb) {
    std::string host, path;
    ExtractUrlHostPath(uri, &host, &path);

    MessagePtr message(new RequestMessageImpl());
    message->headers().insertMethod().value().setReference(
        Http::Headers::get().MethodValues.Get);
    message->headers().insertPath().value(path);
    message->headers().insertHost().value(host);

    request_ = cm.httpAsyncClientForCluster(cluster).send(
        std::move(message), *this, Optional<std::chrono::milliseconds>());
  }

  // AsyncClient::Callbacks
  void onSuccess(MessagePtr&& response) {
    std::string status = response->headers().Status()->value().c_str();
    if (status == "200") {
      ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: success",
                uri_);
      std::string body;
      if (response->body()) {
        auto len = response->body()->length();
        body = std::string(static_cast<char*>(response->body()->linearize(len)),
                           len);
      } else {
        ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: body is empty",
                  uri_);
      }
      cb_(true, body);
    } else {
      ENVOY_LOG(debug,
                "AsyncClientCallbacks [uri = {}]: response status code {}",
                uri_, status);
      cb_(false, "");
    }
    delete this;
  }
  
  void onFailure(AsyncClient::FailureReason) {
    ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: failed",
              uri_);
    cb_(false, "");
    delete this;
  }

  void Cancel() {
    request_->cancel();
    delete this;
  }

 private:
  const std::string& uri_;
  HttpDoneFunc cb_;
  AsyncClient::Request* request_;
};

// This is per-request auth object.
class AuthRequest : public Logger::Loggable<Logger::Id::http>,
                    public std::enable_shared_from_this<AuthRequest> {
 public:
  AuthRequest(JwtAuthControl* auth_control, HeaderMap& headers,
              DoneFunc on_done)
      : auth_control_(auth_control), headers_(headers), on_done_(on_done) {}

  // Verify a JWT token.
  CancelFunc Verify() {
    const HeaderEntry* entry = headers_.get(kAuthorizationHeaderKey);
    if (!entry) {
      return DoneWithStatus(Status::OK);
    }

    // Extract token from header.
    const HeaderString& value = entry->value();
    if (value.length() <= kBearerPrefix.length() ||
        value.compare(0, kBearerPrefix.length(), kBearerPrefix) != 0) {
      return DoneWithStatus(Status::BEARER_PREFIX_MISMATCH);
    }

    // Parse JWT token
    jwt_.reset(new Auth::Jwt(value.c_str() + kBearerPrefix.length()));
    if (jwt_->GetStatus() != Status::OK) {
      return DoneWithStatus(jwt_->GetStatus());
    }

    // Check "exp" claim.
    auto unix_timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (jwt_->Exp() < unix_timestamp) {
      return DoneWithStatus(Status::JWT_EXPIRED);
    }

    // Check the issuer is configured or not.
    auto issuer = auth_control_->LookupIssuer(jwt_->iss());
    if (!issuer) {
      return DoneWithStatus(Status::JWT_UNKNOWN_ISSUER);
    }

    // Check if audience is allowed
    if (!issuer->config.IsAudienceAllowed(jwt_->Aud())) {
      return DoneWithStatus(Status::AUDIENCE_NOT_ALLOWED);
    }
    
    if (issuer->pkey && !issuer->Expired()) {
      return Verify(*issuer->pkey);
    }

    auth pThis = GetPtr();
    return auth_control_->SendHttpRequest(
        issuer->config.uri, issuer->config.cluser, [pThis](bool ok, const std::string& body) {
          pThis->OnFetchPubkeyDone(ok, body);
        });
  }

 private:
  // Get the shared_ptr from this object.
  std::shared_ptr<AuthRequest> GetPtr() { return shared_from_this(); }

  // Handle the verification done.
  CancelFunc DoneWithStatus(Status status) {
    on_done_(status);
    return nullptr;
  }

  // Verify with a specific public key.
  CancelFunc Verify(const Auth::Pubkeys& pkey) {
    Auth::Verifier v;
    if (!v.Verify(*jwt_, pkey)) {
      return DoneWithStatus(v.GetStatus());
    }

    headers_.addReferenceKey(kAuthorizedHeaderKey,
                             jwt_->PayloadStrBase64Url());

    // Remove JWT from headers.
    headers_.remove(kAuthorizationHeaderKey);
    return DoneWithStatus(Status::OK);
  }

  // Handle the public key fetch done event.
  CancelFunc OnFetchPubkeyDone(bool ok, const std::string& pkey) {
    if (!ok) {
      return DoneWithStatus(Status::FAILED_FETCH_PUBKEY);
    }

    auto issuer = auth_control_->LookupIssuer(jwt_->iss());
    issuer->pkey = Pubkeys::CreateFrom(pkey, issuer->config.pkey_type);
    if (issuer->pKey.GetStatus() != Status::OK) {
      auto status = issuer->pKey.GetStatus();
      issuer->pkey.reset();
      return DoneWithStatus(status);
    }
    
    if (issuer->config.pubkey_cache_expiration_sec > 0) {
      issuer->expiration_time =
	std::chrono::system_clock::now() + std::chrono::seconds(issuer->config.pubkey_cache_expiration_sec);
    }
    return Verify(*issuer->pkey);
  }

  // The parent control object with public key cache.
  JwtAuthControl* auth_control_;
  // The headers
  HeaderMap& headers_;
  // The on_done function.
  DoneFunc on_done_;
  // The JWT object.
  std::unique_ptr<Auth::Jwt> jwt_;
};

}  // namespace

JwtAuthControl::JwtAuthControl(const JwtAuthConfig& config,
                               Server::Configuration::FactoryContext& context)
  : cm_(context.context.clusterManager()) {
  for (const auto& issuer : config.issuers()) {
    auto item = issuer_maps_[issuer.name];
    item.config = issuer;
    if (!issuer.pkey_value.empty()) {
      item.pkey = Pubkeys::CreateFrom(issuer.pkey_value, issuer.pkey_type);
    }
  }
}

CancelFunc JwtAuthControl::Verify(HeaderMap& headers, DoneFunc on_done) {
  auto request = std::make_shared<AuthRequest>(this, headers, on_done);
  return request->Verify();
}

Cancel JwtAuthControl::SendHttpRequest(const std::string& uri,
				       const std::string& cluser,
				       HttpDoneFunc http_done) {
  auto transport = new AsyncClientCallbacks(cm_, uri, cluster, http_done);
  return [transport]() { transport->Cancel(); };
}

IssuerItem* JwtAuthControl::LookupIssuer(const std::string& name) {
  auto it = issuer_maps_.find(name);
  if (it == issuer_maps_.end()) {
    return nullptr;
  }
  return &it->second;
}

JwtAuthControlFactory::JwtAuthControlFactory(std::unique_ptr<JwtAuthConfig> config,
					     Server::Configuration::FactoryContext& context) :
  config_(std::move(config)), tls_(context.threadLocal().allocateSlot()) {
  const JwtAuthConfig& auth_config = *config_;
  tls_->set([&auth_config, &context](Event::Dispatcher& dispatcher)
	    -> ThreadLocal::ThreadLocalObjectSharedPtr {
	      return ThreadLocal::ThreadLocalObjectSharedPtr(
							     new JwtAuthControl(auth_config, context));
	    });
}
  
}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
