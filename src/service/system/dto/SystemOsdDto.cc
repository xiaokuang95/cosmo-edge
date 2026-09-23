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
}  // namespace cosmo::System
