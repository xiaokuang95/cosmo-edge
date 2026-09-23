#pragma once

#include <system_error>

#include "util/dto/ServerMsgTypes.h"

namespace cosmo::System {

// apiKey (OSD snapshot / channel list third-party key) management. mtk auth only.
struct MsgQueryOsdApiKeyRecv : public MsgRecvHead {};

struct MsgQueryOsdApiKeySend : public MsgSendHead {
    struct ResData {
        std::string apiKey;
        friend void to_json(nlohmann::json& j, const ResData& v);
        friend void from_json(const nlohmann::json& j, ResData& v);
    } resData;
};
void to_json(nlohmann::json& j, const MsgQueryOsdApiKeySend& v);
void from_json(const nlohmann::json& j, MsgQueryOsdApiKeySend& v);

struct MsgRegenerateOsdApiKeyRecv : public MsgRecvHead {};

struct MsgRegenerateOsdApiKeySend : public MsgSendHead {
    struct ResData {
        std::string apiKey;
        friend void to_json(nlohmann::json& j, const ResData& v);
        friend void from_json(const nlohmann::json& j, ResData& v);
    } resData;
};
void to_json(nlohmann::json& j, const MsgRegenerateOsdApiKeySend& v);
void from_json(const nlohmann::json& j, MsgRegenerateOsdApiKeySend& v);


// Pass-flow OSD appearance (counter labels + position). mtk auth only.
struct MsgQueryOsdConfigRecv : public MsgRecvHead {};

struct MsgQueryOsdConfigSend : public MsgSendHead {
    struct ResData {
        std::string enterLabel;
        std::string leaveLabel;
        double xRatio{0.7};
        double yRatio{0.6};
        friend void to_json(nlohmann::json& j, const ResData& v);
        friend void from_json(const nlohmann::json& j, ResData& v);
    } resData;
};
void to_json(nlohmann::json& j, const MsgQueryOsdConfigSend& v);
void from_json(const nlohmann::json& j, MsgQueryOsdConfigSend& v);

struct MsgSetOsdConfigRecv : public MsgRecvHead {
    std::string enterLabel{"进入"};
    std::string leaveLabel{"离开"};
    double xRatio{0.7};
    double yRatio{0.6};
    friend void to_json(nlohmann::json& j, const MsgSetOsdConfigRecv& v);
    friend void from_json(const nlohmann::json& j, MsgSetOsdConfigRecv& v);
};

struct MsgSetOsdConfigSend : public MsgSendHead {};
void to_json(nlohmann::json& j, const MsgSetOsdConfigSend& v);
void from_json(const nlohmann::json& j, MsgSetOsdConfigSend& v);
}  // namespace cosmo::System
