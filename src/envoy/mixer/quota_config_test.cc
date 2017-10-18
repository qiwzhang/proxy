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

#include "quota_config.h"

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

using ::google::protobuf::TextFormat;
using ::istio::mixer::v1::config::QuotaSpec;
using ::istio::mixer_client::Attributes;

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

const char kQuotaNotMatch[] = R"(
rules {
  quotas {
    quota: "quota1"
    charge: 1
  }
  quotas {
    quota: "quota2"
    charge: 2
  }
}
)";

const char kQuotaMatch[] = R"(
rules {
  match {
    clause {
      key: "request.http_method"
      value {
        exact: "GET"
      }
    }
    clause {
      key: "request.path"
      value {
        prefix: "/books"
      }
    }
  }
  match {
    clause {
      key: "api.operation"
      value {
        exact: "get_books"
      }
    }
  }
  quotas {
    quota: "quota-name"
    charge: 1
  }
}
)";

using QuotaVector = std::vector<QuotaConfig::Quota>;

class QuotaConfigTest : public ::testing::Test {
 public:
  void SetUp() {}
};

TEST_F(QuotaConfigTest, TestNotMatch) {
  QuotaSpec quota_spec;
  ASSERT_TRUE(TextFormat::ParseFromString(kQuotaNotMatch, &quota_spec));
  QuotaConfig config(quota_spec);

  Attributes attributes;
  ASSERT_EQ(config.Check(attributes),
            QuotaVector({{"quota1", 1}, {"quota2", 2}}));
}

TEST_F(QuotaConfigTest, TestMatch) {
  QuotaSpec quota_spec;
  ASSERT_TRUE(TextFormat::ParseFromString(kQuotaMatch, &quota_spec));
  QuotaConfig config(quota_spec);

  Attributes attributes;
  ASSERT_EQ(config.Check(attributes), QuotaVector());

  // Wrong http_method
  attributes.attributes["request.http_method"] =
      Attributes::StringValue("POST");
  attributes.attributes["request.path"] = Attributes::StringValue("/books/1");
  ASSERT_EQ(config.Check(attributes), QuotaVector());

  // Matched
  attributes.attributes["request.http_method"] = Attributes::StringValue("GET");
  ASSERT_EQ(config.Check(attributes), QuotaVector({{"quota-name", 1}}));

  attributes.attributes.clear();
  // Wrong api.operation
  attributes.attributes["api.operation"] =
      Attributes::StringValue("get_shelves");
  ASSERT_EQ(config.Check(attributes), QuotaVector());

  // Matched
  attributes.attributes["api.operation"] = Attributes::StringValue("get_books");
  ASSERT_EQ(config.Check(attributes), QuotaVector({{"quota-name", 1}}));
}

}  // namespace
}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
