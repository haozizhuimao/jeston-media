
#include "Encodevideoffmpeg.h"
#include <iostream>
#include <chrono>
#include "kits/utility/log/CRossLogger.h"
#include "FFmpegHWDeviceCtr.h"
// #include <vld.h>
namespace _Kits
{
FFMPEGEncodeVideo::FFMPEGEncodeVideo(/* args */)
{

    av_log_set_level(AV_LOG_QUIET); 
    //av_log_set_level(AV_LOG_TRACE); 
    m_subtitleGraph = std::make_unique<SubtitleFilterGraph>();
    m_scaleGraph = std::make_unique<ScaleFilterGraph>();
}

FFMPEGEncodeVideo::~FFMPEGEncodeVideo()
{
    Release();
	av_buffer_unref(&m_hw_device_ctx);
}

// 

int FFMPEGEncodeVideo::Initencoder(const TIS_Info::EncoderInfo& encoderinfo,std::function<void(AVPacket&)>  writefilecallback,const TIS_Info::SubtitleInputInfo& subtitleinfo)
{
    m_subtitleinfo = subtitleinfo;
    auto supported_hw = FFmpegHWDeviceCtr::getSupportedHWAccelTypes();
    m_hwDeviceType = FFmpegHWDeviceCtr::createSupportedHW(supported_hw,m_hw_device_ctx);
    LogInfo("FFMPEGEncodeVideo Initecnoder m_hwDeviceType {}",(int)m_hwDeviceType);
    if(AV_HWDEVICE_TYPE_NONE != m_hwDeviceType)
    {
        m_isHwAccelAvailable.store(true);
        m_hwsoftpixfmt = AV_PIX_FMT_NV12;
    }

    FFmpegHWDeviceCtr::createEncodeHW(m_hwDeviceType,encoderinfo,m_encodecodec,m_pixFormat);

    m_writefilecallback = writefilecallback;

    // 创建硬件帧上下文
    //initHWFrame(encoderinfo);

    m_codecContext = avcodec_alloc_context3(m_encodecodec);
    if (nullptr ==m_codecContext) 
    {
        LogError("Could not alloc an encoding context");
        return -2;
    }
    switch ((m_encodecodec)->type) 
    {
        case AVMEDIA_TYPE_VIDEO:
        {
            m_codecContext->codec_id = (AVCodecID)encoderinfo.avcodeid;
            m_codecContext->bit_rate = 1000000;
            m_codecContext->width    = encoderinfo.width;
            m_codecContext->height   = encoderinfo.height;
            m_codecContext->time_base = {1, encoderinfo.fps};
            m_codecContext->framerate = {encoderinfo.fps, 1};
            m_codecContext->gop_size      = encoderinfo.fps / 2; /* emit one intra frame every twelve frames at most */
            m_codecContext->pix_fmt       = m_pixFormat;
            
            if (m_isHwAccelAvailable.load())
            {
               // m_codecContext->pix_fmt       = m_pixFormat;
               // m_codecContext->hw_frames_ctx = av_buffer_ref(m_hw_frames_ctx);
                m_codecContext->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);
            }
            break;
        }
        default:
            break;
    }
    m_pts = 0;
    OpenVideo(nullptr);
    LogInfo("FFMPEGEncodeVideo Initecnoder successed");
//    auto ret =  m_filterGraph->initialize(1920, 1080,AV_PIX_FMT_NV12,m_subtitleinfo,false);
    return 0;
}
void FFMPEGEncodeVideo::OpenVideo(AVDictionary *opt_arg)
{
   int ret = avcodec_open2(m_codecContext, m_encodecodec, nullptr);
    if (ret < 0) 
    {
        LogError("Could not open video codec: {}",ret);
        return;
    }	
    m_initialized = true;
    return;
}
int FFMPEGEncodeVideo::WriteRGBData(const uint8_t *rgb,int width,int height,int srcpixfmt,const std::string& subtitles)
{
    if(nullptr == rgb)
    {
        return -1;
    }
    if (!m_subtitilesInit.load() && !subtitles.empty())
    {
       if(!initSubtitleGraph(width,height,m_hwsoftpixfmt))
       {
         return -2;
       }
        
    }
    if(!m_isInitFilters.load() && (width != m_codecContext->width || height != m_codecContext->height))
    {
        if(!initScaleGraph(width,height,m_hwsoftpixfmt))
        {
            return -3;
        }
    }

    
    std::lock_guard<std::mutex> lock(m_write_mutex);  
    // 构造 NV12 帧输入：Y + UV
    if (nullptr == m_srcFrame)
    {
        m_srcFrame = av_frame_alloc();
        m_srcFrame->format = m_hwsoftpixfmt;
        m_srcFrame->width = width;
        m_srcFrame->height = height;
        av_frame_get_buffer(m_srcFrame, 32);
      
    }
    av_frame_make_writable(m_srcFrame);
    avFrameCreate(rgb,width,height,static_cast<AVPixelFormat>(srcpixfmt),m_srcFrame);
    
    m_srcFrame->pts = m_pts;
    m_pts += av_rescale_q(1, m_codecContext->time_base, m_codecContext->time_base);


    if(!subtitles.empty() && m_subtitilesInit.load())
    {
        m_subtitleGraph->updateText(subtitles);
        // m_subtitleGraph->processFrame(m_srcFrame, result_frame);
        if(m_isInitFilters.load())
        {
            return EncodeVideo(m_srcFrame);
        }
        else
        {
           return EncodeSubtitle(m_srcFrame);
        }   
    }
    else
    {
        if(m_isInitFilters.load())
        {
            return EncodeScale(m_srcFrame);
        }
        else
        {
           return EncodeVideoNormal(m_srcFrame);
        }   

    }
    return 0;
}
 void FFMPEGEncodeVideo::EndEncode()
 {
    Release();
 }
