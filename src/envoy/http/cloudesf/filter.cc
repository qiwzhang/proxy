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

#include "src/envoy/http/cloudesf/filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace CloudESF {

Http::FilterHeadersStatus Filter::decodeHeaders(Http::HeaderMap&, bool) {
  ENVOY_LOG(debug, "Called CloudESF Filter : {}", __func__);

  state_ = Calling;
  stopped_ = false;
  token_fetcher_ = TokenFetcher::create(config_->cm());
  token_fetcher_->fetch(config_->config().token_uri(), *this);

  if (state_ == Complete) {
    return Http::FilterHeadersStatus::Continue;
  }
  ENVOY_LOG(debug, "Called CloudESF filter : Stop");
  stopped_ = true;
  return Http::FilterHeadersStatus::StopIteration;
}

void Filter::onDestroy() {
  if (token_fetcher_) {
    token_fetcher_->cancel();
    token_fetcher_ = nullptr;
  }
}

void Filter::onTokenSuccess(const std::string& token, int expires_in) {
  ENVOY_LOG(debug, "Fetched access_token : {}, expires_in {}", token,
            expires_in);
  token_ = token;
  // This stream has been reset, abort the callback.
  if (state_ == Responded) {
    return;
  }

  state_ = Complete;
  if (stopped_) {
    decoder_callbacks_->continueDecoding();
  }
}

void Filter::onTokenError(TokenFetcher::TokenReceiver::Failure) {
  // This stream has been reset, abort the callback.
  if (state_ == Responded) {
    return;
  }
  state_ = Responded;

  Http::Code code = Http::Code(401);
  decoder_callbacks_->sendLocalReply(code, "Failed to fetch access_token",
                                     nullptr);
  decoder_callbacks_->streamInfo().setResponseFlag(
      StreamInfo::ResponseFlag::UnauthorizedExternalService);
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance&, bool) {
  ENVOY_LOG(debug, "Called CloudESF Filter : {}", __func__);
  if (state_ == Calling) {
    return Http::FilterDataStatus::StopIterationAndWatermark;
  }
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus Filter::decodeTrailers(Http::HeaderMap&) {
  ENVOY_LOG(debug, "Called CloudESF Filter : {}", __func__);
  if (state_ == Calling) {
    return Http::FilterTrailersStatus::StopIteration;
  }
  return Http::FilterTrailersStatus::Continue;
}

void Filter::setDecoderFilterCallbacks(
    Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

void Filter::log(const Http::HeaderMap* /*request_headers*/,
                 const Http::HeaderMap* /*response_headers*/,
                 const Http::HeaderMap* /*response_trailers*/,
                 const StreamInfo::StreamInfo& /*stream_info*/) {
  ENVOY_LOG(debug, "Called CloudESF Filter : {}", __func__);
}

}  // namespace CloudESF
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy