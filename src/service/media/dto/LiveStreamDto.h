// LiveStream DTO definitions (extracted from MessageLiveStreamHandler.h)

#pragma once

#include <system_error>

#include "util/dto/ServerMsgTypes.h"

namespace cosmo {
namespace LiveStream {
    struct MsgRequestLiveStreamRecv : public MsgRecvHead {
        std::string channelId;
        std::string algorithmId;
    };

    void to_json(nlohmann::json& j, const MsgRequestLiveStreamRecv& v);
    void from_json(const nlohmann::json& j, MsgRequestLiveStreamRecv& v);

    // Stream info for live stream response
    struct LiveStreamInfo {
        std::string protocol{"rtmp"};
        std::string url;
        std::string webrtcUrl;
        std::string flvUrl;
        std::string hlsUrl;
        int keepAliveInterval;
        std::string keepAliveUrl;
        int port{8081};
        int httpPort{8080};
        int rtcApiPort{1985};
        int portMin{8081};
        int portMax{8088};
        friend void to_json(nlohmann::json& j, const LiveStreamInfo& v);
        friend void from_json(const nlohmann::json& j, LiveStreamInfo& v);
    };

    struct LiveStreamResData {
        LiveStreamInfo stream;
        friend void to_json(nlohmann::json& j, const LiveStreamResData& v);
        friend void from_json(const nlohmann::json& j, LiveStreamResData& v);
    };

    //
    struct MsgRequestLiveStreamSend : public MsgSendHead {
        LiveStreamResData resData;
    };

    void to_json(nlohmann::json& j, const MsgRequestLiveStreamSend& v);
    void from_json(const nlohmann::json& j, MsgRequestLiveStreamSend& v);

    // Video heartbeat request
    struct MsgStreamKeepAliveRecv : public MsgRecvHead {
        std::string channelId;
        std::string algorithmId;
    };

    void to_json(nlohmann::json& j, const MsgStreamKeepAliveRecv& v);
    void from_json(const nlohmann::json& j, MsgStreamKeepAliveRecv& v);

    // Video heartbeat response
    struct MsgStreamKeepAliveSend : public MsgSendHead {};

    struct MsgStreamStopRecv : public MsgRecvHead {
        std::string channelId;
        std::string algorithmId;
    };

    void to_json(nlohmann::json& j, const MsgStreamStopRecv& v);
    void from_json(const nlohmann::json& j, MsgStreamStopRecv& v);

    // Video heartbeat response
    struct MsgStreamStopSend : public MsgSendHead {};

    // ── Third-party OSD snapshot API ──

    struct MsgListChannelsRecv : public MsgRecvHead {};
    struct MsgListChannelsAlgorithm {
        std::string algorithmId;
        std::string algorithmName;
        int enableStatus{0};
        friend void to_json(nlohmann::json& j, const MsgListChannelsAlgorithm& v);
        friend void from_json(const nlohmann::json& j, MsgListChannelsAlgorithm& v);
    };
    struct MsgListChannelsRow {
        std::string channelId;
        std::string channelName;
        std::vector<MsgListChannelsAlgorithm> algorithms;
        friend void to_json(nlohmann::json& j, const MsgListChannelsRow& v);
        friend void from_json(const nlohmann::json& j, MsgListChannelsRow& v);
    };
    struct MsgListChannelsResData {
        std::vector<MsgListChannelsRow> rows;
        friend void to_json(nlohmann::json& j, const MsgListChannelsResData& v);
        friend void from_json(const nlohmann::json& j, MsgListChannelsResData& v);
    };
    struct MsgListChannelsSend : public MsgSendHead {
        MsgListChannelsResData resData;
    };
    void to_json(nlohmann::json& j, const MsgListChannelsSend& v);
    void from_json(const nlohmann::json& j, MsgListChannelsSend& v);

    struct MsgGetOsdPictureRecv : public MsgRecvHead {
        std::string channelId;
        std::string algorithmId;
        std::string base64{"1"};
    };
    void from_json(const nlohmann::json& j, MsgGetOsdPictureRecv& v);

    struct MsgGetOsdPictureResData {
        std::string url;
        std::string fullUrl;
        std::string pictureBase64;
        friend void to_json(nlohmann::json& j, const MsgGetOsdPictureResData& v);
        friend void from_json(const nlohmann::json& j, MsgGetOsdPictureResData& v);
    };
    struct MsgGetOsdPictureSend : public MsgSendHead {
        MsgGetOsdPictureResData resData;
    };
    void to_json(nlohmann::json& j, const MsgGetOsdPictureSend& v);
    void from_json(const nlohmann::json& j, MsgGetOsdPictureSend& v);
}  // namespace LiveStream
}  // namespace cosmo
