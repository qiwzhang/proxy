

CheckOptions GetJustCheckOptions(const MixerFilterConfig& config) {
  if (config.disable_check_cache()) {
    return CheckOptions(0);
  }
  return CheckOptions();
}

CheckOptions GetCheckOptions(const MixerFilterConfig& config) {
  auto options = GetJustCheckOptions(config);
  if (config.network_fail_policy() == MixerFilterConfig::FAIL_CLOSE) {
    options.network_fail_open = false;
  }
  return options;
}

QuotaOptions GetQuotaOptions(const MixerFilterConfig& config) {
  if (config.disable_quota_cache()) {
    return QuotaOptions(0, 1000);
  }
  return QuotaOptions();
}

ReportOptions GetReportOptions(const MixerFilterConfig& config) {
  if (config.disable_report_batch()) {
    return ReportOptions(0, 1000);
  }
  return ReportOptions();
}

ClientContext::ClientContext(const FactoryData& data) : mixer_config_(data.mixer_config) {
  MixerClientOptions options(GetCheckOptions(mixer_config_),
			     GetReportOptions(mixer_config_),
			     GetQuotaOptions(mixer_config_));
  options.check_transport = data.check_transport;
  options.report_transport = data.report_transport;
  options.timer_create_func = data.timer_create_func;
  options.uuid_generate_func = data.uuid_generate_func;

  mixer_client_ = ::istio::mixer_client::CreateMixerClient(options);
}

istio::mixer_client::CancelFunc ClientContext::SendCheck(
					  const Attributes& attributes,
					  TransportCheckFunc transport,
					  DoneFunc on_done) {
  if (!mixer_client_) {
    on_done(
        Status(StatusCode::INVALID_ARGUMENT, "Missing mixer_server cluster"));
    return nullptr;
  }
  ENVOY_LOG(debug, "Send Check: {}", request_data->attributes.DebugString());
  return mixer_client_->Check(attributes, transport, on_done);
}

void ClientContext::SendReport(const Attributes& attributes) {
  if (!mixer_client_) {
    return;
  }
  ENVOY_LOG(debug, "Send Report: {}", request_data->attributes.DebugString());
  mixer_client_->Report(attributes);
}


// Per client context
class ClientContext {
public:
  ClientContext(const FactoryData& data);

  
  CancelFunc SendCheck(const Attributes& attributes,
		       TransportCheckFunc transport,
		       DoneFunc on_done);
  
  void SendReport(const Attributes& attributes);

 private:
  // The mixer client
  std::unique_ptr<::istio::mixer_client::MixerClient> mixer_client_;
  const MixerFilterConfig& config_;
};

// Per service context (per-route)
class ServiceContext {
public:
  
  std::shared_ptr<ClientContext> client_context;
private:
  const MixerControlConfig& service_config_;
};


// Per request context
class HttpRequestContext {
public:
  void ExtractRequestAttributes() {
  }

  void ExtractResponseAttributes(std::unique_ptr<HttpResponseData> response) {
  }
  
  CancelFunc Check(TransportCheckFunc transport,
		   DoneFunc on_done) {
    return service_context()->client_context()->SendCheck(attributes_, transport, on_done)
  }
  
  void Report() {
    service_context()->client_context()->SendReport(attributes_);
  }

private:
  Attributes attributes_;

  std::shared_ptr<ServiceContext> service_context;
  std::unique_ptr<HttpRequestData> request_data;
  const PerRouteConfig& per_route_config;
};


class HttpRequestData {
public:
  // Find "x-istio-attributes" headers, if found base64 decode
  // its value and remove it from the headers.
  virtual bool ExtractIstioAttributes(std::string* data) = 0;
  // base64 encode data, and add it to the HTTP header.
  virtual void AddIstoAttributes(const std::string& data) = 0;

  virtual bool GetSourceIpPort(std::string* ip, int* port) const = 0;

  virtual bool GetSourceUser(std::string* user) const = 0;
  
  virtual std::map<std::string, std::string> GetHeaders() const = 0;

  // These headers are extracted into top level attributes
  // They can be retrieved at O(1) speed by platform.
  enum HeaderType {
    HEADER_PATH = 0,
    HEADER_HOST,
    HEADER_SCHEME,
    HEADER_USER_AGENT,
    HEADER_METHOD,
    HEADER_REFERER,
  };
  virtual bool FindHeader(HeaderType header_type, std::string* value) const = 0;
};
    


class EnvoyHttpRequestData : public HttpRequestData {
public:
  EnvoyHttpRequestData(HeaderMap& headers) : headers_(headers) {}
  
  bool ExtractIstioAttributes(std::string* data) override {
    // Extract attributes from x-istio-attributes header
    const HeaderEntry* entry = headers_.get(Utils::kIstioAttributeHeader);
    if (entry) {
      *data = Base64::decode(strstr(entry->value().c_str(), entry->value().size()));
      headers_.remove(Utils::kIstioAttributeHeader);
      return true;
    }
    return false;
  }

