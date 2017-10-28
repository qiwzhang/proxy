
#include "attribute_names.h"

// Define attribute names
const std::string AttributeNames::kSourceUser = "source.user";

const std::string AttributeNames::kRequestHeaders = "request.headers";
const std::string AttributeNames::kRequestHost = "request.host";
const std::string AttributeNames::kRequestMethod = "request.method";
const std::string AttributeNames::kRequestPath = "request.path";
const std::string AttributeNames::kRequestReferer = "request.referer";
const std::string AttributeNames::kRequestScheme = "request.scheme";
const std::string AttributeNames::kRequestSize = "request.size";
const std::string AttributeNames::kRequestTime = "request.time";
const std::string AttributeNames::kRequestUserAgent = "request.useragent";

const std::string AttributeNames::kResponseCode = "response.code";
const std::string AttributeNames::kResponseDuration = "response.duration";
const std::string AttributeNames::kResponseHeaders = "response.headers";
const std::string AttributeNames::kResponseSize = "response.size";
const std::string AttributeNames::kResponseTime = "response.time";

// TCP attributes
// Downstream tcp connection: source ip/port.
const std::string AttributeNames::kSourceIp = "source.ip";
const std::string AttributeNames::kSourcePort = "source.port";
// Upstream tcp connection: destionation ip/port.
const std::string AttributeNames::kDestinationIp = "destination.ip";
const std::string AttributeNames::kDestinationPort = "destination.port";
const std::string AttributeNames::kConnectionReceviedBytes = "connection.received.bytes";
const std::string AttributeNames::kConnectionReceviedTotalBytes =
    "connection.received.bytes_total";
const std::string AttributeNames::kConnectionSendBytes = "connection.sent.bytes";
const std::string AttributeNames::kConnectionSendTotalBytes = "connection.sent.bytes_total";
const std::string AttributeNames::kConnectionDuration = "connection.duration";

// Context attributes
const std::string AttributeNames::kContextProtocol = "context.protocol";
const std::string AttributeNames::kContextTime = "context.time";

// Check status code.
const std::string AttributeNames::kCheckStatusCode = "check.status";

