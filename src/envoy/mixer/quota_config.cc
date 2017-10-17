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

#include "src/envoy/mixer/quota_config.h"

using ::istio::mixer_client::Attributes;
using ::istio::mixer::v1::AttrbiuteMatch;

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

bool Match(const AttributeMatch& match, const Attributes& attributes) {
  for (const auto& map_it : match.clause()) {
    // map is attribute_name to StringMatch.
    const std::string& attr_name = map_it.first;
    const auto& match = map_it.second;

    // Check if required attribure exists with string type.
    const auto& attr_it = attributes.attributes.find(attr_name);
    if (attr_it == attributes.attributes.end() ||
        attr_it->second.type != Attributes::Value::STRING) {
      return false;
    }
    const std::string& attr_value = attr_it->second.str_v;

    switch (match.match_type_case()) {
      case ::istio::proxy::v1::config::StringMatch::kExact:
        if (attr_value != match.exact()) {
          return false;
        }
        break;
      case ::istio::proxy::v1::config::StringMatch::kPrefix:
        if (attr_value.length() < match.prefix().length() ||
            attr_value.compare(0, match.prefix().length(), match.prefix()) != 0) {
          return false;
        }
        break;
      case ::istio::proxy::v1::config::StringMatch::kRegex:
        // TODO: support regex
        return false;
        break;
      default:
        break;
    }
  }
  return true;
}

}  // namespace

QuotaConfig::QuotaConfig(const ::istio::proxy::v1::config::QuotaConfig& config_pb)
    : config_pb_(config_pb) {}

std::vector<QuotaConfig::Quota> QuotaConfig::Check(
    const Attributes& attributes) const {
  std::vector<Quota> results;
  for (const auto& rule : config_pb_.rules()) {
    for (const auto& match : rule.match()) {
      if (Match(match, attributes)) {
	for (const auto& quota : rule.quotas()) {
	  results.push_back({quota.quota(), quota.charge()});
	}
        break;
      }
    }
  }
  return results;
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
