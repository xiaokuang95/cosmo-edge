#include "util/PassFlowOsdDraw.h"

#include <algorithm>
#include <string>

#include "flow/common/AreaLineUtil.h"
#include "media/Color.h"
#include "service/detail/ServiceRegistry.h"
#include "service/media/IVideoFrameOSD.h"
#include "util/PassFlowOsdConfig.h"

namespace cosmo {
namespace {

size_t Utf8CharCount(const std::string& s) {
    size_t count = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        if (c < 0x80)
            i += 1;
        else if ((c >> 5) == 0x6)
            i += 2;
        else if ((c >> 4) == 0xe)
            i += 3;
        else if ((c >> 3) == 0x1e)
            i += 4;
        else
            i += 1;
        count++;
    }
    return count;
}

}  // namespace

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
    // Mirror StreamViewerDraw + FrameOverview so snapshot/RTSP/alarm JPEG share one layout.
    const auto cfg      = PassFlowOsdConfig::Snapshot();
    const int  w        = frame->GetWidth();
    const int  h        = frame->GetHeight();
    const int  fontSize = cfg.fontSize;
    const int  lineDiff = fontSize * 2 + 32;
    const std::string t1 = cfg.enterLabel + " " + std::to_string(enter);
    const std::string t2 = cfg.leaveLabel + " " + std::to_string(leave);
    const int maxChars   = static_cast<int>(std::max(Utf8CharCount(t1), Utf8CharCount(t2)));
    const int maxTextLen = (fontSize < 10) ? maxChars * 11 : (fontSize / 10) * 11 * maxChars;
    int x = std::max(10, static_cast<int>(w * cfg.xRatio));
    int y = std::max(60, static_cast<int>(h * cfg.yRatio));
    const int totalH = 2 * lineDiff;
    if (y > totalH) {
        y -= totalH;
    }
    if (x + maxTextLen >= w) {
        x = w - maxTextLen - 10;
    }
    if (x < 0 || y < 0) {
        return true;
    }
    const media::Color textColor = cfg.TextColor();
    osd.OSDDrawTextEx(x, y, t1, textColor, fontSize, {0, 0, 0}, 0, true, 0);
    osd.OSDDrawTextEx(x, y + lineDiff, t2, textColor, fontSize, {0, 0, 0}, 0, true, 0);
    return true;
}

}  // namespace cosmo
