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

#include "config.h"

#include <chrono>

namespace Envoy {
namespace Http {
namespace Auth {
namespace {
// Default public cache cache duration: 5 minutes.
const int64_t kPubKeyCacheExpirationSec = 600;
}  // namespace

std::string IssuerInfo::Validate() const {
  if (!pkey_value.empty()) {
    auto pkey = Pubkeys::CreateFrom(pkey_value, pkey_type);
    if (pkey->GetStatus() != Status::OK) {
      return std::string("Public key invalid value: ") + pkey_value;
    }
  } else {
    if (uri == "") {
      return "Public key missing uri";
    }
    if (cluster == "") {
      return "Public key missing cluster";
    }
  }
  return "";
}

JwtAuthConfig::JwtAuthConfig(std::vector<IssuerInfo>&& issuers)
    : issuers_(std::move(issuers)) {
  ENVOY_LOG(debug, "JwtAuthConfig: {}", __func__);
}

JwtAuthConfig::JwtAuthConfig(const Json::Object& config) {
  ENVOY_LOG(debug, "JwtAuthConfig: {}", __func__);

  // Load the issuers
  std::vector<Json::ObjectSharedPtr> issuer_jsons;
  try {
    issuer_jsons = config.getObjectArray("issuers");
  } catch (...) {
    ENVOY_LOG(error, "JwtAuthConfig: issuers should be array type");
    return;
  }

  for (auto issuer_json : issuer_jsons) {
    IssuerInfo issuer;
    if (LoadIssuerInfo(*issuer_json, &issuer)) {
      std::string err = issuer.Validate();
      if (err.empty()) {
        issuers_.push_back(issuer);
      } else {
        ENVOY_LOG(error, "JwtAuthConfig: invalid issuer config for {}: {}",
                  issuer.name, err);
      }
    }
  }
}

bool JwtAuthConfig::LoadIssuerInfo(const Json::Object& json,
                                   IssuerInfo* issuer) {
  // Check "name"
  issuer->name = json.getString("name", "");
  if (issuer->name == "") {
    ENVOY_LOG(error, "IssuerInfo: Issuer name missing");
    return false;
  }
  // Check "audience". It will be an empty array if the key "audience" does not
  // exist
  try {
    auto audiences = json.getStringArray("audiences", true);
    issuer->audiences.insert(audiences.begin(), audiences.end());
  } catch (...) {
    ENVOY_LOG(error, "IssuerInfo [name = {}]: Bad audiences", issuer->name);
    return false;
  }
  // Check "pubkey"
  Json::ObjectSharedPtr json_pubkey;
  try {
    json_pubkey = json.getObject("pubkey");
  } catch (...) {
    ENVOY_LOG(error, "IssuerInfo [name = {}]: Public key missing",
              issuer->name);
    return false;
  }
  // Check "type"
  std::string pkey_type_str = json_pubkey->getString("type", "");
  if (pkey_type_str == "pem") {
    issuer->pkey_type = Pubkeys::PEM;
  } else if (pkey_type_str == "jwks") {
    issuer->pkey_type = Pubkeys::JWKS;
  } else {
    ENVOY_LOG(error,
              "IssuerInfo [name = {}]: Public key type missing or invalid",
              issuer->name);
    return false;
  }
  // Check "value"
  issuer->pkey_value = json_pubkey->getString("value", "");
  // If pkey_value is not empty, not need for "uri" and "cluster"
  if (issuer->pkey_value != "") {
    return true;
  }

  // Check "uri" and "cluster"
  issuer->uri = json_pubkey->getString("uri", "");
  issuer->cluster = json_pubkey->getString("cluster", "");

  // For fetched public key.
  issuer->pubkey_cache_expiration_sec = json_pubkey->getInteger(
      "pubkey_cache_expiration_sec", kPubKeyCacheExpirationSec);
  return true;
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
