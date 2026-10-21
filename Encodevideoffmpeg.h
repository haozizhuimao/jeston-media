#pragma once

#include  <mutex>
#include <thread>
#include <functional>
#include "tis_global/struct_details/encode_input_struct.h"
//#include "VideoFilterGraph.h"
#include <memory>
#include <unordered_map>
#include <string>
#include "SubtitleFilterGraph.h"
#include "ScaleFilterGraph.h"

extern "C"
{
    #include "ffmpeg/libavcodec/avcodec.h"
    #include "ffmpeg/libavformat/avformat.h"
    #include "ffmpeg/libswscale/swscale.h"
    #include "ffmpeg/libavutil/hwcontext.h"
    #include "ffmpeg/libavutil/opt.h"
    #include "ffmpeg/libavutil/channel_layout.h"
    #include "ffmpeg/libavutil/common.h"
    #include "ffmpeg/libavutil/imgutils.h"
    #include "ffmpeg/libavutil/mathematics.h"
    #include "ffmpeg/libavutil/samplefmt.h"
    #include "ffmpeg/libavutil/time.h"
    #include "ffmpeg/libavutil/fifo.h"
    #include "ffmpeg/libavcodec/avcodec.h"
// 	#include "libavcodec/qsv.h"
	#include "ffmpeg/libavformat/avformat.h"
//	#include "libavformat/url.h"
	#include "ffmpeg/libavformat/avio.h"
//	#include "libavfilter/avcodec.h"
	// #include "libavfilter/avfiltergraph.h"
	#include "ffmpeg/libavfilter/avfilter.h"
	#include "ffmpeg/libavfilter/buffersink.h"
	#include "ffmpeg/libavfilter/buffersrc.h"
	#include "ffmpeg/libswscale/swscale.h"
	#include "ffmpeg/libswresample/swresample.h"
	#include "ffmpeg/libavdevice/avdevice.h"
}
namespace _Kits
{

class FFMPEGEncodeVideo final
{
private:

private:
    AVCodec *m_encodecodec = nullptr;
    AVCodecContext *m_codecContext= nullptr;;
    std::function<void(AVPacket&)>  m_writefilecallback;
    std::atomic_bool m_initialized = false;
    std::mutex m_write_mutex;
	AVBufferRef *m_hw_device_ctx = nullptr;
    AVBufferRef *m_hw_frames_ctx = nullptr;
    TIS_Info::SubtitleInputInfo m_subtitleinfo;
    std::unique_ptr<ScaleFilterGraph> m_scaleGraph = nullptr;
    std::unique_ptr<SubtitleFilterGraph> m_subtitleGraph = nullptr;

    SwsContext *m_swscontext = nullptr;
    AVFrame* m_srcFrame = nullptr;
    std::atomic_bool m_isHwAccelAvailable = false;
    std::atomic_bool m_isInitFilters = false;
    int64_t m_pts = 0;
    AVPixelFormat m_pixFormat = AV_PIX_FMT_YUV420P;
    AVPixelFormat m_hwsoftpixfmt = AV_PIX_FMT_YUV420P;
    AVHWDeviceType m_hwDeviceType = AV_HWDEVICE_TYPE_NONE;
    std::atomic_bool m_subtitilesInit = false;

public:
    FFMPEGEncodeVideo();
    ~FFMPEGEncodeVideo();

    /** * @brief  初始化编码
     * @param   encoderinfo   视频参数信息
     * @param   writefilecallback   写入文件回调
     * @return  0: sucess ** **/
    int Initencoder(const TIS_Info::EncoderInfo& encoderinfo, std::function<void(AVPacket&)>  writefilecallback,const TIS_Info::SubtitleInputInfo& subtitleinfo);

    /** * @brief  推入图片数据
     * @param   rgb   rgb图片数据
     * @param   size  图片大小
     * @param   srcpixfmt  图片格式
     * @return  0: sucess ** **/
    int WriteRGBData(const uint8_t *rgb,int width,int height,int srcpixfmt,const std::string& subtitles);

    /** * @brief  结束编码
     * @param   
     * @return  ** **/  
    void EndEncode();
private:
    /** * @brief  打开视频文件
     * @param   opt_arg  图片格式
     * @return ** **/
    void OpenVideo(AVDictionary *opt_arg);

    /** * @brief 释放资源
        * @return ** **/
    void Release();

    void handelPacket();

    bool initSwsContext(enum AVPixelFormat srcpixfmat,int width,int height,enum AVPixelFormat dstpixfmt);
    bool initSubtitleGraph(const int width,const int height,const int format);
    bool initScaleGraph(const int width,const int height,const int format);
    int EncodeVideo(AVFrame* input_frame);
    int EncodeSubtitle(AVFrame* input_frame);
    int EncodeScale(AVFrame* input_frame);
    int EncodeVideoNormal(AVFrame* input_frame);
    void initHWFrame(const int width,const int height);
    void avFrameCreate(const uint8_t* data,int width,int height,enum AVPixelFormat srcpixfmat,AVFrame* &frame);

};

}