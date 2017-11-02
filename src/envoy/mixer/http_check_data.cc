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

#include "src/envoy/mixer/http_check_data.h"
#include "src/envoy/mixer/utils.h"

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

const LowerCaseString kIstioAttributeHeader("x-istio-attributes");

}  // namespace

bool HttpCheckData::ExtractIstioAttributes(std::string* data) override {
  // Extract attributes from x-istio-attributes header
  const HeaderEntry* entry = headers_.get(Utils::kIstioAttributeHeader);
  if (entry) {
    *data =
        Base64::decode(strstr(entry->value().c_str(), entry->value().size()));
    headers_.remove(Utils::kIstioAttributeHeader);
    return true;
  }
  return false;
}

void HttpCheckData::AddIstioAttributes(
    const std::string& serialized_str) override {
  std::string base64 =
      Base64::encode(serialized_str.c_str(), serialized_str.size());
  ENVOY_LOG(debug, "Mixer forward attributes set: {}", base64);
  headers.addReferenceKey(Utils::kIstioAttributeHeader, base64);
}

bool HttpCheckData::GetSourceIpPort(std::string* str_ip, int* port) override {
  if (connection_) {
    return Utils::GetIpPort(connection->remoteAddress().ip(), str_ip, port);
  }
  return false;
}

bool HttpCheckData::GetSourceUser(std::string* user) const override {
  Ssl::Connection* ssl = const_cast<Ssl::Connection*>(connection_->ssl());
  if (ssl != nullptr) {
    *user = ssl->uriSanPeerCertificate();
    return true;
  }
  return false;
}

std::map<std::string, std::string> GetRequestHeaders() const override {
  return Utils::ExtractHeaders(header_map_);
}

bool HttpCheckData::FindRequestHeader(
    ::istio::mixer_control::HttpCheckData::HeaderType header_type,
    std::string* value) const override {
  switch
    header_type {
      case HEADER_PATH:
        if (headers_.Path()) {
          *value = std::string(headers_.Path()->value().c_str(),
                               headers_.Path()->value().size());
          return true;
        }
        break;
      case HEADER_HOST:
        if (headers_.Host()) {
          *value = std::string(headers_.Host()->value().c_str(),
                               headers_.Host()->value().c_str());
          return true;
        }
        break;
      case HEADER_SCHEME:
        if (headers_.Scheme()) {
          *value = std::string(headers_.Scheme()->value().c_str(),
                               headers_.Scheme()->value().c_str());
          return true;
        }
        break : case HEADER_USER_AGENT : if (headers_.UserAgent()) {
          *value = std::string(headers_.UserAgent()->value().c_str(),
                               headers_.UserAgent()->value().c_str());
          return true;
        }
        break : case HEADER_METHOD : if (headers_.Method()) {
          *value = std::string(headers_.Method()->value().c_str(),
                               headers_.Method()->value().c_str());
          return true;
        }
        break : case HEADER_REFERER : {
          const HeaderEntry* referer = headers_.get(kRefererHeaderKey);
          if (referer) {
            *data =
                std::string(referer->value().c_str(), referer->value().size());
            return true;
          }
        }
        break:
    }
  return false;
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
