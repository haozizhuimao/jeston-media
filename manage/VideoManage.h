#pragma once


#include "EncodeVideoOperate.h"
#include "tis_global/struct_details/camera_struct.h"
#include <functional>
#include <thread>
#include <string>
#include <atomic>
#include <vector>
#include "kits/utility/container/BlockingQueue.h"

namespace _Kits
{
    using AlgorithPushCallback = std::function<void(std::shared_ptr<TIS_Info::ImageBuffer>)>;
    // using SendToUICallback = std::function<void(std::shared_ptr<TIS_Info::ImageBuffer>)>;
    class VideoManage
    {
    private:
        /* data */
    public:
        VideoManage(const std::string& name);
        ~VideoManage();
        void init(const TIS_Info::EncoderInfo& encoderinfo, const TIS_Info::SubtitleInputInfo& subtitleinfo);
        void start();
        void setVideoSaveRootPath(const std::string& path);
        void pushImage(std::shared_ptr<TIS_Info::ImageBuffer> imagebuffer);
        void updateSubtitles(const std::string& subtitles);
        void updataVideoName(const std::string& name);
        void stop();
        bool isInit() const;
        void setLiveStreamCallBack(SendLiveStreamCallBack callback);

    private:
        void run();
        void createDirectory(const std::string &strpath);
    private:

        std::string m_name = "video";
        std::string m_videoRootPath = "";
        std::thread m_dealThread;
        std::string m_videoSaveRootPath = "";
        std::atomic_bool m_isRun = false;
        BlockingQueue<std::shared_ptr<TIS_Info::ImageBuffer>> m_imagebufferQueue {20};
        std::unique_ptr<EncodeVideoOperate> m_encodeVideoOperate = nullptr;
        std::atomic_bool m_isInit {false};
        TIS_Info::EncoderInfo m_encoderInfo;
      
    };
         
}