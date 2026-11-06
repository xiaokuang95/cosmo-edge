// AiTrackerArea.cc — Target area marking and break-line detection for AiTracker.
// Split from AiTracker.cc to reduce file size (DEBT-007).

#include <algorithm>
#include <limits>

#include "flow/common/AlgDataRecord.h"
#include "flow/track/AiTracker.h"
#include "flow/track/AiTrackerTypes.h"
#include "util/GeometricCalculation.h"
#include "util/GeometricPos.h"
#include "util/Log.h"
#include "util/UuidUtil.h"

static constexpr const char* kTag = "AI-TRACKER ";
namespace cosmo {

TargetPosition AiTracker::GetTaskLabelPos(const std::string& label,
                                          const std::vector<AiLabelParam>& labelParams) {
    auto it = std::find_if(labelParams.begin(), labelParams.end(),
                           [&label](const auto& lp) { return lp.label == label; });
    if (it != labelParams.end()) {
        return it->pos;
    }

    return TargetPosition::kBottom;  // Default
}

void AiTracker::TargetAddArea(AiDetectRstEl& target, TargetPosition pos, TargetAreaType type,
                              const MsgTaskArea& area, bool bAssociatedArea, const std::string& mainAreaId) {
    if ((pic_width_ <= 0) || (pic_height_ <= 0)) {
        return;
    }
    if (BoxInArea(target.box, pos, target.point, area.points, pic_width_, pic_height_)) {
        TargetAreaUnit targetArea;
        targetArea.area_id            = area.areaId;
        targetArea.area_name          = area.name;
        targetArea.is_associated_area = bAssociatedArea;
        targetArea.main_area_id       = mainAreaId;
        targetArea.position           = pos;
        targetArea.type               = type;
        target.targetPos              = pos;
        if (TargetAreaType::kNormal == type) {
            target.areaSign.areas.push_back(targetArea);
        } else {
            target.areaSign.shielded_areas.push_back(targetArea);
        }
    }
    return;
}

void AiTracker::TargetAddLine(AiDetectRstEl& target, TargetPosition pos, const MsgTaskArea& area,
                              bool bAssociatedArea, const std::string& mainAreaId) {
    if ((pic_width_ <= 0) || (pic_height_ <= 0)) {
        return;
    }
    TargetLineUnit targetonLineUnit;
    targetonLineUnit.area_id            = area.areaId;
    targetonLineUnit.area_name          = area.name;
    targetonLineUnit.is_associated_area = bAssociatedArea;
    targetonLineUnit.main_area_id       = mainAreaId;
    targetonLineUnit.position           = pos;
    targetonLineUnit.type =
        BoxOnLinePos(target.box, pos, target.point, area.linePoints, pic_width_, pic_height_);
    target.targetPos = pos;
    target.areaSign.lines.push_back(targetonLineUnit);

    return;
}
// Mark area info on target
void AiTracker::SignTargetAreas(AlgDataPtr dataPtr) {
    if (!dataPtr) {
        return;
    }

    if (!dataPtr->chanDataDec.frame) {
        return;
    }

    if (!dataPtr->GetTaskResult(AlgDataType::TaskDataTrack)) {
        return;
    }

    auto detRet = dataPtr->GetTaskResult(AlgDataType::TaskDataTrack);
    pic_width_  = dataPtr->chanDataDec.frame->GetWidth();
    pic_height_ = dataPtr->chanDataDec.frame->GetHeight();
    std::shared_lock<std::shared_mutex> lock(mtx);
    if (task_area_.areas.size()) {
        detRet->bHaveArea = true;
    }
    if (task_area_.shieldedAreas.size()) {
        detRet->bHaveShieldedArea = true;
    }

    for (auto& target : detRet->targets) {
        auto pos = GetTaskLabelPos(target.confidence.label, label_param_);
        for (auto area : task_area_.areas) {
            if (!area.points.empty()) {
                TargetAddArea(target, pos, TargetAreaType::kNormal, area, false, "");
            }
            if (!area.linePoints.empty()) {
                TargetAddLine(target, pos, area, false, "");
            }
            // Associated area
            for (auto assArea : area.associatedAreas) {
                if (!assArea.points.empty()) {
                    TargetAddArea(target, pos, TargetAreaType::kNormal, assArea, true, area.areaId);
                }
                if (!assArea.linePoints.empty()) {
                    TargetAddLine(target, pos, assArea, true, area.areaId);
                }
            }
        }

        for (auto shieldedArea : task_area_.shieldedAreas) {
            if (!shieldedArea.points.empty()) {
                TargetAddArea(target, pos, TargetAreaType::kShield, shieldedArea, false, "");
            }
            // Associated area
            for (auto assArea : shieldedArea.associatedAreas) {
                if (!assArea.points.empty()) {
                    TargetAddArea(target, pos, TargetAreaType::kShield, assArea, true, shieldedArea.areaId);
                }
                if (!assArea.linePoints.empty()) {
                    TargetAddLine(target, pos, assArea, true, shieldedArea.areaId);
                }
            }
        }
    }

    return;
}

void AiTracker::TargetAddHistory(AlgDataPtr algData) {
    if (!algData->GetTaskResult(AlgDataType::TaskDataTrack)) {
        return;
    }

    for (auto& target : algData->GetTaskResult(AlgDataType::TaskDataTrack)->targets) {
        auto& idData = map_track_id_status_[target.trackId];
        if (idData.track_id != target.trackId) {
            idData.track_id_info = util::GenerateUUID();
            idData.track_id      = target.trackId;
            LOG_AITARGET("{}[{} {}] TrackId {}/{} Appear At Frame:{}", kTag, name_, uuid, target.trackId,
                         idData.track_id_info, frame_index_);
        }

        if (target.box.height > 0) {
            idData.hw_ratios.push(target.hwRatio);
        }
        while (idData.hw_ratios.size() > static_cast<size_t>(param_.motion.frames)) {
            idData.hw_ratios.pop();
        }
        target.hwRatioVariation = CalculateNormalizedVariation(idData.hw_ratios);
        if (target.hwRatioVariation > shape_change_threshold_) {
            target.shapeChangeStatus = AIShapeChangeState::CHANGE;
        } else {
            target.shapeChangeStatus = AIShapeChangeState::STILL;
        }

        target.trackIdInfo = idData.track_id_info;
        idData.detected    = true;
        idData.det_el      = target;
    }
}

void AiTracker::TargetOldHistory(AlgDataPtr /*algData*/) {
    // Keep line side/state across short detection gaps, like the tracker does.
    constexpr int kKeepLostTrackFrames = 40;
    for (auto it = map_track_id_status_.begin(); it != map_track_id_status_.end();) {
        if (!it->second.detected) {
            ++it->second.missed_frames;
            if (it->second.missed_frames > kKeepLostTrackFrames) {
                LOG_AITARGET("{}[{} {}] drop track {} last box:{}x{} {}x{} missed:{}", kTag, name_, uuid,
                         it->first, it->second.det_el.box.x, it->second.det_el.box.y,
                         it->second.det_el.box.width, it->second.det_el.box.height,
                         it->second.missed_frames);
                it = map_track_id_status_.erase(it);
            } else {
                ++it;
            }
        } else {
            it->second.missed_frames = 0;
            it->second.detected      = false;
            ++it;
        }
    }
}

TargetLineUnit AiTracker::TargetOnLinePos(const AiDetectRstEl& target, const std::string& areaId) {
    auto it = std::find_if(target.areaSign.lines.begin(), target.areaSign.lines.end(),
                           [&areaId](const auto& line) { return line.area_id == areaId; });
    if (it != target.areaSign.lines.end()) {
        return *it;
    }

    return {};
}

void AiTracker::TargetCalcBreakLineArea(TrackIdData& idData, const MsgTaskArea& area) {
    auto& targetLineData           = idData.line_datas[area.areaId];
    targetLineData.break_line_type = TargetBreakLineType::kNone;
    // Predicted (LOSS) boxes are guesses; only real detections move the line state.
    if (idData.det_el.bFilter || AITrackingStatus::TRACKING != idData.det_el.trackStatus) {
        return;
    }
    // Enter shielded area
    if (!idData.det_el.areaSign.shielded_areas.empty()) {
        targetLineData.target_pos_now = TargetPositionOnLineType::kUnknown;
        LOG_AITARGET("{}[{} {}] TrackId {} Entered shielded area", kTag, name_, uuid, idData.track_id);
        return;
    }

    for (auto& assArea : area.associatedAreas) {
        TargetCalcBreakLineArea(idData, assArea);
    }

    // Status of the latest point
    auto targetInfo   = TargetOnLinePos(idData.det_el, area.areaId);
    auto targetPosNow = targetInfo.type;
    if (TargetPositionOnLineType::kOnLine == targetPosNow) {
        // Point is on the line, does not cross
        return;
    }

    // A crossing must clear a band around the line and hold for two frames.
    // Box-height-proportional, so it works at any resolution; stops foot jitter
    // on the line from counting a pass, and a new track id born on the line
    // from counting at all (its side is learned once it leaves the band).
    if (area.linePoints.size() >= 2) {
        cosmo::util::Point point_px;
        auto point = GetMsgPointFromRect(idData.det_el.box, targetInfo.position, point_px, pic_width_, pic_height_);
        double dist = std::numeric_limits<double>::max();
        for (size_t i = 1; i < area.linePoints.size(); ++i) {
            dist = std::min(dist, static_cast<double>(
                                       cosmo::util::PointLineDistance(point, area.linePoints[i - 1],
                                                                      area.linePoints[i])));
        }
        const double h      = std::max(1, pic_height_);
        const double margin = std::max(6.0 / h, 0.1 * idData.det_el.box.height / h);
        if (dist < margin) {
            return;
        }
        if (targetLineData.last_outside_pos != targetPosNow) {
            targetLineData.last_outside_pos = targetPosNow;
            return;
        }
    }

    // Latest point has same relative position as the last point
    if (targetLineData.target_pos_now == targetPosNow) {
        return;
    }

    if (TargetPositionOnLineType::kLeft == targetPosNow) {
        auto point = GetMsgPointFromRect(idData.det_el.box, targetInfo.position, idData.det_el.point,
                                         pic_width_, pic_height_);
        LOG_AITARGET(
            "{}[{} {}] TrackId {} on line {}/{} left side of, point:{:02f}.{:02f} {}.{} {}x{} {} {}x{} {} {}",
            kTag, name_, uuid, idData.track_id, area.areaId, area.name, point.x, point.y, idData.det_el.box.x,
            idData.det_el.box.y, idData.det_el.box.width, idData.det_el.box.height, targetInfo.position,
            pic_width_, pic_height_, idData.det_el.motionStatus, idData.det_el.trackStatus);
    }
    if (TargetPositionOnLineType::kRight == targetPosNow) {
        auto point = GetMsgPointFromRect(idData.det_el.box, targetInfo.position, idData.det_el.point,
                                         pic_width_, pic_height_);
        LOG_AITARGET(
            "{}[{} {}] TrackId {} on line {}/{} right side of, point:{:02f}.{:02f} {}.{} {}x{} {} {}x{} {} "
            "{}",
            kTag, name_, uuid, idData.track_id, area.areaId, area.name, point.x, point.y, idData.det_el.box.x,
            idData.det_el.box.y, idData.det_el.box.width, idData.det_el.box.height, targetInfo.position,
            pic_width_, pic_height_, idData.det_el.motionStatus, idData.det_el.trackStatus);
    }

    const bool two_way = DirectionType::DirectionTypeTwoWay == area.iderectionType;
    // Forward is the left side of the drawn line, not the camera.
    auto type = CalcBreakLine(targetLineData.target_pos_now, targetPosNow, two_way);
    type      = KeepFirstDirection(targetLineData.counted_dir, type);
    if (type != TargetBreakLineType::kNone) {
        LOG_INFO("{}[{} {}] TrackId {} {}crossed line {}/{} prev:{} now:{} box:{}x{} {}x{} At {}", kTag, name_, uuid,
                 idData.track_id, type == TargetBreakLineType::kPos ? "forward" : "reverse", area.areaId,
                 area.name, static_cast<int>(targetLineData.target_pos_now), static_cast<int>(targetPosNow),
                 idData.det_el.box.x, idData.det_el.box.y, idData.det_el.box.width, idData.det_el.box.height,
                 frame_index_);
        targetLineData.break_line_type = type;
    }
    targetLineData.target_pos_now = targetPosNow;
    targetLineData.det_el         = idData.det_el;
}

void AiTracker::TargetCalcBreakLine(AlgDataPtr /*algData*/) {
    if (!area_have_line_) {
        return;
    }

    std::shared_lock<std::shared_mutex> lock(mtx);  // Lock task_area_
    for (auto it = map_track_id_status_.begin(); it != map_track_id_status_.end(); it++) {
        for (auto& area : task_area_.areas) {
            auto& idData = it->second;
            if (idData.missed_frames > 0 || idData.det_el.bFilter) {
                continue;
            }
            TargetCalcBreakLineArea(idData, area);
        }
    }
}

// Copy crossing status to data stream
void AiTracker::CpBreakLineStatusToData(AlgDataPtr algData) {
    if (!area_have_line_) {
        return;
    }

    for (auto& target : algData->GetTaskResult(AlgDataType::TaskDataTrack)->targets) {
        auto& idData = map_track_id_status_[target.trackId];

        for (auto& linestatus : target.areaSign.lines) {
            auto& targetLineData = idData.line_datas[linestatus.area_id];
            linestatus.status    = targetLineData.break_line_type;
        }
    }
}
}  // namespace cosmo
