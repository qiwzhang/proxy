// Copyright 2017 Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

package test

import (
	"fmt"
	"os"
	"text/template"
)

const (
	// These ports should match with used envoy.conf
	// Default is using one in this folder.
	ServerProxyPort = 29090
	ClientProxyPort = 27070
	TcpProxyPort    = 26060
	MixerPort       = 29091
	BackendPort     = 28080
	AdminPort       = 29001
)

type ConfParam struct {
	ClientPort   int
	ServerPort   int
	TcpProxyPort int
	AdminPort    int
	MixerServer  string
	Backend      string
	ClientConfig string
	ServerConfig string
	AccessLog    string
}

// A basic config
const basicConfig = `
                  "mixer_attributes": {
                      "target.uid": "POD222",
                      "target.namespace": "XYZ222"
                  }
`

// A quota config without cache
const quotaConfig = `
                  "quota_name": "RequestCount",
                  "quota_amount": "5"
`

// A quota config with cache
const quotaCacheConfig = `
                  "quota_name": "RequestCount"
`

// A config with check cache keys
const checkCacheConfig = `
                  "check_cache_keys": [
                      "request.host",
                      "request.path",
                      "origin.user"
                  ]
`

// A config with network fail close policy
const networkFailClose = `
                  "network_fail_policy": "close"
`

// The default client proxy mixer config
const defaultClientMixerConfig = `
                   "forward_attributes": {
                      "source.uid": "POD11",
                      "source.namespace": "XYZ11"
                   }
`

// The envoy config template
const envoyConfTempl = `
{
  "listeners": [
    {
      "address": "tcp://0.0.0.0:{{.ServerPort}}",
      "bind_to_port": true,
      "filters": [
        {
          "type": "read",
          "name": "http_connection_manager",
          "config": {
            "codec_type": "auto",
            "stat_prefix": "ingress_http",
            "route_config": {
              "virtual_hosts": [
                {
                  "name": "backend",
                  "domains": ["*"],
                  "routes": [
                    {
                      "timeout_ms": 0,
                      "prefix": "/",
                      "cluster": "service1",
                      "opaque_config": {
                        "mixer_control": "on",
                        "mixer_forward": "off",
                        "mixer_attributes.target.user": "target-user",
                        "mixer_attributes.target.name": "target-name"
                      }
                    }
                  ]
                }
              ]
            },
            "access_log": [
              {
                "path": "{{.AccessLog}}"
              }
            ],
            "filters": [
              {
                "type": "decoder",
                "name": "mixer",
                "config": {
{{.ServerConfig}}
                }
              },
              {
                "type": "decoder",
                "name": "router",
                "config": {}
              }
            ]
          }
        }
      ]
    },
    {
      "address": "tcp://0.0.0.0:{{.ClientPort}}",
      "bind_to_port": true,
      "filters": [
        {
          "type": "read",
          "name": "http_connection_manager",
          "config": {
            "codec_type": "auto",
            "stat_prefix": "ingress_http",
            "route_config": {
              "virtual_hosts": [
                {
                  "name": "backend",
                  "domains": ["*"],
                  "routes": [
                    {
                      "timeout_ms": 0,
                      "prefix": "/",
                      "cluster": "service2",
                      "opaque_config": {
                        "mixer_forward_attributes.source.user": "source-user",
                        "mixer_forward_attributes.source.name": "source-name"
                      }
                    }
                  ]
                }
              ]
            },
            "access_log": [
              {
                "path": "{{.AccessLog}}"
              }
            ],
            "filters": [
              {
                "type": "decoder",
                "name": "mixer",
                "config": {
{{.ClientConfig}}
                }
              },
              {
                "type": "decoder",
                "name": "router",
                "config": {}
              }
            ]
          }
        }
      ]
    },
    {
      "address": "tcp://0.0.0.0:{{.TcpProxyPort}}",
      "bind_to_port": true,
      "filters": [
        {
          "type": "both",
          "name": "mixer",
          "config": {
{{.ServerConfig}}
          }
        },
        {
          "type": "read",
          "name": "tcp_proxy",
          "config": {
            "stat_prefix": "tcp",
            "route_config": {
              "routes": [
                {
                  "cluster": "service1"
                }
              ]
            }
          }
        }
      ]
    }
  ],
  "admin": {
    "access_log_path": "/dev/stdout",
    "address": "tcp://0.0.0.0:{{.AdminPort}}"
  },
  "cluster_manager": {
    "clusters": [
      {
        "name": "service1",
        "connect_timeout_ms": 5000,
        "type": "strict_dns",
        "lb_type": "round_robin",
        "hosts": [
          {
            "url": "tcp://{{.Backend}}"
          }
        ]
      },
      {
        "name": "service2",
        "connect_timeout_ms": 5000,
        "type": "strict_dns",
        "lb_type": "round_robin",
        "hosts": [
          {
            "url": "tcp://localhost:{{.ServerPort}}"
          }
        ]
      },
      {
        "name": "mixer_server",
        "connect_timeout_ms": 5000,
        "type": "strict_dns",
	"circuit_breakers": {
           "default": {
	      "max_pending_requests": 10000,
	      "max_requests": 10000
            }
	},
        "lb_type": "round_robin",
        "features": "http2",
        "hosts": [
          {
            "url": "tcp://{{.MixerServer}}"
          }
        ]
      }
    ]
  }
}
`

func (c *ConfParam) write(path string) error {
	tmpl, err := template.New("test").Parse(envoyConfTempl)
	if err != nil {
		return fmt.Errorf("Failed to parse config template: %v", err)
	}

	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("Failed to create file %v: %v", path, err)
	}
	defer f.Close()
	return tmpl.Execute(f, *c)
}

func getConf() ConfParam {
	return ConfParam{
		ClientPort:   ClientProxyPort,
		ServerPort:   ServerProxyPort,
		TcpProxyPort: TcpProxyPort,
		AdminPort:    AdminPort,
		MixerServer:  fmt.Sprintf("localhost:%d", MixerPort),
		Backend:      fmt.Sprintf("localhost:%d", BackendPort),
		ClientConfig: defaultClientMixerConfig,
		AccessLog:    "/dev/stdout",
	}
}

func CreateEnvoyConf(path string, conf string, stress bool) error {
	c := getConf()
	c.ServerConfig = conf
	if stress {
		c.AccessLog = "/dev/null"
	}
	return c.write(path)
}
