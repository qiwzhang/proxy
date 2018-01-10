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

#include "envoy/upstream/cluster_manager.h"

#include "rapidjson/document.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {
  namespace {
    const LowerCaseString kAuthorizationHeaderKey("Authorization");

    const LowerCaseString kAuthorizedHeaderKey("sec-istio-auth-userinfo");
    
    const std::string kBearerPrefix = "Bearer ";

    void ExtractUriHostPath(const std::string &uri, std::string* host, std::string* path) {
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
    class AsyncClientCallbacks : public AsyncClient::Callbacks,
				 public Logger::Loggable<Logger::Id::http> {
    public:
      AsyncClientCallbacks(Upstream::ClusterManager &cm, const std::string &uri, const std::string &cluster, HttpDoneFunc cb)
	: cm_(cm), uri_(uri), cluster_(cluster), cb_(cb) {
	std::string host, path;
	ExtractUrlHostPath(uri, &host, &path);
	
	MessagePtr message(new RequestMessageImpl());
	message->headers().insertMethod().value().setReference(
							       Http::Headers::get().MethodValues.Get);
	message->headers().insertPath().value(path);
	message->headers().insertHost().value(host);

	request_ = cm_.httpAsyncClientForCluster(cluster_)
	  .send(std::move(message), *this, Optional<std::chrono::milliseconds>());
      }
      // AsyncClient::Callbacks
      void onSuccess(MessagePtr &&response) {
	std::string status = response->headers().Status()->value().c_str();
	if (status == "200") {
	  ENVOY_LOG(debug, "AsyncClientCallbacks [cluster = {}]: success",
		    cluster_);
	  std::string body;
	  if (response->body()) {
	    auto len = response->body()->length();
	    body = std::string(static_cast<char *>(response->body()->linearize(len)),
			       len);
	  } else {
	    ENVOY_LOG(debug, "AsyncClientCallbacks [cluster = {}]: body is null",
		      cluster_);
	  }
	  cb_(true, body);
	} else {
	  ENVOY_LOG(debug,
		    "AsyncClientCallbacks [cluster = {}]: response status code {}",
		    cluster_, status);
	  cb_(false, "");
	}
	delete this;
      }
      void onFailure(AsyncClient::FailureReason) {
	ENVOY_LOG(debug, "AsyncClientCallbacks [cluster = {}]: failed",
		  cluster_->name());
        cb_(false, "");
	delete this;
      }

      void Cancel() {
	request_->cancel();
	delete this;
      }

    private:
      Upstream::ClusterManager &cm_;
      const std::string& uri_;
      const std::string& cluster_;
      HttpDoneFunc cb_;
      AsyncClient::Request *request_;
    };
  }  // namespace


  // This is per-request auth object.
  class AuthRequest : public Logger::Loggable<Logger::Id::http> {
  public:
    AuthRequest(JwtAuthControl* auth_control,
		HeaderMap& headers,
		DoneFunc on_done) :
      auth_control_(auth_control), headers_(headers), on_done_(on_done) {}

    CancelFunc Verify() {
    }
    
  private:
    JwtAuthControl* auth_control_;
    HeaderMap& headers_;
    DoneFunc on_done_:
    std::unique_ptr<Auth::Jwt> jwt_;
  };

  JwtAuthControl::JwtAuthControl(const JwtAuthConfig &config,
				 Server::Configuration::FactoryContext &context)
    : config_(config), cm_(context.context.clusterManager()) {

    for (const auto& issuer : config.issuers()) {
      auto data = issuer_maps_[issuer.name];
      
      data.info = issuer;
      if (!issuer.pkey_value.empty()) {
	data.pkey = Pubkeys::CreateFrom(issuer.pkey_value, issuer.pkey_type);
      }
    }
  }

  CancelFunc JwtAuthControl::Verify(HeaderMap& headers, DoneFunc on_done) {
    auto request = std::make_shared<AuthRequest>(this, headers, on_done);
    return request->Verify();
  }

  Cancel JwtAuthControl::HttpRequest(const std::string& uri, const std::string& cluser, HttpDoneFunc http_done) {
    auto transport = new AsyncClientCallbacks(cm_, uri, cluster, http_done);
    return [transport]() { transport->Cancel(); };
  }


  IssuerData* JwtAuthControl::GetIssuer(const std::string& name) {
    auto it = issuer_maps_.find(name);
    if (it == issuer_maps_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  
  CancelFunc AuthRequest::DoneWithStatus(Status status) {
    on_done_(status);
    return nullptr;
  }

  CancelFunc AuthRequest::Verify() {
    const HeaderEntry* entry = headers_.get(kAuthorizationHeaderKey);
    if (!entry) {
      return DoneWithStatus(Status::OK);
    }

    const HeaderString& value = entry->value();
    if (value.length() <= kBearerPrefix.length() ||
	value.compare(0, kBearerPrefix.length(), kBearerPrefix) != 0) {
      return DoneWithStatus(Status::BEARER_PREFIX_MISMATCH);
    }
    
    jwt_.reset(new Auth::Jwt(value.c_str() + kBearerPrefix.length()));
    if (jwt_->GetStatus() != Status::OK) {
      return DoneWithStatus(jwt_->GetStatus());
    }

    // Check "exp" claim.
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
std::chrono::system_clock::now().time_since_epoch())
      .count();
    if (jwt_->Exp() < unix_timestamp) {
      return DoneWithStatus(Status::JWT_EXPIRED);
    }

    auto issuer = auth_control_->GetIssuer(jwt_->iss());
    if (!issuer) {
      return DoneWithStatus(Status::NOT_ISSUER_CONFIGED);
    }
    
    if (issuer->pkey && !issuer->pKeyCacheExpired()) {
      return Verify(*issuer->pkey);
    }

    auth pThis = GetPtr();
    return auth_control_->SendHttpRequest(issuer->uri, issuer->cluser,
			   [pThis](bool ok, const std::string& body) {
			    pThis->OnFetchPubkeyDone(ok, body);
      });
  }

  CancelFunc AuthRequest::Verify(const Auth::Pubkeys& pkey) {
      Auth::Verifier v;
      if (!v.Verify(*jwt_, pkey)) {
	return DoneWithStatus(v.GetStatus());
      }

      headers_.addReferenceKey(AuthorizedHeaderKey(), jwt_->PayloadStrBase64Url());

      // Remove JWT from headers.
      headers_.remove(kAuthorizationHeaderKey);
      return DoneWithStatus(Status::OK);
  }

  
  CancelFunc AuthRequest::OnFetchPubkeyDone(bool ok, const std::string& pkey) {
    if (!ok) {
      return DoneWithStatus(Status::FAILED_FETCH_PUBKEY);
    }

    auto issuer = auth_control_->GetIssuer(jwt_->iss());;
    issuer->pkey = Pubkeys::CreateFrom(pkey, issuer->info.pkey_type);
    issuer->create_time = std::chrono::system_clock::now();
    return Verify(*issuer->pkey);
  }
  

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
