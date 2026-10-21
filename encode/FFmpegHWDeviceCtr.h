#pragma once

#include <vector>
#include <unordered_map>
#include "tis_global/struct_details/encode_input_struct.h"

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
#include "ffmpeg/libavformat/avformat.h"
#include "ffmpeg/libavformat/avio.h"
#include "ffmpeg/libavfilter/avfilter.h"
#include "ffmpeg/libavfilter/buffersink.h"
#include "ffmpeg/libavfilter/buffersrc.h"
#include "ffmpeg/libswscale/swscale.h"
#include "ffmpeg/libswresample/swresample.h"
#include "ffmpeg/libavdevice/avdevice.h"
}

namespace _Kits {

class FFmpegHWDeviceCtr
{
  public:
    inline static std::vector<AVHWDeviceType> priority_order = {
        AV_HWDEVICE_TYPE_CUDA,   // 第一优先级: NVIDIA CUDA
        AV_HWDEVICE_TYPE_QSV,    // 第二优先级: Intel Quick Sync
        AV_HWDEVICE_TYPE_VULKAN, // 第三优先级: Vulkan
        AV_HWDEVICE_TYPE_D3D11VA,
        AV_HWDEVICE_TYPE_NONE // CPU 回退
    };
    inline static std::unordered_map<AVHWDeviceType, AVPixelFormat> hwDeviceFormat = {{AV_HWDEVICE_TYPE_CUDA, AV_PIX_FMT_CUDA},
                                                                                      {AV_HWDEVICE_TYPE_QSV, AV_PIX_FMT_QSV},
                                                                                      {AV_HWDEVICE_TYPE_VULKAN, AV_PIX_FMT_VULKAN},
                                                                                      {AV_HWDEVICE_TYPE_NONE, AV_PIX_FMT_YUV420P}};

  public:
    FFmpegHWDeviceCtr() = default;
    ~FFmpegHWDeviceCtr() = default;
    FFmpegHWDeviceCtr(const FFmpegHWDeviceCtr &) = delete;
    FFmpegHWDeviceCtr(const FFmpegHWDeviceCtr &&) = delete;
    FFmpegHWDeviceCtr &operator=(const FFmpegHWDeviceCtr &) = delete;
    FFmpegHWDeviceCtr &&operator=(const FFmpegHWDeviceCtr &&) = delete;

  public:
    static std::vector<AVHWDeviceType> getSupportedHWAccelTypes();
    static AVHWDeviceType createSupportedHW(const std::vector<AVHWDeviceType> &hwAccelTypes, AVBufferRef *&hw_device_ctx);
    static void createEncodeHW(const AVHWDeviceType hwAccelType,
                               const TIS_Info::EncoderInfo &encoderinfo,
                               AVCodec *&encodecodec,
                               AVPixelFormat &pixFormat);
};

} // namespace _Kits