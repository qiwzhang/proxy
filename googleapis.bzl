# Copyright 2017 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

def googleapis_repositories(bind=True):
    GOOGLEAPIS_BUILD_FILE = """
package(default_visibility = ["//visibility:public"])

load("@com_google_protobuf//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "http_api_protos",
    srcs = [
        "google/api/annotations.proto",
        "google/api/http.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_wkt_protos",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)
cc_proto_library(
    name = "servicecontrol",
    srcs = [
        "google/api/servicecontrol/v1/check_error.proto",
        "google/api/servicecontrol/v1/distribution.proto",
        "google/api/servicecontrol/v1/log_entry.proto",
        "google/api/servicecontrol/v1/metric_value.proto",
        "google/api/servicecontrol/v1/operation.proto",
        "google/api/servicecontrol/v1/quota_controller.proto",
        "google/api/servicecontrol/v1/service_controller.proto",
        "google/api/servicemanagement/v1/servicemanager.proto",
        "google/api/servicemanagement/v1/resources.proto",
        "google/logging/type/http_request.proto",
        "google/logging/type/log_severity.proto",
        "google/api/config_change.proto",
        "google/longrunning/operations.proto",
        "google/rpc/error_details.proto",
        "google/type/money.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        ":rpc_status_proto",
        ":service_config",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)

cc_proto_library(
    name = "service_config",
    srcs = [
        "google/api/auth.proto",
        "google/api/backend.proto",
        "google/api/billing.proto",
        "google/api/consumer.proto",
        "google/api/context.proto",
        "google/api/control.proto",
        "google/api/documentation.proto",
        "google/api/endpoint.proto",
        "google/api/label.proto",
        "google/api/log.proto",
        "google/api/logging.proto",
        "google/api/metric.proto",
        "google/api/experimental/experimental.proto",
        "google/api/experimental/authorization_config.proto",
        "google/api/monitored_resource.proto",
        "google/api/monitoring.proto",
        "google/api/quota.proto",
        "google/api/service.proto",
        "google/api/source_info.proto",
        "google/api/system_parameter.proto",
        "google/api/usage.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        ":http_api_protos",
        "//external:cc_wkt_protos",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)

cc_proto_library(
    name = "cloud_trace",
    srcs = [
        "google/devtools/cloudtrace/v1/trace.proto",
    ],
    include = ".",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        ":service_config",
        "//external:cc_wkt_protos",
    ],
)

cc_proto_library(
    name = "rpc_status_proto",
    srcs = [
        "google/rpc/status.proto",
    ],
    visibility = ["//visibility:public"],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
    deps = [
        "//external:cc_wkt_protos",
    ],
)

"""
    new_git_repository(
        name = "com_github_googleapis_googleapis",
        build_file_content = GOOGLEAPIS_BUILD_FILE,
        commit = "ec795a6a63eaf529c225ec45fa9de535bff53c99",
        remote = "https://github.com/googleapis/googleapis.git",
    )

    if bind:
        native.bind(
            name = "rpc_status_proto",
            actual = "@com_github_googleapis_googleapis//:rpc_status_proto",
        )
        native.bind(
            name = "rpc_status_proto_genproto",
            actual = "@com_github_googleapis_googleapis//:rpc_status_proto_genproto",
        )

        native.bind(
            name = "servicecontrol",
            actual = "@com_github_googleapis_googleapis//:servicecontrol",
        )

        native.bind(
            name = "servicecontrol_genproto",
            actual = "@com_github_googleapis_googleapis//:servicecontrol_genproto",
        )

        native.bind(
            name = "service_config",
            actual = "@com_github_googleapis_googleapis//:service_config",
        )

        native.bind(
            name = "cloud_trace",
            actual = "@com_github_googleapis_googleapis//:cloud_trace",
        )

        native.bind(
            name = "http_api_protos",
            actual = "@com_github_googleapis_googleapis//:http_api_protos",
        )
