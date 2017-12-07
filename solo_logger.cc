#include "solo_logger.h"
#include "server/config/network/http_connection_manager.h"

#include "common/http/message_impl.h"
#include "envoy/http/header_map.h"

#include "common/common/hex.h"
#include "common/common/empty_string.h"
#include "common/common/utility.h"


#define COPY_INLINE_HEADER(name, from, to) \
  if(from->name() != nullptr) \
    to->insert##name().value(std::string(from->name()->value().c_str()));

namespace solo {
namespace logger {

  CloudCollector::CloudCollector(Envoy::Upstream::ClusterManager& cm) :
    cm_(cm),
    cluster_name_("sololog"),
    timeout_(std::chrono::milliseconds(1000)),
    retry_count_(0),
    in_flight_request_(nullptr) {}

  CloudCollector::~CloudCollector() {}

  void CloudCollector::storeRequestInfo(solo::logger::RequestInfo& info, Envoy::Http::HeaderMap* headers) {

    Envoy::Http::MessagePtr request(new Envoy::Http::RequestMessageImpl());
    request->headers().insertContentType().value(std::string("application/json"));
    request->headers().insertPath().value(std::string("/api/v1/store"));
    request->headers().insertHost().value(std::string("sololog"));
    request->headers().insertMethod().value(std::string("POST"));

    Envoy::Http::HeaderMap* h = &(request->headers());
    COPY_INLINE_HEADER(RequestId, headers, h);
    COPY_INLINE_HEADER(XB3TraceId, headers, h);
    COPY_INLINE_HEADER(XB3SpanId, headers, h);
    COPY_INLINE_HEADER(XB3ParentSpanId, headers, h);
    COPY_INLINE_HEADER(XB3Sampled, headers, h);
    COPY_INLINE_HEADER(OtSpanContext, headers, h);

    //h->insertRequestId().value(std::string(headers->RequestId()->value().c_str()));

    std::string body = "{\"requestId\":\"" + info.request_id_ + 
          "\",\"function\":\"" + info.function_name_ + 
          "\",\"region\":\"" + info.region_ + 
          "\",\"project\":\"" + info.project_ + 
          "\",\"provider\":\"" + info.provider_ + "\"}";

    request->body().reset(new Envoy::Buffer::OwnedImpl(body));

    in_flight_request_ = cm_.httpAsyncClientForCluster(cluster_name_).send(std::move(request), *this, timeout_);
  }

  void CloudCollector::abortRequest() {
    if (in_flight_request_ != nullptr) {
      in_flight_request_->cancel();
      in_flight_request_ = nullptr;
    }
  }

  void CloudCollector::onSuccess(Envoy::Http::MessagePtr&&) {
  }

  void CloudCollector::onFailure(Envoy::Http::AsyncClient::FailureReason) {
  }

} // logger
} // solo