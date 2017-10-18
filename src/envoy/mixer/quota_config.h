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

#include <string>
#include <vector>

#include "include/attribute.h"
#include "proxy/v1/config/quota.pb.h"

namespace Envoy {
namespace Http {
namespace Mixer {

// An object uses quota config to check a request
// attributes to generate required quotas.
class QuotaConfig {
 public:
  QuotaConfig(const ::istio::proxy::v1::config::QuotaConfig& config_pb);

  struct Quota {
    std::string quota;
    uint64_t amount;
  };
  // Generate required quotas for a request attributes.
  std::vector<Quota> Check(
      const ::istio::mixer_client::Attributes& attributes) const;

 private:
  // The quota config proto
  const ::istio::proxy::v1::config::QuotaConfig& config_pb_;
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
