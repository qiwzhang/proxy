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

#include "src/envoy/mixer/envoy_http_check_data.h"

namespace Envoy {
namespace Http {
namespace Mixer {

  std::map<std::string, std::string> EnvoyHttpReportData::GetResponseHeaders() const override {
    return Utils::ExtractHeaders(header_map_);
  }
  
  void EnvoyHttpReportData::GetInfo(RequestInfo* data) const override {
    data->received_bytes = info_.bytesReceived();
    data->send_bytes = info_.bytesSent();
    data->duration = 
      std::chrono::duration_cast<std::chrono::nanoseconds>(info_.duration());

    if (info.responseCode().valid()) {
      data->response_code = info.responseCode().value();
    }
  }
  
  
}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
