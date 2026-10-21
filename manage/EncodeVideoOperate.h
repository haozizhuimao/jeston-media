#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include "tis_global/struct_details/camera_struct.h"
#include "tis_global/struct_details/encode_input_struct.h"
#include "kits/common/video_package/encode/Encodevideoffmpeg.h"
#include "Muxingvideo.h"
#include <functional>
#include "kits/utility/container/BlockingQueue.h"
#if defined(__linux__)
// 统一判断：是否是 ARM（32位或64位）
#if defined(__arm__) || defined(__aarch64__)
#define ARM_PLATFORM
#include "kits/common/video_package/jeston_media/JestonEncode.h"
#endif
#endif

namespace _Kits {
using SendLiveStreamCallBack = std::function<void(const uint8_t *, const int)>;
class EncodeVideoOperate
{
  public:
    EncodeVideoOperate();
    ~EncodeVideoOperate();
    void initEncoderInfo(const TIS_Info::EncoderInfo &encoderinfo, const TIS_Info::SubtitleInputInfo &subtitleinfo);
    void saveImage(std::shared_ptr<TIS_Info::ImageBuffer> imagebuffer);
    void startRecord(const std::string &savePath);
    void stopRecord();
    void updateSubtitles(const std::string &subtitles);
    void initLiveStreamerCallback(SendLiveStreamCallBack callback);
    void setEncodeVideoFlag(bool flag)
    {
        m_encodeVideFlag.store(flag);
    }

  private:
    void encodeThreadFunc();
    void writeVideofunc(AVPacket &pkt);
    void writeVideofunctest(int no, AVPacket &pkt, int flag);

  private:
    BlockingQueue<std::shared_ptr<TIS_Info::ImageBuffer>> m_imagebufferQueue{50};
    std::thread m_encodeThread;
    std::atomic_bool m_bRunFlag{false};
    std::atomic_bool m_bSaveVideoFlag{false};
    std::atomic_bool m_encodeVideFlag{false};
    std::unique_ptr<_Kits::MuxingVideo> m_muxingVideo = nullptr;
    std::unique_ptr<_Kits::FFMPEGEncodeVideo> m_encodeVideo = nullptr;
    TIS_Info::EncoderInfo m_encoderInfo;
    std::string m_currentSubtitles{""};
    SendLiveStreamCallBack m_sendLiveStreamCallback = nullptr;
#ifdef ARM_PLATFORM
    std::unique_ptr<JestonEncode> m_jestonEncode = nullptr;
    bool m_first = true;
#endif
};
} // namespace _Kits