void FFMPEGEncodeVideo::Release()
{
	if (!m_initialized)
	{
		return;
	}
    m_initialized = false;

    std::lock_guard<std::mutex> lock(m_write_mutex);  
    if (nullptr != m_codecContext)
    {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }

    if (nullptr != m_codecContext)
    {
        avcodec_close(m_codecContext);
        m_codecContext = nullptr;
    }
     m_pts = 0;
    
}
void FFMPEGEncodeVideo::handelPacket()
{
   // LogInfo("**************************avcodec_receive_packet: start ********************* ");
    AVPacket pkt = {0};
    av_init_packet(&pkt);
    while (true)
    {
        try
        {
          
            auto  ret = avcodec_receive_packet(m_codecContext, &pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
            {
                LogTrace("avcodec_receive_packet: EAGAIN or EOF");
                break;
            }
            else if (ret < 0) 
            {
                LogError("avcodec_receive_packet: {}",ret);
                break;
            }
            if (nullptr !=  m_writefilecallback) 
            {
                m_writefilecallback(pkt);
            }
            av_packet_unref(&pkt);
            LogTrace("**************************avcodec_receive_packet: end ********************* ");
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }   
    }
    av_packet_unref(&pkt);
    
}
bool FFMPEGEncodeVideo::initSwsContext(enum AVPixelFormat srcpixfmat,int width,int height,enum AVPixelFormat dstpixfmt)
{
    //m_swscontext = sws_getContext(width,height,srcpixfmat,width, height,dstpixfmt,SWS_BICUBIC,NULL,NULL,NULL);
    m_swscontext = sws_getContext(width,height,srcpixfmat,width, height,dstpixfmt, SWS_BILINEAR,NULL,NULL,NULL);
    if (nullptr == m_swscontext)
    {
        LogError("could not initialize the conversion context srcpixfmat:{} dstpixfmt:{}",(int)srcpixfmat,(int)dstpixfmt);
        // goto cleanup_name_context_stream_context_frame;
        return false;
    }
    LogInfo("initSwsContext: srcpixfmat:{} dstpixfmt:{}",(int)srcpixfmat,(int)dstpixfmt);
    return true;
}

bool FFMPEGEncodeVideo::initSubtitleGraph(const int width,const int height,const int format)
{
    auto ret = m_subtitleGraph->initialize(width, height,format,m_subtitleinfo);
    if (ret)
    {
        m_subtitilesInit.store(true);
    }
    else
    {
        LogError("initfilters failed:{}",ret);
    }
    return ret;
}

bool FFMPEGEncodeVideo::initScaleGraph(const int width,const int height,const int format)
{
    auto ret = m_scaleGraph->initialize(width, height,format,m_hwDeviceType,m_codecContext->width,m_codecContext->height,m_hw_device_ctx);
    if (ret)
    {
        m_isInitFilters.store(true);

    }
    else
    {
        LogError("initfilters sacle failed:");
    }
    return ret;
}

int FFMPEGEncodeVideo::EncodeVideo(AVFrame* input_frame)
{
    // int ret = 0;
    AVFrame* result_frame =  av_frame_alloc();      
    av_frame_make_writable(result_frame);

    m_subtitleGraph->processFrame(input_frame, result_frame);
        
    AVFrame*  hw_result_frame =  av_frame_alloc();      
    av_frame_make_writable(hw_result_frame);
    m_scaleGraph->processFrame(result_frame, &hw_result_frame);
    
    if (!m_initialized)
    {
        m_codecContext->hw_frames_ctx = av_buffer_ref(hw_result_frame->hw_frames_ctx);
        OpenVideo(nullptr);
    }
    auto ret = avcodec_send_frame(m_codecContext, hw_result_frame);
    if (ret == AVERROR(EAGAIN)) 
    {
        ret = avcodec_send_frame(m_codecContext, hw_result_frame); // 重试发送
    }
    else if (ret < 0 && ret != AVERROR_EOF) 
    {
        av_frame_unref(hw_result_frame);
        LogError("error sending a frame for encoding:{}",ret);
        return 0 ;
    }

    handelPacket();
    av_frame_unref(hw_result_frame);

    av_frame_free(&result_frame);
    av_frame_free(&hw_result_frame);
    return 0;
}

int FFMPEGEncodeVideo::EncodeSubtitle(AVFrame* input_frame)
{
    AVFrame* result_frame =  av_frame_alloc();      
    av_frame_make_writable(result_frame);

    m_subtitleGraph->processFrame(input_frame, result_frame);  

    if (!m_initialized)
    {
        initHWFrame(m_codecContext->width,m_codecContext->height);
        m_codecContext->hw_frames_ctx = av_buffer_ref(m_hw_frames_ctx);
        OpenVideo(nullptr);
    }
    
    AVFrame* hwframe = av_frame_alloc();
    hwframe->format = m_pixFormat;
    hwframe->width = m_codecContext->width;
    hwframe->height = m_codecContext->height;
    int ret = av_hwframe_get_buffer(m_hw_frames_ctx, hwframe, 0);
    if (ret < 0)
    {
        av_frame_free(&hwframe);
        LogError("av_hwframe_get_buffer ret:{},{},{}",ret,hwframe->width,hwframe->height);
    }
        
    ret =  av_hwframe_transfer_data(hwframe, result_frame, 0);
    LogTrace("av_hwframe_transfer_data ret:{}",ret);

    ret = avcodec_send_frame(m_codecContext, hwframe);
    if (ret == AVERROR(EAGAIN)) 
    {
        ret = avcodec_send_frame(m_codecContext, hwframe); // 重试发送
    }
    else if (ret < 0 && ret != AVERROR_EOF) 
    {
        LogError("error sending a frame for encoding:{}",ret);
        return 0 ;
    }
    handelPacket();
    av_frame_free(&hwframe);
    av_frame_free(&result_frame);
    return 0;
}
int FFMPEGEncodeVideo::EncodeScale(AVFrame* input_frame)
{
    // int ret = 0;

    AVFrame*  hw_result_frame =  av_frame_alloc();      
    av_frame_make_writable(hw_result_frame);
    m_scaleGraph->processFrame(input_frame, &hw_result_frame);
    
    // do 
    // {
        if (!m_initialized)
        {
            m_codecContext->hw_frames_ctx = av_buffer_ref(hw_result_frame->hw_frames_ctx);
            OpenVideo(nullptr);
        }
       auto ret = avcodec_send_frame(m_codecContext, hw_result_frame);
        if (ret == AVERROR(EAGAIN)) 
        {
            ret = avcodec_send_frame(m_codecContext, hw_result_frame); // 重试发送
        }
        else if (ret < 0 && ret != AVERROR_EOF) 
        {
            av_frame_unref(hw_result_frame);
            //std::cout << "error sending a frame for encoding:" << ret <<  std::endl;
            return 0 ;
        }

        handelPacket();
        av_frame_unref(hw_result_frame);
    // }
    // while (!ret);
    av_frame_free(&hw_result_frame);
    return 0;
}
int FFMPEGEncodeVideo::EncodeVideoNormal(AVFrame* input_frame)
{

    if (!m_initialized)
    {
        initHWFrame(m_codecContext->width,m_codecContext->height);
        m_codecContext->hw_frames_ctx = av_buffer_ref(m_hw_frames_ctx);
        OpenVideo(nullptr);
    }
    if(m_isHwAccelAvailable.load())
    { 
        AVFrame* hwframe = av_frame_alloc();
        hwframe->format = m_pixFormat;
        hwframe->width = m_codecContext->width;
        hwframe->height = m_codecContext->height;
        int ret1 = av_hwframe_get_buffer(m_hw_frames_ctx, hwframe, 0);
        if (ret1 < 0) {
            av_frame_free(&hwframe);
            return -1;
        LogError("av_hwframe_get_buffer ret:{}",ret1);
        }

        auto ret =  av_hwframe_transfer_data(hwframe, input_frame, 0);
        LogTrace("av_hwframe_transfer_data ret:{}",ret);

        ret = avcodec_send_frame(m_codecContext, hwframe);
        if (ret == AVERROR(EAGAIN)) 
        {
            ret = avcodec_send_frame(m_codecContext, hwframe); // 重试发送
        }
        else if (ret < 0 && ret != AVERROR_EOF) 
        {
            av_frame_free(&hwframe);
            return 0 ;
        }
        av_frame_free(&hwframe);
    }
    else
    {
        auto ret = avcodec_send_frame(m_codecContext, input_frame);
        if (ret == AVERROR(EAGAIN)) 
        {
            ret = avcodec_send_frame(m_codecContext, input_frame); // 重试发送
        }
        else if (ret < 0 && ret != AVERROR_EOF) 
        {

            return 0 ;
        }
    }

    handelPacket();
    return 0;
}

void FFMPEGEncodeVideo::initHWFrame(const int width,const int height)
{
// std::cout<<"InitHWFrame"<<std::endl;
    if (nullptr == m_hw_device_ctx)
    {
        LogError("HW device context is null");
        // std::cout<<"InitHWFrame 1"<<std::endl;
        return;
    }

    m_hw_frames_ctx = av_hwframe_ctx_alloc(m_hw_device_ctx);
    if (nullptr ==  m_hw_frames_ctx) 
    {
        LogError("Failed to allocate HW frames context");
        return;
    }

    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)m_hw_frames_ctx->data;
    frames_ctx->format = m_pixFormat;
    frames_ctx->sw_format = m_hwsoftpixfmt;  // 或者你的输入格式
    frames_ctx->width =  width;
    frames_ctx->height =  height;
    frames_ctx->initial_pool_size = 20;  // 帧缓冲区数量

    auto ret = av_hwframe_ctx_init(m_hw_frames_ctx);
    if (ret < 0) {
        LogError("Failed to initialize HW frames context");
            return;
    }

}

