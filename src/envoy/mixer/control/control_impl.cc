

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
  virtual void ExtractForwardedAttributes(Attributes *attributes) = 0;
};
    

HttpRequestContext::ExtractHttpAttributes() {
  request_data->ExtractForwardedAttributes(&attributes_);
  if (
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
