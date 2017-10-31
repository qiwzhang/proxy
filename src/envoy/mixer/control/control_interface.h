
#ifndef API__H_
#define API__H_

namespace istio {
  namespace mixer_client {

    class HttpRequestHandler {
      istio::mixer_client::CancelFunc Check(::istio::mixer_client::TransportCheckFunc transport,
					    ::istio::mixer_client::DoneFunc on_done);

      // Make remote report call.
      void Report(std::unique_ptr<HttpResponseData> response);
    };

    struct PerRouteConfig {
      bool enable_mixer_check;
      bool enable_mixer_report;
      bool forward_attributes;
    };

    struct FactoryData {
      // Mixer filter config
      const MixerFilterConfig& mixer_config;
      
      // Transport functions.
      TransportCheckFunc check_transport;
      TransportReportFunc report_transport;
      TimerCreateFunc timer_create_func;
      UUIDGenerateFunc uuid_generate_func; 
    };
    
    class Controller {
    public:
      virtual std::unique_ptr<HttpRequestHandler> CreateHttpRequestHandler(
								   std::unique_ptr<HttpRequestData> request,
								   const PerRouteConfig& per_route_config) = 0;

      virtual std::unique_ptr<TcpRequestHandler> CreateTcpRequestHandler(
									 std::unique_ptr<TcpRequestData> request)  = 0;
      
      static std::unique_ptr<Controller> Create(std::unique_ptr<FactoryData> factory_data);
      
    };

  }  // namespace mixer_client
}  // istio

#endif // API__H_
