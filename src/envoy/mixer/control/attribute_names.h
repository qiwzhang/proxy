
// Define attribute names
struct AttributeNames {  
  static const std::string kSourceUser;

static const std::string kRequestHeaders;
  static const std::string kRequestHost;
  static const std::string kRequestMethod;
  static const std::string kRequestPath;
static const std::string kRequestReferer;
  static const std::string kRequestScheme;
static const std::string kRequestSize;
static const std::string kRequestTime;
  static const std::string kRequestUserAgent;

static const std::string kResponseCode;
static const std::string kResponseDuration;
static const std::string kResponseHeaders;
static const std::string kResponseSize;
static const std::string kResponseTime;

// TCP attributes
// Downstream tcp connection: source ip/port.
static const std::string kSourceIp;
static const std::string kSourcePort;
// Upstream tcp connection: destionation ip/port.
  
static const std::string kDestinationIp;
static const std::string kDestinationPort;
static const std::string kConnectionReceviedBytes;
  static const std::string kConnectionReceviedTotalBytes;
static const std::string kConnectionSendBytes;
static const std::string kConnectionSendTotalBytes;
static const std::string kConnectionDuration;

// Context attributes
static const std::string kContextProtocol;
static const std::string kContextTime;

// Check status code.
static const std::string kCheckStatusCode;
};


