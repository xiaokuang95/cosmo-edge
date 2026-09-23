#pragma once

#include <string>

#include "media/VideoFrame.h"
#include "util/dto/TaskAreaTypes.h"

namespace cosmo {

// Draw pass-flow tripwires and 进入/离开 counters onto the frame in-place.
// Returns false if OSD session could not begin.
bool PassFlowOsdDraw(VideoFramePtr frame, const std::vector<MsgTaskArea>& areas, int enter, int leave);

}  // namespace cosmo
