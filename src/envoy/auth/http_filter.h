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

#include "config.h"
#include "control.h"

#include "common/common/logger.h"
#include "server/config/network/http_connection_manager.h"

#include <map>
#include <memory>
#include <string>

namespace Envoy {
namespace Http {

class JwtVerificationFilter : public StreamDecoderFilter,
                              public Logger::Loggable<Logger::Id::http> {
 public:
  JwtVerificationFilter(std::shared_ptr<Auth::JwtAuthControlFactory> control_factory);
  ~JwtVerificationFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

 private:
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  Auth::JwtAuthControl& auth_control_;
  Auth::CancelFunc cancel_check_;

  enum State { Init, Calling, Responded, Complete };
  State state_ = Init;
  bool stopped_ = false;
};

}  // Http
}  // Envoy
