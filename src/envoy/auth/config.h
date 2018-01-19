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

#ifndef AUTH_CONFIG_H
#define AUTH_CONFIG_H

#include "jwt.h"
#include "common/common/logger.h"
#include "envoy/json/json_object.h"

#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

// Struct to hold an issuer's config.
struct IssuerInfo {
  std::string uri;      // URI for public key
  std::string cluster;  // Envoy cluster name for public key

  std::string name;         // e.g. "https://accounts.example.com"
  Pubkeys::Type pubkey_type;  // Format of public key.

  // public key value.
  std::string pubkey_value;

  // Time to expire a cached public key (sec).
  // 0 means never expired.
  int64_t pubkey_cache_expiration_sec{};

  // specified audiences from config.
  std::set<std::string> audiences;

  // Check if an audience is allowed.
  // If audiences is an empty array or not specified, any "aud" claim will be
  // accepted.
  bool IsAudienceAllowed(const std::string &aud) const {
    return audiences.empty() || audiences.find(aud) != audiences.end();
  }

  // Validate the issuer config.
  // Return error message if invalid, otherwise return empty string.
  std::string Validate() const;
};

// A config for Jwt auth filter
class Config : public Logger::Loggable<Logger::Id::http> {
 public:
  // Load the config from envoy config.
  // It will abort when "issuers" is missing or bad-formatted.
  Config(const Json::Object &config);

  // Constructed by IssuerInfo directly.
  Config(std::vector<IssuerInfo> &&issuers);

  // Get the list of issuers.
  const std::vector<IssuerInfo>& issuers() const { return issuers_; }

 private:
  // Load one issuer config from JSON object.
  bool LoadIssuerInfo(const Json::Object &json, IssuerInfo *issuer);

  // Each element corresponds to an issuer
  std::vector<IssuerInfo> issuers_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_CONFIG_H
