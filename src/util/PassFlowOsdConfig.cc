#include "util/PassFlowOsdConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>

#include <nlohmann/json.hpp>

#include "util/PathUtil.h"

namespace cosmo {
namespace {

struct Holder {
    static void Clamp(PassFlowOsdConfig& c) {
        c.enterLabel.resize(std::min<size_t>(c.enterLabel.size(), 32));
        c.leaveLabel.resize(std::min<size_t>(c.leaveLabel.size(), 32));
        c.xRatio = std::clamp(c.xRatio, 0.0, 1.0);
        c.yRatio = std::clamp(c.yRatio, 0.0, 1.0);
        c.fontSize = std::clamp(c.fontSize, 10, 64);
    }

    static PassFlowOsdConfig Load() {
        PassFlowOsdConfig c;
        try {
            const auto path = std::filesystem::path(cosmo::path::GetCfgPath()) / "passflow_osd.json";
            std::ifstream ifs(path);
            if (!ifs) {
                return c;
            }
            auto j = nlohmann::json::parse(ifs, nullptr, false);
            if (j.is_discarded()) {
                return c;
            }
            if (j.contains("enterLabel")) c.enterLabel = j["enterLabel"].get<std::string>();
            if (j.contains("leaveLabel")) c.leaveLabel = j["leaveLabel"].get<std::string>();
            if (j.contains("xRatio")) c.xRatio = j["xRatio"].get<double>();
            if (j.contains("yRatio")) c.yRatio = j["yRatio"].get<double>();
            if (j.contains("fontSize")) c.fontSize = j["fontSize"].get<int>();
        } catch (...) {
        }
        Clamp(c);
        return c;
    }

    std::mutex mtx;
    PassFlowOsdConfig cfg = Load();
};

Holder& Get() {
    static Holder h;
    return h;
}

}  // namespace

PassFlowOsdConfig PassFlowOsdConfig::Snapshot() {
    auto& h = Get();
    std::lock_guard<std::mutex> lock(h.mtx);
    return h.cfg;
}

void PassFlowOsdConfig::Store(const PassFlowOsdConfig& cfg) {
    PassFlowOsdConfig c = cfg;
    Holder::Clamp(c);
    try {
        const auto path = std::filesystem::path(cosmo::path::GetCfgPath()) / "passflow_osd.json";
        std::ofstream ofs(path);
        nlohmann::json j;
        j["enterLabel"] = c.enterLabel;
        j["leaveLabel"] = c.leaveLabel;
        j["xRatio"]     = c.xRatio;
        j["yRatio"]     = c.yRatio;
        j["fontSize"]   = c.fontSize;
        ofs << j.dump(2) << std::endl;
    } catch (...) {
    }
    auto& h = Get();
    std::lock_guard<std::mutex> lock(h.mtx);
    h.cfg = c;
}

}  // namespace cosmo
