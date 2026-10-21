#pragma once
#include <memory>
#include <string>

extern "C" 
{
    #include "ffmpeg/libavfilter/buffersink.h"
    #include "ffmpeg/libavfilter/buffersrc.h"
    #include "ffmpeg/libavutil/opt.h"
    #include "ffmpeg/libavutil/hwcontext.h"
}

namespace _Kits
{
    class ScaleFilterGraph
    { 
    public:
        ScaleFilterGraph();
        ~ScaleFilterGraph();

        // 禁用拷贝和赋值
        ScaleFilterGraph(const ScaleFilterGraph&) = delete;
        ScaleFilterGraph& operator=(const ScaleFilterGraph&) = delete;

        int processFrame(AVFrame* input_frame, AVFrame** output_frame);

        bool initialize(int width, int height, int format,AVHWDeviceType type,
                    const int outwidth,const int outheight,const  AVBufferRef* hw_device_ctx);

        // 清理资源
        void cleanup();

    private:

        bool createBufferSourceFilter(int width, int height, int format);
        bool createHardwareUploadFilter(const  AVBufferRef* hw_device_ctx,AVHWDeviceType type);
        bool createScaleFilter(const int width,const int height, const AVBufferRef* hw_device_ctx,AVHWDeviceType type);
        bool createBufferSinkFilter(const  AVBufferRef* hw_device_ctx);
        bool linkFilters();
        int initSacaleFilter(const std::string &args,AVHWDeviceType type);
        std::string getUploadFilterName(AVHWDeviceType type);
        



    private:
        AVFilterGraph* m_filter_graph = nullptr;
        AVFilterContext* m_buffersrc_ctx = nullptr;
        AVFilterContext* m_buffersink_ctx = nullptr;
        AVFilterContext* m_drawtext_ctx = nullptr;
        AVFilterContext* m_hwupload_ctx = nullptr;
        AVFilterContext* m_scale_ctx = nullptr;
       // AVBufferRef* m_hw_device_ctx = nullptr;
        AVHWDeviceType m_hw_device_type = AV_HWDEVICE_TYPE_NONE;
         bool m_initialized = false;
    }; 
}