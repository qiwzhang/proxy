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

#ifndef AUTH_CONTROL_H
#define AUTH_CONTROL_H

#include "jwt.h"

#include "common/common/logger.h"
#include "envoy/upstream/cluster_manager.h"

#include <unordered_map>

namespace Envoy {
namespace Http {
namespace Auth {

// The callback function after JWT verification is done.
using DoneFunc = std::function<void(const Status& status)>;
// The callback function after HTTP fetch call is done.
using HttpDoneFunc = std::function<void(bool ok, const std::string& body)>;
// The function to cancel the pending remote call.
using CancelFunc = std::function<void()>;

// A struct to hold issuer cache item.
struct IssuerItem {
    IssuerInfo config;
    std::unique_ptr<Pubkeys> pkey;
    std::chrono::system_clock::time_point expiration_time;

    bool Expired() const {
      return config.pubkey_cache_expiration_sec > 0 &&
	std::chrono::system_clock::now() >= expiration_time;
    }
};
  
// Auth control object to handle the token verification flow. It has:
// * token cache and public key cache
// * Know how to make remote call to fetch public keys
// * should be created per-thread local so not need to protect its data.
//   But AuthConfig is globally per-process.
class JwtAuthControl : public Logger::Loggable<Logger::Id::http> {
 public:
  // Load the config from envoy config.
  // It will abort when "issuers" is missing or bad-formatted.
  JwtAuthControl(const JwtAuthConfig& config,
                 Server::Configuration::FactoryContext& context);

  // Verify JWT. on_done function will be called after verification is done.
  // If there is pending remote call, a CancelFunc will be returned
  // It can be used to cancel the remote call. When remote call is canceled
  // on_done function will not be called.
  CancelFunc Verify(HeaderMap& headers, DoneFunc on_done);

  // Send a HTTP GET request. http_done is called when receives the response.
  CancelFunc SendHttpRequest(const std::string& url, const std::string& cluster,
                             HttpDoneFunc http_done);

  // Lookup issuer cache map.
  IssuerItem* LookupIssuer(const std::string& name);

 private:
  // The object needed to make async client call.
  Upstream::ClusterManager& cm_;

  // The public key cache, indexed by issuer.
  std::unordered_map<std::string, IssuerItem> issuer_map_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_CONTROL_H
