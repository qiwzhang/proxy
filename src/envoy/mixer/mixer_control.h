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

#include "control/include/controller.h"
#include "envoy/upstream/cluster_manager.h"
#include "src/envoy/mixer/config.h"

namespace Envoy {
namespace Http {
namespace Mixer {

class MixerControl final : public ThreadLocal::ThreadLocalObject {
 public:
  // The constructor.
  MixerControl(const MixerConfig& mixer_config, Upstream::ClusterManager& cm,
               Event::Dispatcher& dispatcher, Runtime::RandomGenerator& random);

  Upstream::ClusterManager& cm() { return cm_; }

  ::istio::mixer_control::Controller* controller() { return controller_.get(); }
  const MixerConfig& mixer_config() { return mixer_config_; }

 private:
  // Envoy cluster manager for making gRPC calls.
  Upstream::ClusterManager& cm_;
  // The mixer control
  std::unique_ptr<::istio::mixer_control::Controller> controller_;
  // The mixer config
  const MixerConfig& mixer_config_;
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
