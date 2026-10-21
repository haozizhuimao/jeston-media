#include "FFmpegPuller.h"
#include <iostream>
#include "kits/utility/log/CRossLogger.h"
#include "kits/primary/thread_pool/thread_pool.h"


namespace _Kits{
FFmpegPuller::FFmpegPuller() 
{
    //  av_log_set_level(AV_LOG_TRACE);
    avformat_network_init();
    m_frameDataInfoPool = std::make_shared<ObjectPool<TIS_Info::ImageBuffer>>(64);
}

FFmpegPuller::~FFmpegPuller()
{
    stop();
    avformat_network_deinit();
}

void FFmpegPuller::setFrameCallback(FrameCallback cb)
{
    m_callback = std::move(cb);
}

bool FFmpegPuller::open(const std::string& url,const std::string& cameraname) 
{
    m_url = url;
    m_cameraname = cameraname;
    AVFormatContext* format_ctx = nullptr;
    AVDictionary* options = nullptr;
    
    // 紧急网络优化参数
    av_dict_set(&options, "rtsp_transport", "tcp", 0); // 强制TCP，避免UDP丢包
    av_dict_set(&options, "timeout", "10000000", 0);   // 10秒超时
    av_dict_set(&options, "stimeout", "10000000", 0);  // 10秒socket超时
    av_dict_set(&options, "max_delay", "100000", 0);   // 大幅减少最大延迟为0.1秒

    // 关键：减少缓冲区大小，避免延迟累积
    av_dict_set(&options, "buffer_size", "65536", 0); // 64KB缓冲区
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
    av_dict_set(&options, "avioflags", "direct", 0);

    // 重连设置
    av_dict_set(&options, "reconnect", "1", 0);
    av_dict_set(&options, "reconnect_at_eof", "1", 0);
    av_dict_set(&options, "reconnect_streamed", "1", 0);
    av_dict_set(&options, "reconnect_delay_max", "2", 0);

    // 针对丢包的特定优化
    av_dict_set(&options, "rtsp_flags", "prefer_tcp", 0);
    av_dict_set(&options, "skip_loop_filter", "none", 0);
    av_dict_set(&options, "skip_frame", "none", 0);

    // 减少分析时间，快速启动
    av_dict_set(&options, "analyzeduration", "100000", 0); // 0.1秒
    av_dict_set(&options, "probesize", "512", 0);
    // 打开输入流
    auto ret = avformat_open_input(&m_fmt_ctx, url.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (ret < 0)
    {
        LogError("[拉流模块] Failed to open input: {}  {}", url, ret);
        return false;
    }
    ret = avformat_find_stream_info(m_fmt_ctx, nullptr);
    if (ret < 0)
    {

        LogError("[拉流模块] Failed to find stream info. {}", ret);
        return false;
    }

    // 找到视频流
    ret = av_find_best_stream(m_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0)
    {
        LogError("[拉流模块] No video stream found. {}", ret);
        return false;
    }
    m_video_stream_index = ret;

    AVStream *video_stream = m_fmt_ctx->streams[m_video_stream_index];
    const AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (nullptr == codec)
    {
        LogError("[拉流模块] Decoder not found");
        return false;
    }

    m_codec_ctx = avcodec_alloc_context3(codec);
    if (nullptr == m_codec_ctx)
    {

        LogError("[拉流模块] Failed to alloc codec ctx.");
        return false;
    }

    avcodec_parameters_to_context(m_codec_ctx, video_stream->codecpar);
    ret = avcodec_open2(m_codec_ctx, codec, nullptr);
    if (ret < 0)
    {
        // std::cerr << "Failed to open codec." << std::endl;
        LogError("[拉流模块] Failed to open codec. {}", ret);
        return false;
    }
    m_startFlag.store(true);
    return true;
}

void FFmpegPuller::start()
{
    if (m_running || !m_startFlag.load())
        return;
    m_running = true;
    m_thread = std::thread(&FFmpegPuller::run, this);
}

void FFmpegPuller::stop()
{
    m_startFlag.store(false);
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();

    if (nullptr != m_codec_ctx)
    {
        avcodec_free_context(&m_codec_ctx);
        m_codec_ctx = nullptr;
    }
    if (nullptr != m_fmt_ctx)
    {
        avformat_close_input(&m_fmt_ctx);
        m_fmt_ctx = nullptr;
    }
}
void FFmpegPuller::avframeToImageBuffer(const AVFrame *frame)
{
    if (nullptr == frame)
    {
        return;
    }
    auto imageBuffer = m_frameDataInfoPool->getObject(frame->width,frame->height,3);
    imageBuffer->name = m_cameraname;

    uint8_t *dst_data[4];
    int dst_linesize[4];
    av_image_fill_arrays(dst_data, dst_linesize, imageBuffer->data.data(), (AVPixelFormat)frame->format, frame->width, frame->height, 1);

    // 拷贝像素数据
    av_image_copy(
        dst_data, dst_linesize, (const uint8_t **)frame->data, frame->linesize, (AVPixelFormat)frame->format, frame->width, frame->height);

    if (m_callback)
    {
        m_callback(std::move(imageBuffer));
    }
}
void FFmpegPuller::run()
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while (m_running)
    {
        int ret = av_read_frame(m_fmt_ctx, pkt);
        if (ret < 0)
        {
            LogError ("End of stream or error, ret={}",ret);
            //av_usleep(10000);
            
            if(AVERROR_HTTP_BAD_REQUEST == ret || -10054 == ret || -138 == ret)
            {
                //重连
                std::string title = "FFmpegPuller_ " + m_url;
                _Kits::thread_pool::runEvery(title,60* 1000, [this, title]()
                {
                    LogInfo("FFmpegPuller::runEvery {} url {}",title, m_url);
                    stop();
                    auto ret = open(m_url,m_cameraname);
                    if(ret)
                    {
                        start();
                        _Kits::thread_pool::cancelTimer(title);
                    }
                });
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (pkt->stream_index == m_video_stream_index)
        {
            ret = avcodec_send_packet(m_codec_ctx, pkt);
            if (ret < 0)
                continue;

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(m_codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0)
                    break;

                avframeToImageBuffer(frame);
                av_frame_unref(frame);
            }
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
}

} // namespace _Kits