  void AddIstoAttributes(const std::string& serialized_str) override {
    std::string base64 =
      Base64::encode(serialized_str.c_str(), serialized_str.size());
    ENVOY_LOG(debug, "Mixer forward attributes set: {}", base64);
    headers.addReferenceKey(Utils::kIstioAttributeHeader, base64);
  }
  
  bool GetSourceIpPort(std::string* str_ip, int* port) override {
    if (connection_) {
      const Network::Address::Ip* ip = connection->remoteAddress().ip();
      if (ip) {
	*port = ip->port();
	if (ip->ipv4()) {
	  uint32_t ipv4 = ip->ipv4()->address();
	  *str_ip = std::string(reinterpret_cast<const char*>(&ipv4), sizeof(ipv4));
	  return true;
	}
	if (ip->ipv6()) {
	  std::array<uint8_t, 16> ipv6 = ip.ipv6()->address();
	  *str_ip = std::string(reinterpret_cast<const char*>(ipv6.data()), 16);
	  return true;
	}
      }
    }
    return false;
  }

  bool GetSourceUser(std::string* user) const override {
    Ssl::Connection* ssl = const_cast<Ssl::Connection*>(connection_->ssl());
    if (ssl != nullptr) {
      *user = ssl->uriSanPeerCertificate();
      return true;
    }
    return false;
  }

  std::map<std::string, std::string> GetHeaders() const override {
    std::map<std::string, std::string> headers;
    header_map.iterate(
		       [](const HeaderEntry& header, void* context) -> HeaderMap::Iterate {
			 std::map<std::string, std::string>* header_map =
			   static_cast<std::map<std::string, std::string>*>(context);
			 (*header_map)[header.key().c_str()] = header.value().c_str();
			 return HeaderMap::Iterate::Continue;
		       },
		       &headers);
    return headers;
  }
  
  bool FindHeader(HeaderType header_type, std::string* value) const override {
    switch header_type {
      case HEADER_PATH:
	if (headers_.Path()) {
	  *value  = std::string(headers_.Path()->value().c_str(),
				headers_.Path()->value().size());
	  return true;
	}
	break;
      case HEADER_HOST:
	if (headers_.Host()) {
	  *value = std::string(headers_.Host()->value().c_str(),
			       headers_.Host()->value().c_str());
	  return true;
	}
	break;
      case HEADER_SCHEME:
	if (headers_.Scheme()) {
	  *value = std::string(headers_.Scheme()->value().c_str(),
			       headers_.Scheme()->value().c_str());
	  return true;
	}
	break:	  
      case HEADER_USER_AGENT:
	  if (headers_.UserAgent()) {
	    *value = std::string(headers_.UserAgent()->value().c_str(),
				 headers_.UserAgent()->value().c_str());
	  return true;
	}
	break:	  
      case HEADER_METHOD:
	  if (headers_.Method()) {
	    *value = std::string(headers_.Method()->value().c_str(),
				 headers_.Method()->value().c_str());
	    return true;
	  }
	break:	  
      case HEADER_REFERER:
	  {
	    const HeaderEntry* referer = headers_.get(kRefererHeaderKey);
	    if (referer) {
	      *data = std::string(referer->value().c_str(), referer->value().size());
	      return true;
	    }
	  }
	break:	  
      }
    return false;
  }
  
private:
  HeaderMap& headers_;
  const Network::Connection* connection_;
};
    

class HttpResponseData {
public:
  virtual std::map<std::string, std::string> GetHeaders() const = 0;

  struct RequestInfo {
    uint64_t send_bytes;
    uint64_t received_bytes;
    std::chrono::nanoseconds duration;
    int response_code;
  };
  virtual void GetInfo(RequestInfo* info) const = 0;
};

class EnvoyHttpResponseData : public HttpResponseData {
public:
  EnvoyHttpResponseData(HeaderMap& headers,
			const AccessLog::RequestInfo& info, int check_status_code) : headers_(headers),
										     info_(info), check_status_codes_(check_status_code) {}
  
  std::map<std::string, std::string> GetHeaders() const override {
    std::map<std::string, std::string> headers;
    header_map.iterate(
		       [](const HeaderEntry& header, void* context) -> HeaderMap::Iterate {
			 std::map<std::string, std::string>* header_map =
			   static_cast<std::map<std::string, std::string>*>(context);
			 (*header_map)[header.key().c_str()] = header.value().c_str();
			 return HeaderMap::Iterate::Continue;
		       },
		       &headers);
    return headers;
  }
  
  void GetInfo(RequestInfo* data) const override {
    data->received_bytes = info_.bytesReceived();
    data->send_bytes = info_.bytesSent();
    data->duration = 
      std::chrono::duration_cast<std::chrono::nanoseconds>(info_.duration());

    if (info.responseCode().valid()) {
      data->response_code = info.responseCode().value();
    } else {
      data->response_code = check_status_code_;
    }
  }
  
private:
  HeaderMap& headers_;
  const AccessLog::RequestInfo& info_;
  int check_status_code_;
}

