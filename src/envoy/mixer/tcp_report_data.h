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
#include "control/include/tcp_report_data.h"
#include "envoy/upstream/cluster_manager.h"

namespace Envoy {
namespace Http {
namespace Mixer {

class TcpReportData : public ::istio::mixer_control::TcpReportData {
 public:
  TcpReportData(uint64_t received_bytes, uint64_t send_bytes,
                std::chrono::nanoseconds duration,
                Upstream::HostDescriptionConstSharedPtr upstream_host);

  bool GetDestinationIpPort(std::string* ip, int* port) const override;
  void GetReportInfo(
      ::istio::mixer_control::TcpReportData::ReportInfo* data) const override;

 private:
  uint64_t received_bytes_;
  uint64_t send_bytes_;
  std::chrono::nanoseconds duration_;
  Upstream::HostDescriptionConstSharedPtr upstream_host_;
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
