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

}  // namespace cosmo::System