void HttpRequestContext::FillRequestHeaderAttributes() {
  AttributesBuilder builder(&attributes);
  std::map<std::string, std::string> headers = request_data()->GetHeaders();
  builder.AddStringMap(kRequestHeaders, headers);

  struct TopLevelHeader {
    aderType header_type;
    const std::string& name;
    bool set_default;
    const char * default_value;      
  };
  static TopLevelheaders top_level_atts[] = {
    {HEADER_PATH, kRequestPath, true, ""},
    {HEADER_HOST, kRequestHost, true, ""},
    {HEADER_SCHEME, kRequestScheme, true, "http"},
    {HEADER_USER_AGENT, kRequestUserAgent, false, ""},
    {HEADER_METHOD, kRequestMethod, false, ""},
    {HEADER_REFERER, kRequestReferer, false, ""}
  };
  for (const auto & it : top_level_atts) {
    std::string data;
    if (request_data()->FindHeader(it.header_type, &data)) {
      builder.AddString(it.name, data);
    } else if (it.set_default) {
      builder.AddString(it.name, it.default_value);
    }
  }
}

void HttpRequestContext::ExtractRequestAttributes() {
  std::string forwarded_data;
  if (request_data()->ExtractIstoAttributes(&forwarded_data)) {    
    ::istio::mixer::v1::Attributes_StringMap forwarded_attributes;
    forwarded_attributes.ParseFromString(forwarded_data);
      
    for (const auto& it : forwarded_attributes.entries()) {
      SetMeshAttribute(it.first, it.second, &attributes);
    }
  }

  // Quota should be part of service_config().mixer_attributes.
  attributes.MergeFrom(service_context().service_config().mixer_attributes());
  attributes.MergeFrom(pre_route_config().mixer_attributes());

  AttributesBuilder builder(&attributes);
  std::string source_ip;
  int  source_port;
  if (request_data()->GetSourceIpPort(source_ip, source_port)) {
    builder.AddBytes(kSourceIp, source_ip);
    builder.AddInt64(kSourcePort, source_port);
  }

  FillRequestHeaderAttributes();

  std::string source_user;
  if (request_data()->GetSourceUser(&source_user)) {
    builder.AddString(kSourceUser, source_user);
  }
  builder.AddTimestamp(kRequestTime, std::chrono::system_clock::now());
  builder.AddString(kContextProtocol, "http");

  if (!service_config_.forward_attributes().empty()) {
    std::string str;
    service_config_.forward_attributes().SerializeToString(&str);
    request_data()->AddIstioAttributes(str);  ForwardAttributes();
  }
}

void HttpRequestContext::ExtractResponseAttributes(std::unique_ptr<HttpResponseData> response_data) {
  AttributesBuilder builder(&attributes);
  std::map<std::string, std::string> headers = response_data()->GetHeaders();
  builder.AddStringMap(kResponseHeaders, headers);

  builder.AddTimestamp(kResponseTime, std::chrono::system_clock::now());

  RequestInfo info;
  response_data->GetInfo(&info);
  builder.AddInt64(kRequestSize,data.received_bytes);
  builder.AddInt64(kResponseSize, data.send_bytes);
  builder.AddDuration(kResponseDuration, info.duration);
  builder.AddInt64(kResponseCode, info.response_code);
}

class ControllerImpl : public Controller {
 public:
  ControllerImpl(const FactoryData& data);

 private:
  std::shared_ptr<ClientContext> client_context_;
};
    

ControllerImpl::ControllerImpl(const FactoryData& data) : mixer_config_(data.mixer_config) {
  client_context_ = make_shared<ClientContext>(data);
}



class HttpRequestHandlerImpl : public HttpRequestHandler {
public:
  HttpRequestHandlerImpl(std::shared_ptr<ServiceContext> service_context,
			 std::unique_ptr<HttpRequestData> request_data,
			 const PerRouteConfig& per_route_config) :
    request_context_(new HttpRequestContext(service_context, request_data, per_route_config)) {
  }
  
  istio::mixer_client::CancelFunc Check(::istio::mixer_client::TransportCheckFunc transport,
					::istio::mixer_client::DoneFunc on_done) {
    
    request_context_->ExtractRequestAttributes();
    return request_context_->Check(transport, on_done);
  }

      // Make remote report call.
  void Report(std::unique_ptr<HttpResponseData> response) {
    request_context_->ExtractResponseAttributes(std::move(response));
    request_context_->Report();
  }

private:
  std::unique_ptr<RequestContext> request_context_;
}

std::unique_ptr<HttpRequestHandler> ControllerImpl::CreateHttpRequestHandler(
								   std::unique_ptr<HttpRequestData> request_data,
								   const PerRouteConfig& per_route_config) {
  return std::unique_ptr<HttpRequestHandler>(new HttpRequestHandlerImpl(
									client_context_->SelectService(), std::move(request_data), per_route_config));
}


std::unique_ptr<TcpRequestHandler> ControllerImpl::CreateTcpRequestHandler(
									   std::unique_ptr<TcpRequestData> request) {
}
