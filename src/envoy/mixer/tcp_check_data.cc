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

#include "src/envoy/mixer/tcp_check_data.h"
#include "src/envoy/mixer/utils.h"

namespace Envoy {
namespace Http {
namespace Mixer {

bool TcpCheckData::GetSourceIpPort(std::string* str_ip, int* port) override {
  if (connection_) {
    const Network::Address::Ip* ip = connection->remoteAddress().ip();
    if (ip) {
      *port = ip->port();
      if (ip->ipv4()) {
        uint32_t ipv4 = ip->ipv4()->address();
        *str_ip =
            std::string(reinterpret_cast<const char*>(&ipv4), sizeof(ipv4));
        return true;
      }
      if (ip->ipv6()) {
        std::array<uint8_t, 16> ipv6 = ip.ipv6()->address();
        *str_ip = std::string(reinterpret_cast<const char*>(ipv6.data()), 16);
        return true;
      }
    }
  }
  return false;
}

bool TcpCheckData::GetSourceUser(std::string* user) const override {
  Ssl::Connection* ssl = const_cast<Ssl::Connection*>(connection_->ssl());
  if (ssl != nullptr) {
    *user = ssl->uriSanPeerCertificate();
    return true;
  }
  return false;
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
