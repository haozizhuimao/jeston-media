#pragma once
extern "C"
{
    #include  "ffmpeg\libavformat\avformat.h"
    #include  "ffmpeg\libavcodec\avcodec.h"
    #include  "ffmpeg/libswscale/swscale.h"
    #include  "ffmpeg/libavutil/imgutils.h"
}

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include "kits/utility/object_pool/ObjectPool.h"
#include "tis_global/Struct_details/camera_struct.h"

namespace _Kits {
class FFmpegPuller
{
  public:
    using FrameCallback = std::function<void(std::shared_ptr<TIS_Info::ImageBuffer>)>;

    FFmpegPuller();
    ~FFmpegPuller();

    // 设置回调：每获取到一帧视频，就回调
    void setFrameCallback(FrameCallback cb);

    // 打开流（例如 rtsp://xxx / rtmp://xxx / http://xxx）
    
    bool open(const std::string& url,const std::string& cameraname);

    // 开始解码线程
    void start();

    // 停止线程并释放资源
    void stop();

  private:
    void run();
    void avframeToImageBuffer(const AVFrame *frame);

private:
    std::string m_url {""};
    std::string m_cameraname {""};
    std::thread m_thread;
    std::atomic_bool m_running{false};
    std::atomic_bool m_startFlag{false};
    FrameCallback m_callback = nullptr;

    AVFormatContext *m_fmt_ctx = nullptr;
    AVCodecContext *m_codec_ctx = nullptr;
    int m_video_stream_index = -1;
    std::shared_ptr<ObjectPool<TIS_Info::ImageBuffer>> m_frameDataInfoPool = nullptr;
};

} // namespace _Kits