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

#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include "common/common/logger.h"
#include "envoy/json/json_object.h"
#include "jwt.h"

#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

// Struct to hold an issuer's config.
struct IssuerInfo {
  // Default one.
  IssuerInfo() {}
  // Allow default construct.
  IssuerInfo(const std::string &name, const std::string &uri,
             const std::string &cluster, Pubkeys::Type pkey_type,
             std::vector<std::string> &&audiences)
      : uri(uri),
        cluster(cluster),
        name(name),
        pkey_type(pkey_type),
        audiences(std::move(audiences)) {}

  std::string uri;      // URI for public key
  std::string cluster;  // Envoy cluster name for public key

  std::string name;         // e.g. "https://accounts.example.com"
  Pubkeys::Type pkey_type;  // Format of public key.

  // public key value.
  std::string pkey_value;

  // Time to expire a cached public key (sec).
  // 0 means never expired.
  int64_t pubkey_cache_expiration_sec{};

  // Validate the config, if fails, return non-empty error string.
  std::string Validate() const;

  // specified audiences from config.
  std::set<std::string> audiences;

  // Check if an audience is allowed.
  // If audiences is an empty array or not specified, any "aud" claim will be
  // accepted.
  bool IsAudienceAllowed(const std::string &aud) {
    return audiences.empty() || audiences.find(aud) != audiences.end();
  }

  // Validate the issuer config.
  // Return error message if invalid, otherwise return empty string.
  std::string Validate() const;
};

// A config for Jwt auth filter
class JwtAuthConfig : public Logger::Loggable<Logger::Id::http> {
 public:
  // Load the config from envoy config.
  // It will abort when "issuers" is missing or bad-formatted.
  JwtAuthConfig(const Json::Object &config);

  // Constructed by IssuerInfo directly.
  JwtAuthConfig(std::vector<IssuerInfo> &&issuers);

 private:
  // Load one issuer config.
  bool LoadIssuerInfo(const Json::Object &json, IssuerInfo *issuer);

  // Each element corresponds to an issuer
  std::vector<IssuerInfo> issuers_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // PROXY_CONFIG_H
