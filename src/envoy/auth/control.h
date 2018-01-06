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
#include "common/http/message_impl.h"
#include "envoy/http/async_client.h"
#include "envoy/json/json_object.h"
#include "envoy/json/json_object.h"
#include "envoy/upstream/cluster_manager.h"
#include "server/config/network/http_connection_manager.h"

#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

// Auth control object to handle the token verification flow. It has:
// * token cache and public key cache
// * Know how to make remote call to fetch public keys
// * will be created per-thread vs AuthConfig is globally per-process.
class JwtAuthControl : public Logger::Loggable<Logger::Id::http> {
 public:
  // Load the config from envoy config.
  // It will abort when "issuers" is missing or bad-formatted.
  JwtAuthControl(const JwtAuthConfig &config,
                Server::Configuration::FactoryContext &context);

  using DoneFunc = std::function<void(const Status& status)>;
  void Verify(HeaderMap& headers, DoneFunc func);

 private:
  // Need to make async client call.
  Upstream::ClusterManager &cm_;
  const JwtAuthConfig &config_;

  struct IssuerData {
    IssuerInfo  info;
    std::unique_ptr<Pubkeys> pkey;
    std::chrono::system_clock::time_point create_time;
  }

  std::unordered_map<std::string, IssuerData> issuer_map_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_CONTROL_H
