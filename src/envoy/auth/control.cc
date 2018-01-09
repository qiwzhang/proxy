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
  }  // namespace


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

  void JwtAuthControl::Verify(HeaderMap& headers, DoneFunc on_done) {
    const HeaderEntry* entry = headers.get(kAuthorizationHeaderKey);
    if (!entry) {
      on_done(Status::OK);
      return;
    }

    const HeaderString& value = entry->value();
    if (value.length() <= kBearerPrefix.length() ||
	value.compare(0, kBearerPrefix.length(), kBearerPrefix) != 0) {
      on_done(Status::BEARER_PREFIX_MISMATCH);
      return;
    }
    
    Auth::Jwt jwt(value.c_str() + kBearerPrefix.length());
    if (jwt.GetStatus() != Status::OK) {
      on_done(Status::BEARER_PREFIX_MISMATCH);
      return;
    }

    // Check "exp" claim.
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
std::chrono::system_clock::now().time_since_epoch())
      .count();
    if (jwt.Exp() < unix_timestamp) {
      on_done(Status::JWT_EXPIRED);
      return;
    }

    
    auto it = issuer_maps_.find(jwt.iss());
    if (it == issuer_maps_.end()) {
      on_done(Status::NOT_ISSUER_CONFIGED);
      return;
    }
    auto& issuer = it->second;
    if (issuer.pkey && !issuer.pKeyCacheExpired()) {
      Auth::Verifier v;
      if (v.Verify(jwt, *issuer->pkey)) {
	headers.addReferenceKey(AuthorizedHeaderKey(), jwt.PayloadStrBase64Url());

	// Remove JWT from headers.
	headers.remove(kAuthorizationHeaderKey);
	on_done(Status::OK);
	return;
      } else {
	one_done(v.GetStatus());
	return;
      }
	
    }

    const std::string& issuer_name = issuer.name;
    FetchPubkey(issuer.uri, issuer.cluser, [issuer_name](bool ok, const std::string& body) {
	OnFetchPubkeyDone(issuer_name, ok, body);
      });
  }

  void JwtAuthControl::HttpRequest(issuer.uri, issuer.cluser, [issuer_name](bool ok, const std::string& body) {
  

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
