#pragma once

#include <string>

namespace cosmo {

// User-tunable pass-flow OSD appearance: counter labels and on-screen position.
// Persisted at <cfg>/passflow_osd.json; updated at runtime via System/SetOsdConfig.
// xRatio/yRatio are the top-left corner of the two-line counter block, as
// fractions of frame width/height.
struct PassFlowOsdConfig {
    std::string enterLabel{"\u8fdb\u5165"};
    std::string leaveLabel{"\u79bb\u5f00"};
    double xRatio{0.7};
    double yRatio{0.6};
    int fontSize{22};

    static PassFlowOsdConfig Snapshot();              // thread-safe copy
    static void Store(const PassFlowOsdConfig& cfg);  // persist + update snapshot
};

}  // namespace cosmo
