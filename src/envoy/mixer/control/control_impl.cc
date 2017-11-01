





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
