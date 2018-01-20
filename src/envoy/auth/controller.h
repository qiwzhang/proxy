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

#ifndef AUTH_CONTROLLER_H
#define AUTH_CONTROLLER_H

#include "config.h"
#include "http_request.h"
#include "pubkey_cache.h"

#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"

namespace Envoy {
namespace Http {
namespace Auth {

// Auth control object to handle the token verification flow.
class Controller : public ThreadLocal::ThreadLocalObject {
 public:
  // Load the config from envoy config.
  Controller(const Config& config, HttpGetFunc http_get_func);

  // The callback function after JWT verification is done.
  using DoneFunc = std::function<void(const Status& status)>;

  // Verify JWT. on_done function will be called after verification is done.
  // If there is pending remote call, a CancelFunc will be returned
  // It can be used to cancel the remote call. When remote call is canceled
  // on_done function will not be called.
  CancelFunc Verify(HeaderMap& headers, DoneFunc on_done);

  // The authorized header key.
  static const LowerCaseString& AuthorizedHeaderKey();

 private:
  // The transport function to make remote http get call.
  HttpGetFunc http_get_func_;

  // The public key cache, indexed by issuer.
  PubkeyCache pubkey_cache_;
};

// The factory object to create per-thread auth control.
class ControllerFactory {
 public:
  ControllerFactory(std::unique_ptr<Config> config,
                    Server::Configuration::FactoryContext& context);

  // Get per-thread auth_control.
  Controller& controller() { return tls_->getTyped<Controller>(); }

 private:
  // The auth config.
  std::unique_ptr<Config> config_;
  // Thread local slot to store per-thread auth_control
  ThreadLocal::SlotPtr tls_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_CONTROLLER_H