#include "service/system/dto/SystemOsdDto.h"

#include <nlohmann/json.hpp>

#include "util/JsonFieldOpt.h"

namespace cosmo::System {
void to_json(nlohmann::json& j, const MsgQueryOsdApiKeySend::ResData& v) {
    j["apiKey"] = v.apiKey;
}
void from_json(const nlohmann::json& j, MsgQueryOsdApiKeySend::ResData& v) {
    JSON_OPT(j, v, apiKey);
}
void to_json(nlohmann::json& j, const MsgQueryOsdApiKeySend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
    j["resData"] = v.resData;
}
void from_json(const nlohmann::json& j, MsgQueryOsdApiKeySend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
    JSON_OPT(j, v, resData);
}

void to_json(nlohmann::json& j, const MsgRegenerateOsdApiKeySend::ResData& v) {
    j["apiKey"] = v.apiKey;
}
void from_json(const nlohmann::json& j, MsgRegenerateOsdApiKeySend::ResData& v) {
    JSON_OPT(j, v, apiKey);
}
void to_json(nlohmann::json& j, const MsgRegenerateOsdApiKeySend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
    j["resData"] = v.resData;
}
void from_json(const nlohmann::json& j, MsgRegenerateOsdApiKeySend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
    JSON_OPT(j, v, resData);
}

void to_json(nlohmann::json& j, const MsgQueryOsdConfigSend::ResData& v) {
    j["enterLabel"] = v.enterLabel;
    j["leaveLabel"] = v.leaveLabel;
    j["xRatio"]     = v.xRatio;
    j["yRatio"]     = v.yRatio;
    j["fontSize"]   = v.fontSize;
}
void from_json(const nlohmann::json& j, MsgQueryOsdConfigSend::ResData& v) {
    JSON_OPT(j, v, enterLabel);
    JSON_OPT(j, v, leaveLabel);
    JSON_OPT(j, v, xRatio);
    JSON_OPT(j, v, yRatio);
    JSON_OPT(j, v, fontSize);
}
void to_json(nlohmann::json& j, const MsgQueryOsdConfigSend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
    j["resData"] = v.resData;
}
void from_json(const nlohmann::json& j, MsgQueryOsdConfigSend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
    JSON_OPT(j, v, resData);
}

void to_json(nlohmann::json& j, const MsgSetOsdConfigRecv& v) {
    to_json(j, static_cast<const MsgRecvHead&>(v));
    j["enterLabel"] = v.enterLabel;
    j["leaveLabel"] = v.leaveLabel;
    j["xRatio"]     = v.xRatio;
    j["yRatio"]     = v.yRatio;
    j["fontSize"]   = v.fontSize;
}
void from_json(const nlohmann::json& j, MsgSetOsdConfigRecv& v) {
    from_json(j, static_cast<MsgRecvHead&>(v));
    JSON_OPT(j, v, enterLabel);
    JSON_OPT(j, v, leaveLabel);
    JSON_OPT(j, v, xRatio);
    JSON_OPT(j, v, yRatio);
    JSON_OPT(j, v, fontSize);
}

void to_json(nlohmann::json& j, const MsgSetOsdConfigSend& v) {
    to_json(j, static_cast<const MsgSendHead&>(v));
}
void from_json(const nlohmann::json& j, MsgSetOsdConfigSend& v) {
    from_json(j, static_cast<MsgSendHead&>(v));
}
}  // namespace cosmo::System
