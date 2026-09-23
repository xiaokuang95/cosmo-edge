#include "util/PassFlowOsdDraw.h"

#include <algorithm>

#include "flow/common/AreaLineUtil.h"
#include "service/detail/ServiceRegistry.h"
#include "service/media/IVideoFrameOSD.h"
#include "media/Color.h"

namespace cosmo {

bool PassFlowOsdDraw(VideoFramePtr frame, const std::vector<MsgTaskArea>& areas, int enter, int leave) {
    if (!frame) {
        return false;
    }
    auto& osd = service::ServiceRegistry::Instance().Get<service::IVideoFrameOSD>();
    if (!osd.BeginOSD(frame)) {
        return false;
    }
    struct Guard {
        service::IVideoFrameOSD& o;
        ~Guard() { o.EndOSD(); }
    } g{osd};
    media::Color lineColor{0, 0, 0xff};
    for (const auto& area : areas) {
        auto lines = cosmo::GetAreaLines(area, frame->GetWidth(), frame->GetHeight());
        if (!lines.empty()) {
            osd.OSDDrawLines(lines, lineColor, 3);
        }
    }
    const int w = frame->GetWidth();
    const int h = frame->GetHeight();
    const int x = std::max(10, w * 7 / 10);
    const int y = std::max(60, (h + 60) / 2);
    osd.OSDDrawTextEx(x, y, "进入 " + std::to_string(enter), {220, 231, 255}, 22, {0, 0, 0}, 0, true, 0);
    osd.OSDDrawTextEx(x, y + 40, "离开 " + std::to_string(leave), {220, 231, 255}, 22, {0, 0, 0}, 0, true, 0);
    return true;
}

}  // namespace cosmo
