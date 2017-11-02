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

namespace Envoy {
namespace Http {
namespace Mixer {

TcpReportData::TcpReportData(
    uint64_t received_bytes, uint64_t send_bytes,
    std::chrono::nanoseconds duration,
    Upstream::HostDescriptionConstSharedPtr upstreamHost)
    : received_bytes_(received_bytes),
      send_bytes_(send_bytes),
      duration_(duration),
      upstreamHost_(upstreamHost) {}

bool TcpReportData::GetDestinationIpPort(std::string* str_ip,
                                         int* port) const override {
  if (upstreamHost && upstreamHost->address()) {
    return Utils::GetIpPort(upstreamHost->address()->ip(), str_ip, port);
  }
  return false;
}

void TcpReportData::GetReportInfo(
    ::istio::mixer_control::TcpReportData::ReportInfo* data) const override {
  data->received_bytes = received_bytes_;
  data->send_bytes = send_bytes_;
  data->duration = duration_;
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
