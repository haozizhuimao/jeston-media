#include "FFmpegHWDeviceCtr.h"
#include "kits/utility/log/CRossLogger.h"

namespace _Kits {
std::vector<AVHWDeviceType> FFmpegHWDeviceCtr::getSupportedHWAccelTypes()
{
    // 优先级顺序
    // 获取系统实际支持的硬件类型
    std::vector<AVHWDeviceType> system_supported;
    AVHWDeviceType current_type = AV_HWDEVICE_TYPE_NONE;
    while ((current_type = av_hwdevice_iterate_types(current_type)) != AV_HWDEVICE_TYPE_NONE)
    {
        system_supported.push_back(current_type);
        LogInfo("System supports HW device type: {}", (int)current_type);
    }

    return system_supported;
}

AVHWDeviceType FFmpegHWDeviceCtr::createSupportedHW(const std::vector<AVHWDeviceType> &hwAccelTypes, AVBufferRef *&hw_device_ctx)
{

    for (auto preferred_type : priority_order)
    {
        // 对于 CPU，总是包含在最后
        if (preferred_type == AV_HWDEVICE_TYPE_NONE)
        {
            // supported_sorted.push_back(preferred_type);
            continue;
        }

        // 检查系统是否支持该硬件类型
        bool is_supported = std::find(hwAccelTypes.begin(), hwAccelTypes.end(), preferred_type) != hwAccelTypes.end();
        if (is_supported)
        {
            // 验证硬件设备是否可以实际创建
            int ret = av_hwdevice_ctx_create(&hw_device_ctx, preferred_type, "auto", nullptr, 0);
            if (0 == ret)
            {
                // supported_sorted.push_back(preferred_type);
                LogInfo("Verified HW device type: {} is available", (int)preferred_type);
                return preferred_type;
            }
            else
            {
                LogWarn("HW device type {} exists but failed to create: {}", (int)preferred_type, ret);
            }
        }
        else
        {
            LogInfo("HW device type {} not supported by system", (int)preferred_type);
        }
    }

    LogInfo("All HW acceleration attempts failed, using CPU encoding");
    hw_device_ctx = nullptr;

    return AV_HWDEVICE_TYPE_NONE;
}

void FFmpegHWDeviceCtr::createEncodeHW(const AVHWDeviceType hwAccelType,
                                       const TIS_Info::EncoderInfo &encoderinfo,
                                       AVCodec *&encodecodec,
                                       AVPixelFormat &pixFormat)
{
    bool encoder_created = false;
    switch (hwAccelType)
    {
    case AV_HWDEVICE_TYPE_CUDA:
        encodecodec = const_cast<AVCodec *>(avcodec_find_encoder_by_name("h264_nvenc"));
        if (encodecodec)
        {
            LogInfo("Using CUDA H.264 encoder: h264_nvenc");
            encoder_created = true;
        }
        else
        {
            LogWarn("CUDA encoder h264_nvenc not found, trying fallback");
        }
        break;

    case AV_HWDEVICE_TYPE_QSV:
        encodecodec = const_cast<AVCodec *>(avcodec_find_encoder_by_name("h264_qsv"));
        if (nullptr != encodecodec)
        {
            LogInfo("Using QSV H.264 encoder: h264_qsv");
            encoder_created = true;
        }
        else
        {
            LogWarn("QSV encoder h264_qsv not found, trying fallback");
        }
        break;

    case AV_HWDEVICE_TYPE_VULKAN:

        // Vulkan 主要用于处理，编码支持有限，尝试使用 Vulkan 加速的编码器
        encodecodec = const_cast<AVCodec *>(avcodec_find_encoder_by_name("h264_v4l2m2m")); // 可能的 Vulkan 相关编码器
        if (!encodecodec)
        {
            encodecodec = const_cast<AVCodec *>(avcodec_find_encoder_by_name("h264_vaapi")); // 备选
        }
        if (encodecodec)
        {
            LogInfo("Using Vulkan-accelerated H.264 encoder: {}", encodecodec->name);
            encoder_created = true;
        }
        else
        {
            LogWarn("Vulkan encoder not found, falling back to CPU");
        }
        break;

    case AV_HWDEVICE_TYPE_NONE:
    default:
        // CPU 编码或硬件编码失败的回退
        break;
    }
    pixFormat = hwDeviceFormat[hwAccelType];
    LogInfo("Using HW device type pixformat: {}", (int)pixFormat);
    // 如果硬件编码器创建失败，回退到软件编码器
    if (!encoder_created)
    {
        LogInfo("Using software H.264 encoder");
        encodecodec = const_cast<AVCodec *>(avcodec_find_encoder_by_name(encoderinfo.codecname.c_str()));
        if (nullptr == encodecodec)
        {
            LogError("not find codec:{}", encoderinfo.codecname);
            encodecodec = const_cast<AVCodec *>(avcodec_find_encoder((AVCodecID)encoderinfo.avcodeid));
            if (nullptr == encodecodec)
            {
                LogError("not find codec id:{}", encoderinfo.avcodeid);
            }
        }
    }
}

//   void FFmpegHWDeviceCtr::initHWFrame(const TIS_Info::EncoderInfo& encoderinfo)
// {
//    // std::cout<<"InitHWFrame"<<std::endl;
//     if (nullptr == m_hw_device_ctx)
//     {
//         LogError("HW device context is null");
//         // std::cout<<"InitHWFrame 1"<<std::endl;
//         return;
//     }

//     m_hw_frames_ctx = av_hwframe_ctx_alloc(m_hw_device_ctx);
//     if (nullptr ==  m_hw_frames_ctx)
//     {
//         LogError("Failed to allocate HW frames context");
//         return;
//     }

//     AVHWFramesContext* frames_ctx = (AVHWFramesContext*)m_hw_frames_ctx->data;
//     m_hwsoftpixfmt = AV_PIX_FMT_NV12;
//     frames_ctx->format = m_pixFormat;
//     frames_ctx->sw_format = m_hwsoftpixfmt;  // 或者你的输入格式
//     frames_ctx->width =  encoderinfo.width;
//     frames_ctx->height =  encoderinfo.height;
//     frames_ctx->initial_pool_size = 20;  // 帧缓冲区数量

//     auto ret = av_hwframe_ctx_init(m_hw_frames_ctx);
//     if (ret < 0) {
//         LogError("Failed to initialize HW frames context");
//             return;
//     }

// }

} // namespace _Kits