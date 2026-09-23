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
        if (c.color.empty() || c.color[0] != '#' || (c.color.size() != 7 && c.color.size() != 4)) {
            c.color = "#DCE7FF";
        }
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
            if (j.contains("color")) c.color = j["color"].get<std::string>();
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
        j["color"]      = c.color;
        ofs << j.dump(2) << std::endl;
    } catch (...) {
    }
    auto& h = Get();
    std::lock_guard<std::mutex> lock(h.mtx);
    h.cfg = c;
}

media::Color PassFlowOsdConfig::TextColor() const {
    media::Color c{220, 231, 255};
    const std::string& s = color;
    auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    if (s.size() == 7 && s[0] == '#') {
        int r1 = hex(s[1]), r2 = hex(s[2]), g1 = hex(s[3]), g2 = hex(s[4]), b1 = hex(s[5]), b2 = hex(s[6]);
        if (r1 >= 0 && r2 >= 0 && g1 >= 0 && g2 >= 0 && b1 >= 0 && b2 >= 0) {
            c.red   = static_cast<uint8_t>(r1 * 16 + r2);
            c.green = static_cast<uint8_t>(g1 * 16 + g2);
            c.blue  = static_cast<uint8_t>(b1 * 16 + b2);
        }
    }
    return c;
}

}  // namespace cosmo
