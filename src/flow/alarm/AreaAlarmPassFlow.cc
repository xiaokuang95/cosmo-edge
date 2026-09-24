// AreaAlarmPassFlow.cc — Count report + pass-flow statistics.
// Implementation partition of AreaAlarm (declared in flow/alarm/AreaAlarm.h).

#include "flow/alarm/AreaAlarm.h"
#include "flow/alarm/AreaAlarmInternalTypes.h"
#include "util/AiTypes.h"
#include "util/DateTimeFormat.h"
#include "util/Log.h"

#include <algorithm>
#include <set>

static constexpr const char* kTag = "AreaAlarm ";

namespace chrono = std::chrono;
namespace cosmo {

// Area target count report
void AreaAlarm::HandAreaTargetCountReport(AlgDataPtr algData, DataDetTrackClassifyPtr input) {
    auto now                          = chrono::steady_clock::now();
    constexpr uint64_t kMinDurationMs = 1000;
    auto areaDuration = params_.area_duration > kMinDurationMs ? params_.area_duration : kMinDurationMs;
    // Report interval
    if (chrono::duration_cast<chrono::milliseconds>(now - report_time_point_).count() <
        static_cast<int64_t>(areaDuration)) {
        return;
    }

    report_time_point_ = now;

    std::map<std::string, DataAlarmUnit> alarms;
    {
        // Report even when no targets are present
        std::shared_lock<std::shared_mutex> lock(mtx);
        for (auto& pair : area_target_status_map_) {
            auto& alarm    = alarms[pair.second.area_id];
            alarm.areaId   = pair.second.area_id;
            alarm.areaName = pair.second.area_name;
        }
    }

    for (auto& target : input->targets) {
        // In shielded area
        if (!target.areaSign.shielded_areas.empty()) {
            continue;
        }
        // Filtered target
        if (target.bFilter) {
            continue;
        }
        for (const auto& targetArea : target.areaSign.areas) {
            auto& alarm        = alarms[targetArea.area_id];
            alarm.flowActionId = action_info_.flowActionId;
            alarm.reportType   = OnEventsReportType::Realtime;
            alarm.areaId       = targetArea.area_id;
            alarm.areaName     = targetArea.area_name;
            alarm.box          = target.box;
            alarm.boxs.push_back(target.box);
        }
    }

    // Target count change detection
    if (params_.target_count_change) {
        std::lock_guard<std::shared_mutex> lock(mtx);
        for (auto& pair : alarms) {
            auto& areaTargetStatus = area_target_status_map_[pair.second.areaId];
            auto targetSize        = pair.second.boxs.size();
            if (targetSize != areaTargetStatus.target_count) {
                areaTargetStatus.target_count = targetSize;
                pair.second.statusChange      = true;  // Status change flag
            }
        }
    }

    for (auto it = alarms.begin(); it != alarms.end(); it++) {
        FillAlarmData(algData, it->second);
    }
}

// Target line-crossing count calculation
void AreaAlarm::PassFlowCount(PassFlowAreaTargets& areaData, const std::deque<AiDetectRstEl>& history,
                              const std::string& areaId) {
    int passCount = 0;  // Forward crossing: +1, reverse crossing: -1
    int trackId   = -1;
    for (auto& target : history) {
        trackId = target.trackId;
        for (auto& targetLine : target.areaSign.lines) {
            if (areaId != targetLine.area_id) {
                continue;
            }

            if (TargetBreakLineType::kPos == targetLine.status) {
                passCount += 1;
            } else if (TargetBreakLineType::kNeg == targetLine.status) {
                passCount -= 1;
            }
        }
    }

    if ((passCount > 1) || (passCount < -1)) {
        LOG_WARN("{}[{}] {}/{} trackId:{} Have {} Pass Line :{}", kTag, task_id, action_info_.actionName,
                 action_info_.flowActionId, trackId, passCount, areaId);
    }
    if (passCount > 0) {
        areaData.enter_org_num += 1;
    } else if (passCount < 0) {
        areaData.leave_org_num += 1;
    }
}

bool AreaAlarm::UseDualLineAnd() const {
    return params_.dual_line_and && pass_flow_areas_map_.size() >= 2;
}

void AreaAlarm::ResolveDualLineIds(std::string& checkId, std::string& shaftId) {
    checkId.clear();
    shaftId.clear();
    std::vector<std::pair<double, std::string>> byY;
    for (const auto& area : task_area_.areas) {
        if (area.name.find("检查") != std::string::npos) {
            checkId = area.areaId;
        }
        if (area.name.find("入井") != std::string::npos) {
            shaftId = area.areaId;
        }
        double y = 0;
        int n = 0;
        for (const auto& p : area.linePoints) {
            y += p.y;
            ++n;
        }
        if (n > 0) {
            byY.push_back({y / n, area.areaId});
        }
    }
    if (!checkId.empty() && !shaftId.empty() && checkId != shaftId) {
        return;
    }
    std::sort(byY.begin(), byY.end());
    if (byY.size() >= 2) {
        checkId = byY.front().second;
        shaftId = byY.back().second;
    }
}

AreaAlarm::PassFlowAreaTargets* AreaAlarm::DualLinePrimary() {
    std::string checkId;
    std::string shaftId;
    ResolveDualLineIds(checkId, shaftId);
    const std::string& id = !shaftId.empty() ? shaftId : pass_flow_areas_map_.begin()->first;
    auto it = pass_flow_areas_map_.find(id);
    if (it == pass_flow_areas_map_.end()) {
        return &pass_flow_areas_map_.begin()->second;
    }
    return &it->second;
}

void AreaAlarm::DualLineClearTrackHistory(unsigned trackId) {
    for (auto& pair : pass_flow_areas_map_) {
        auto it = pair.second.target_map.find(trackId);
        if (it != pair.second.target_map.end()) {
            it->second.history.clear();
        }
    }
}

void AreaAlarm::DualLineCount(PassFlowAreaTargets& areaData, const std::deque<AiDetectRstEl>& history) {
    std::string checkId;
    std::string shaftId;
    ResolveDualLineIds(checkId, shaftId);
    if (checkId.empty() || shaftId.empty() || checkId == shaftId) {
        PassFlowCount(areaData, history, areaData.area_id);
        return;
    }
    std::vector<std::string> seq;
    std::set<std::string> seen;
    for (const auto& target : history) {
        for (const auto& line : target.areaSign.lines) {
            if (TargetBreakLineType::kNone == line.status) {
                continue;
            }
            if (line.area_id != checkId && line.area_id != shaftId) {
                continue;
            }
            if (seen.insert(line.area_id).second) {
                seq.push_back(line.area_id);
            }
        }
    }
    if (seq.size() < 2) {
        return;
    }
    if (seq[0] == checkId && seq[1] == shaftId) {
        areaData.enter_org_num += 1;
    } else if (seq[0] == shaftId && seq[1] == checkId) {
        areaData.leave_org_num += 1;
    }
}

void AreaAlarm::HandPassFlowCalc() {
    if (UseDualLineAnd()) {
        auto* primary = DualLinePrimary();
        for (auto& targetPair : primary->target_map) {
            if (!TargetInArea(targetPair.second.target, primary->associated_area)) {
                DualLineCount(*primary, targetPair.second.history);
                DualLineClearTrackHistory(targetPair.first);
            }
        }
        return;
    }
    for (auto& pair : pass_flow_areas_map_) {
        for (auto& targetPair : pair.second.target_map) {
            // Target left the area or trackId disappeared
            if (!TargetInArea(targetPair.second.target, pair.second.associated_area)) {
                PassFlowCount(pair.second, targetPair.second.history, pair.second.area_id);
                // Counting complete — clear history data
                targetPair.second.history.clear();
            }
        }
    }
}

void AreaAlarm::HandPassFlowAddHistory(AlgDataPtr /*algData*/, DataDetTrackClassifyPtr input) {
    for (auto& pair : pass_flow_areas_map_) {
        for (auto& target : input->targets) {
            auto& trackIdData    = pair.second.target_map[target.trackId];
            trackIdData.track_id = target.trackId;
            trackIdData.detected = true;
            trackIdData.target   = target;
            trackIdData.history.push_back(target);
        }
    }
}

void AreaAlarm::HandPassFlowOldHistory(bool bCalcPassflow) {
    if (UseDualLineAnd()) {
        auto* primary = DualLinePrimary();
        for (auto it = primary->target_map.begin(); it != primary->target_map.end();) {
            if (!it->second.detected) {
                if (bCalcPassflow) {
                    DualLineCount(*primary, it->second.history);
                }
                const unsigned tid = it->first;
                it = primary->target_map.erase(it);
                for (auto& pair : pass_flow_areas_map_) {
                    if (&pair.second == primary) {
                        continue;
                    }
                    pair.second.target_map.erase(tid);
                }
            } else {
                it->second.detected = false;
                ++it;
            }
        }
        for (auto& pair : pass_flow_areas_map_) {
            if (&pair.second == primary) {
                continue;
            }
            for (auto& t : pair.second.target_map) {
                t.second.detected = false;
            }
        }
        return;
    }
    for (auto& pair : pass_flow_areas_map_) {
        for (auto itAreaTargets = pair.second.target_map.begin();
             itAreaTargets != pair.second.target_map.end();) {
            if (!itAreaTargets->second.detected) {
                if (bCalcPassflow) {
                    PassFlowCount(pair.second, itAreaTargets->second.history, pair.second.area_id);
                }

                itAreaTargets = pair.second.target_map.erase(itAreaTargets);
            } else {
                itAreaTargets->second.detected = false;
                ++itAreaTargets;
            }
        }
    }
}

// Report statistics to alarm module; reset enterNumber/leaveNumber on hour change
void AreaAlarm::HandPassFlowReport(AlgDataPtr algData) {
    auto now                          = chrono::steady_clock::now();
    constexpr uint64_t kMinDurationMs = 1000;
    auto areaDuration = params_.area_duration > kMinDurationMs ? params_.area_duration : kMinDurationMs;
    // Report interval
    if (chrono::duration_cast<chrono::milliseconds>(now - report_time_point_).count() <
        static_cast<int64_t>(areaDuration)) {
        return;
    }

    report_time_point_ = now;

    auto sendTime = util::GetCurrentDateTime().ToYMDHMSInt();
    auto hour     = sendTime / 10000;
    for (auto& pair : pass_flow_areas_map_) {
        if (0 == pair.second.hour) {
            pair.second.hour = hour;
        }
        pair.second.enter_number    = pair.second.enter_number + pair.second.enter_org_num;
        pair.second.leave_number    = pair.second.leave_number + pair.second.leave_org_num;
        pair.second.enter_total_num = pair.second.enter_total_num + pair.second.enter_org_num;
        pair.second.leave_total_num = pair.second.leave_total_num + pair.second.leave_org_num;

        DataAlarmUnit alarmUnit;
        alarmUnit.flowActionId = action_info_.flowActionId;
        alarmUnit.reportType   = OnEventsReportType::Realtime;
        alarmUnit.areaId       = pair.second.area_id;
        alarmUnit.areaName     = pair.second.area_name;

        alarmUnit.passFlowData.enterNumber   = pair.second.enter_number;
        alarmUnit.passFlowData.leaveNumber   = pair.second.leave_number;
        alarmUnit.passFlowData.enterOrgNum   = pair.second.enter_org_num;
        alarmUnit.passFlowData.leaveOrgNum   = pair.second.leave_org_num;
        alarmUnit.passFlowData.enterTotalNum = pair.second.enter_total_num;
        alarmUnit.passFlowData.leaveTotalNum = pair.second.leave_total_num;
        alarmUnit.passFlowData.time          = std::to_string(pair.second.hour);
        alarmUnit.passFlowData.timeSec       = std::to_string(sendTime);

        pair.second.enter_org_num = 0;
        pair.second.leave_org_num = 0;
        FillAlarmData(algData, alarmUnit);
        if (pair.second.hour != hour)  // New hour — reset counters
        {
            pair.second.hour          = hour;
            pair.second.enter_number  = 0;
            pair.second.leave_number  = 0;
            pair.second.enter_org_num = 0;
            pair.second.leave_org_num = 0;
        }
    }
}

// kTargetCountReport — target count / area count / pass-flow
void AreaAlarm::HandAreaTargetCount(AlgDataPtr algData, DataDetTrackClassifyPtr input) {
    if (BreakAreaType::kIn == break_area_type_) {
        // Area target count report
        HandAreaTargetCountReport(algData, input);
    } else if (BreakAreaType::kLineBothCalc == break_area_type_) {
        // Pass-flow / traffic-flow
        HandPassFlowAddHistory(algData, input);
        HandPassFlowOldHistory();
        HandPassFlowCalc();
        HandPassFlowReport(algData);
    }
}

}  // namespace cosmo
