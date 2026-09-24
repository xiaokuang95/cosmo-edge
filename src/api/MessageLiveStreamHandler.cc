// MessageLiveStreamHandler — Message Live Stream Handler implementation.

#include "api/MessageLiveStreamHandler.h"

#include <filesystem>
#include <fstream>

#include "flow/common/AreaLineUtil.h"
#include "flow/common/FlowTaskUtil.h"
#include "service/algorithm/IAlgorithmQuery.h"
#include "service/camera/ICameraChannelQuery.h"
#include "service/camera/ICameraDeviceCrud.h"
#include "service/media/IVideoFrameOSD.h"
#include "service/task/ITaskQuery.h"
#include "util/OsdApiKey.h"
#include "util/PassFlowOsdStore.h"
#include "util/PassFlowOsdDraw.h"
#include "service/network/IAuthService.h"
#include "util/IRequestDispatcher.h"
#include "util/UuidUtil.h"
#include "util/TimeUtil.h"
#include "media/Color.h"
#include "media/VideoFrame.h"
#include "flow/channel/AlgChannel.h"

#include "service/detail/ServiceRegistry.h"
#include "service/media/ILiveStreamService.h"
#include "util/ErrorCode.h"
#include "util/Log.h"

namespace cosmo {
MessageLiveStreamHandler::MessageLiveStreamHandler(service::ILiveStreamService& live_stream_service)
    : live_stream_service_(live_stream_service) {}

LiveStream::MsgRequestLiveStreamSend MessageLiveStreamHandler::Handle(
    LiveStream::MsgRequestLiveStreamRecv&& data, std::error_condition& errc) {
    LiveStream::MsgRequestLiveStreamSend retData{};
    errc = live_stream_service_.ViewerCreate(data.channelId, data.algorithmId, retData.resData.stream);
    return retData;
}

LiveStream::MsgStreamKeepAliveSend MessageLiveStreamHandler::Handle(LiveStream::MsgStreamKeepAliveRecv&& data,
                                                                    std::error_condition& errc) {
    LiveStream::MsgStreamKeepAliveSend retData{};
    errc = live_stream_service_.ViewerHeartBeat(data.channelId, data.algorithmId);
    return retData;
}

LiveStream::MsgStreamStopSend MessageLiveStreamHandler::Handle(LiveStream::MsgStreamStopRecv&& data,
                                                               std::error_condition& errc) {
    LiveStream::MsgStreamStopSend retData{};
    errc = live_stream_service_.ViewerDelete(data.channelId, data.algorithmId) ? util::ErrorEnum::Success
                                                                               : util::ErrorEnum::Failed;

    return retData;
}


// ── Third-party OSD snapshot / channel list (apiKey auth, not mtk) ──

namespace {
    bool OsdAuthValid(const std::string& presented) {
        if (cosmo::OsdApiKey::Valid(presented)) {
            return true;
        }
        if (presented.empty()) {
            return false;
        }
        auto& registry = cosmo::service::ServiceRegistry::Instance();
        return registry.Has<cosmo::service::IAuthService>() &&
               registry.Get<cosmo::service::IAuthService>().IsValidToken(presented);
    }

    void SetOsdAuthError(cosmo::MsgSendHead& ret, std::error_condition& errc) {
        errc            = cosmo::util::ErrorEnum::Failed;
        ret.resCode     = 0;
        ret.resMsg.clear();
        cosmo::MsgResBase msg;
        msg.msgCode  = "401";
        msg.msgText  = "invalid apiKey";
        ret.resMsg.push_back(msg);
    }

