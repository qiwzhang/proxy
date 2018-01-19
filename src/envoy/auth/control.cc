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

// This is per-request auth object.
class AuthRequest : public Logger::Loggable<Logger::Id::http>,
                    public std::enable_shared_from_this<AuthRequest> {
 public:
  AuthRequest(HttpGetFunc http_get_func, PubkeyCache& pubkey_cache,
              HeaderMap& headers, DoneFunc on_done)
      : http_get_func_(http_get_func),
        pubkey_cache_(pubkey_cache),
        headers_(headers),
        on_done_(on_done) {}

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
    auto issuer = pubkey_cache_->LookupByIssuer(jwt_->iss());
    if (!issuer) {
      return DoneWithStatus(Status::JWT_UNKNOWN_ISSUER);
    }

    // Check if audience is allowed
    if (!issuer->config().IsAudienceAllowed(jwt_->Aud())) {
      return DoneWithStatus(Status::AUDIENCE_NOT_ALLOWED);
    }

    if (issuer->pkey() && !issuer->Expired()) {
      return Verify(*issuer->pkey());
    }

    auth pThis = GetPtr();
    return http_get_func_(issuer->config().uri, issuer->config().cluser,
                          [pThis](bool ok, const std::string& body) {
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

    headers_.addReferenceKey(kAuthorizedHeaderKey, jwt_->PayloadStrBase64Url());

    // Remove JWT from headers.
    headers_.remove(kAuthorizationHeaderKey);
    return DoneWithStatus(Status::OK);
  }

  // Handle the public key fetch done event.
  CancelFunc OnFetchPubkeyDone(bool ok, const std::string& pkey) {
    if (!ok) {
      return DoneWithStatus(Status::FAILED_FETCH_PUBKEY);
    }

    auto issuer = pubkey_cache_->LookupByIssuer(jwt_->iss());
    Status status = issuer->SetKey(pkey);
    if (status != Status::OK) {
      return DoneWithStatus(status);
    }

    return Verify(*issuer->pkey());
  }

  // The transport function
  HttpGetFunc& http_get_func_;
  // The pubkey cache object.
  PubkeyCache pubkey_cache_;
  // The headers
  HeaderMap& headers_;
  // The on_done function.
  DoneFunc on_done_;
  // The JWT object.
  std::unique_ptr<Auth::Jwt> jwt_;
};

}  // namespace

JwtAuthControl::JwtAuthControl(const JwtAuthConfig& config,
                               HttpGetFunc http_get_func)
    : http_get_func_(http_get_func), pubkey_cache_(config) {}

CancelFunc JwtAuthControl::Verify(HeaderMap& headers, DoneFunc on_done) {
  auto request = std::make_shared<AuthRequest>(http_get_func_, pubkey_cache_,
                                               headers, on_done);
  return request->Verify();
}

JwtAuthControlFactory::JwtAuthControlFactory(
    std::unique_ptr<JwtAuthConfig> config,
    Server::Configuration::FactoryContext& context)
    : config_(std::move(config)), tls_(context.threadLocal().allocateSlot()) {
  const JwtAuthConfig& auth_config = *config_;
  tls_->set(
      [&auth_config, &context](Event::Dispatcher& dispatcher)
          -> ThreadLocal::ThreadLocalObjectSharedPtr {
            return ThreadLocal::ThreadLocalObjectSharedPtr(new JwtAuthControl(
                auth_config,
                NewHttpRequestByAsyncClient(context.context.clusterManager())));
          });
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
