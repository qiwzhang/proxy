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

#pragma once


#include "common/http/headers.h"
#include "control/include/tcp_check_data.h"

namespace Envoy {
namespace Http {
namespace Mixer {

  class EnvoyTcpCheckData : public TcpCheckData {
public:
  EnvoyTcpCheckData(HeaderMap& headers, const Network::Connection* connection) : headers_(headers), connection_(connection) {}
    
  bool GetSourceIpPort(std::string* ip, int* port) const override;

  bool GetSourceUser(std::string* user) const override;
  
private:
  const Network::Connection* connection_;
};
    


}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