    std::string Base64Encode(const std::vector<uint8_t>& data) {
        static constexpr char tbl[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve((data.size() + 2) / 3 * 4);
        for (size_t i = 0; i < data.size(); i += 3) {
            unsigned v = static_cast<unsigned>(data[i]) << 16;
            const bool b1 = i + 1 < data.size();
            const bool b2 = i + 2 < data.size();
            if (b1) v |= static_cast<unsigned>(data[i + 1]) << 8;
            if (b2) v |= static_cast<unsigned>(data[i + 2]);
            out += tbl[(v >> 18) & 63];
            out += tbl[(v >> 12) & 63];
            out += b1 ? tbl[(v >> 6) & 63] : '=';
            out += b2 ? tbl[v & 63] : '=';
        }
        return out;
    }
}  // namespace

LiveStream::MsgListChannelsSend MessageLiveStreamHandler::Handle(LiveStream::MsgListChannelsRecv&& /*data*/, const RequestDispatchContext& context, std::error_condition& errc) {
    LiveStream::MsgListChannelsSend retData{};
    if (!OsdAuthValid(context.credential)) {
        SetOsdAuthError(retData, errc);
        return retData;
    }
    retData.resCode = 1;
    auto& registry  = service::ServiceRegistry::Instance();
    size_t total = 0;
    auto  cameras   = registry.Get<service::ICameraDeviceCrud>().Query("", -1, 1, 500, total);
    LOG_INFO("ListChannels total:{} cameras:{}", total, cameras.size());
    for (const auto& cam : cameras) {
        LOG_INFO("ListChannels cam:{} alg:{}", cam.videoChannelId, cam.taskList.size());
        LiveStream::MsgListChannelsRow row;
        row.channelId   = cam.videoChannelId;
        row.channelName = cam.channelName;
        for (const auto& task : cam.taskList) {
            LiveStream::MsgListChannelsAlgorithm alg;
            alg.algorithmId   = task.algorithmId;
            alg.algorithmName = task.algorithmName;
            alg.enableStatus  = (task.status == 1) ? 1 : 0;
            row.algorithms.push_back(alg);
        }
        retData.resData.rows.push_back(row);
    }
    errc = cosmo::util::ErrorEnum::Success;
    return retData;
}

LiveStream::MsgGetOsdPictureSend MessageLiveStreamHandler::Handle(LiveStream::MsgGetOsdPictureRecv&& data, const RequestDispatchContext& context, std::error_condition& errc) {
    LiveStream::MsgGetOsdPictureSend retData{};
    if (!OsdAuthValid(context.credential)) {
        SetOsdAuthError(retData, errc);
        return retData;
    }
    auto& registry = service::ServiceRegistry::Instance();
    auto  channel  = registry.Get<service::ICameraChannelQuery>().GetChannelInst(data.channelId);
    if (!channel) {
        errc          = cosmo::util::ErrorEnum::Failed;
        retData.resCode = 0;
        cosmo::MsgResBase msg;
        msg.msgCode = "404";
        msg.msgText = "channel not found";
        retData.resMsg.push_back(msg);
        return retData;
    }

    auto frame = channel->CaptureImage(3000);
    if (!::VideoFrameValid(frame)) {
        errc          = cosmo::util::ErrorEnum::Failed;
        retData.resCode = 0;
        cosmo::MsgResBase msg;
        msg.msgCode = "504";
        msg.msgText = "capture timeout";
        retData.resMsg.push_back(msg);
        return retData;
    }

    const std::string taskId = cosmo::ChannelAlgIdToTaskId(data.channelId, data.algorithmId);
    cosmo::MsgTaskConfig params;
    registry.Get<service::ITaskQuery>().GetTaskParam(data.channelId, taskId, params);
    auto totals = ::PassFlowOsdStore::Get(taskId);
    cosmo::PassFlowOsdDraw(frame, params.areas, totals.first, totals.second);

    auto jpeg = registry.Get<service::ICameraChannelQuery>().EncodeJpeg(frame);
    if (jpeg.empty()) {
        errc          = cosmo::util::ErrorEnum::Failed;
        retData.resCode = 0;
        cosmo::MsgResBase msg;
        msg.msgCode = "500";
        msg.msgText = "encode failed";
        retData.resMsg.push_back(msg);
        return retData;
    }

    // Persist under web-accessible temp dir.
    const int64_t nowMs  = cosmo::util::GetMilliseconds();
    std::string   webDir = registry.Get<service::ICameraChannelQuery>().GetWebLocalPath(nowMs);
    std::string   webUrl = registry.Get<service::ICameraChannelQuery>().GetWebAccessPath(nowMs);
    std::filesystem::create_directories(webDir);
    const std::string fname = cosmo::util::GenerateUUID() + ".jpg";
    const std::string fpath = (std::filesystem::path(webDir) / fname).string();
    {
        std::ofstream ofs(fpath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    }
    retData.resData.url = (std::filesystem::path(webUrl) / fname).string();
    // nginx serves /web/... directly; fullUrl left relative-friendly.
    retData.resData.fullUrl = retData.resData.url;
    if (data.base64 != "0") {
        retData.resData.pictureBase64 = Base64Encode(jpeg);
    }
    retData.resCode = 1;
    errc            = cosmo::util::ErrorEnum::Success;
    return retData;
}

}  // namespace cosmo