void FFMPEGEncodeVideo::avFrameCreate(const uint8_t* data,int width,int height,enum AVPixelFormat srcpixfmat,AVFrame* &frame)
{
    if (nullptr == m_swscontext)
    {
        if(!initSwsContext(static_cast<AVPixelFormat>(srcpixfmat),width, height, static_cast<AVPixelFormat>(m_hwsoftpixfmt)))
        {
            LogError("initSwsContext failed");
        }
    }

    if(AV_PIX_FMT_GRAY8 == srcpixfmat)
    {

        // 设置源数据指针
        // const uint8_t* srcdata[AV_NUM_DATA_POINTERS] = {data};
        // int srclinesize[AV_NUM_DATA_POINTERS] = {width};
        // sws_scale(m_swscontext, srcdata, srclinesize, 0, height, frame->data, frame->linesize);
        memcpy(frame->data[0], data, width * height);

        // 填充 UV 分量为 128（灰色）
        memset(frame->data[1], 128, width * height / 2);
    }
    else if(AV_PIX_FMT_YUV420P == srcpixfmat)
    {

        int y_plane_size = width * height;
        int uv_plane_size = (width / 2) * (height / 2);  // 注意：是 width/2 * height/2

        // 设置源数据指针
        const uint8_t* srcdata[AV_NUM_DATA_POINTERS] = {
            data,                        // Y平面
            data + y_plane_size,         // U平面
            data + y_plane_size + uv_plane_size  // V平面
        };

        int srclinesize[AV_NUM_DATA_POINTERS] = {width,width/2,width/2};
        sws_scale(m_swscontext, srcdata, srclinesize, 0, height, frame->data, frame->linesize);

    }
    else if(AV_PIX_FMT_RGB8 == srcpixfmat)
    {
        const uint8_t* srcdata[AV_NUM_DATA_POINTERS] = {data};
        int srclinesize[AV_NUM_DATA_POINTERS] = {width * 3};
        sws_scale(m_swscontext, srcdata, srclinesize, 0, height, frame->data, frame->linesize);
    }

}

}