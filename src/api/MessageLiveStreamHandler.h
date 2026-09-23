#pragma once
#include <system_error>

#include "service/media/dto/LiveStreamDto.h"

namespace cosmo { struct RequestDispatchContext; }

namespace cosmo::service {
class ILiveStreamService;
}  // namespace cosmo::service

namespace cosmo {

class MessageLiveStreamHandler {
public:
    explicit MessageLiveStreamHandler(service::ILiveStreamService& live_stream_service);
    LiveStream::MsgRequestLiveStreamSend Handle(LiveStream::MsgRequestLiveStreamRecv&& data,
                                                std::error_condition& errc);
    LiveStream::MsgStreamKeepAliveSend Handle(LiveStream::MsgStreamKeepAliveRecv&& data,
                                              std::error_condition& errc);
    LiveStream::MsgStreamStopSend Handle(LiveStream::MsgStreamStopRecv&& data, std::error_condition& errc);
    LiveStream::MsgListChannelsSend Handle(LiveStream::MsgListChannelsRecv&& data,
                                           const RequestDispatchContext& context, std::error_condition& errc);
    LiveStream::MsgGetOsdPictureSend Handle(LiveStream::MsgGetOsdPictureRecv&& data,
                                            const RequestDispatchContext& context, std::error_condition& errc);

private:
    service::ILiveStreamService& live_stream_service_;
};

}  // namespace cosmo
