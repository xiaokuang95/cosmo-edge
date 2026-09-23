// LiveStreamDto — LiveStream DTO definitions (extracted from MessageLiveStreamHandler.h)

#include "LiveStreamDto.h"

#include <nlohmann/json.hpp>

#include "util/JsonFieldOpt.h"
#include "util/LimitedTypeJson.h"

// Auto-generated JSON serialization
namespace cosmo::LiveStream {
void to_json(nlohmann::json& j, const MsgRequestLiveStreamRecv& v) {
    to_json(j, static_cast<const MsgRecvHead&>(v));
    j["channelId"]   = v.channelId;
    j["algorithmId"] = v.algorithmId;
}

void from_json(const nlohmann::json& j, MsgRequestLiveStreamRecv& v) {
    from_json(j, static_cast<MsgRecvHead&>(v));
    JSON_OPT(j, v, channelId);
    JSON_OPT(j, v, algorithmId);
}

void to_json(nlohmann::json& j, const MsgRequestLiveStreamSend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
    j["resData"] = v.resData;
}

void from_json(const nlohmann::json& j, MsgRequestLiveStreamSend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
    JSON_OPT(j, v, resData);
}

void to_json(nlohmann::json& j, const MsgStreamKeepAliveRecv& v) {
    to_json(j, static_cast<const MsgRecvHead&>(v));
    j["channelId"]   = v.channelId;
    j["algorithmId"] = v.algorithmId;
}

void from_json(const nlohmann::json& j, MsgStreamKeepAliveRecv& v) {
    from_json(j, static_cast<MsgRecvHead&>(v));
    JSON_OPT(j, v, channelId);
    JSON_OPT(j, v, algorithmId);
}

void to_json(nlohmann::json& j, const MsgStreamStopRecv& v) {
    to_json(j, static_cast<const MsgRecvHead&>(v));
    j["channelId"]   = v.channelId;
    j["algorithmId"] = v.algorithmId;
}

void from_json(const nlohmann::json& j, MsgStreamStopRecv& v) {
    from_json(j, static_cast<MsgRecvHead&>(v));
    JSON_OPT(j, v, channelId);
    JSON_OPT(j, v, algorithmId);
}

void from_json(const nlohmann::json& j, LiveStreamInfo& v) {
    JSON_OPT(j, v, protocol);
    JSON_OPT(j, v, url);
    JSON_OPT(j, v, webrtcUrl);
    JSON_OPT(j, v, flvUrl);
    JSON_OPT(j, v, hlsUrl);
    JSON_OPT(j, v, keepAliveInterval);
    JSON_OPT(j, v, keepAliveUrl);
    JSON_OPT(j, v, port);
    JSON_OPT(j, v, httpPort);
    JSON_OPT(j, v, rtcApiPort);
    JSON_OPT(j, v, portMin);
    JSON_OPT(j, v, portMax);
}

void to_json(nlohmann::json& j, const LiveStreamInfo& v) {
    j["protocol"]          = v.protocol;
    j["url"]               = v.url;
    j["webrtcUrl"]         = v.webrtcUrl;
    j["flvUrl"]            = v.flvUrl;
    j["hlsUrl"]            = v.hlsUrl;
    j["keepAliveInterval"] = v.keepAliveInterval;
    j["keepAliveUrl"]      = v.keepAliveUrl;
    j["port"]              = v.port;
    j["httpPort"]          = v.httpPort;
    j["rtcApiPort"]        = v.rtcApiPort;
    j["portMin"]           = v.portMin;
    j["portMax"]           = v.portMax;
}

void from_json(const nlohmann::json& j, LiveStreamResData& v) {
    JSON_OPT(j, v, stream);
}

void to_json(nlohmann::json& j, const LiveStreamResData& v) {
    j["stream"] = v.stream;
}


void to_json(nlohmann::json& j, const MsgListChannelsAlgorithm& v) {
    j["algorithmId"]     = v.algorithmId;
    j["algorithmName"]   = v.algorithmName;
    j["enableStatus"]    = v.enableStatus;
}
void from_json(const nlohmann::json& j, MsgListChannelsAlgorithm& v) {
    JSON_OPT(j, v, algorithmId);
    JSON_OPT(j, v, algorithmName);
    JSON_OPT(j, v, enableStatus);
}
void to_json(nlohmann::json& j, const MsgListChannelsRow& v) {
    j["channelId"]   = v.channelId;
    j["channelName"] = v.channelName;
    j["algorithms"]  = v.algorithms;
}
void from_json(const nlohmann::json& j, MsgListChannelsRow& v) {
    JSON_OPT(j, v, channelId);
    JSON_OPT(j, v, channelName);
    JSON_OPT(j, v, algorithms);
}
void to_json(nlohmann::json& j, const MsgListChannelsResData& v) {
    j["rows"] = v.rows;
}
void from_json(const nlohmann::json& j, MsgListChannelsResData& v) {
    JSON_OPT(j, v, rows);
}
void to_json(nlohmann::json& j, const MsgListChannelsSend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
    j["resData"] = v.resData;
}
void from_json(const nlohmann::json& j, MsgListChannelsSend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
    JSON_OPT(j, v, resData);
}

void from_json(const nlohmann::json& j, MsgGetOsdPictureRecv& v) {
    from_json(j, static_cast<MsgRecvHead&>(v));
    JSON_OPT(j, v, channelId);
    JSON_OPT(j, v, algorithmId);
    JSON_OPT(j, v, base64);
}
void to_json(nlohmann::json& j, const MsgGetOsdPictureResData& v) {
    j["url"]           = v.url;
    j["fullUrl"]       = v.fullUrl;
    j["pictureBase64"] = v.pictureBase64;
}
void from_json(const nlohmann::json& j, MsgGetOsdPictureResData& v) {
    JSON_OPT(j, v, url);
    JSON_OPT(j, v, fullUrl);
    JSON_OPT(j, v, pictureBase64);
}
void to_json(nlohmann::json& j, const MsgGetOsdPictureSend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
    j["resData"] = v.resData;
}
void from_json(const nlohmann::json& j, MsgGetOsdPictureSend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
    JSON_OPT(j, v, resData);
}

}  // namespace cosmo::LiveStream
