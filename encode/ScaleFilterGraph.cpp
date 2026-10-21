#include "ScaleFilterGraph.h"
#include "kits/utility/log/CRossLogger.h"

#ifdef _WIN32
#include <format> // Windows 用原生
#define FMT_STRING std::format
#else
#include <fmt/core.h> // Linux/macOS 用 fmt
#define FMT_STRING fmt::format
#endif
namespace _Kits {
ScaleFilterGraph::ScaleFilterGraph() {}
ScaleFilterGraph::~ScaleFilterGraph() {}

int ScaleFilterGraph::processFrame(AVFrame *input_frame, AVFrame **output_frame)
{
    if (!m_initialized)
    {
        return AVERROR(EINVAL);
    }

    // 推送帧到过滤器图
    int ret = av_buffersrc_add_frame_flags(m_buffersrc_ctx, input_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0)
    {
        LogError("Failed to push frame to filter graph {}", ret);
        return ret;
    }

    // 从过滤器图获取帧
    ret = av_buffersink_get_frame_flags(m_buffersink_ctx, *output_frame, 0);
    if (0 != ret)
    {
        LogError("Failed to get frame from filter graph {}", ret);
    }
    return ret;
}

bool ScaleFilterGraph::initialize(
    int width, int height, int format, AVHWDeviceType type, const int outwidth, const int outheight, const AVBufferRef *hw_device_ctx)
{
    if (m_initialized)
    {
        cleanup();
    }
    m_filter_graph = avfilter_graph_alloc();
    if (nullptr == m_filter_graph)
    {
        return false;
    }
    // 创建输入过滤器
    if (!createBufferSourceFilter(width, height, format))
    {
        LogError("Failed to create buffer source filter");
        return false;
    }
    // 创建上传
    if (!createHardwareUploadFilter(hw_device_ctx, type))
    {
        LogError("Failed to create hwupload filter");
        return false;
    }
    // 创建缩放过滤器
    if (!createScaleFilter(outwidth, outheight, hw_device_ctx, type))
    {
        LogError("Failed to create drawtext filter");
        return false;
    }

    if (!createBufferSinkFilter(hw_device_ctx))
    {
        LogError("Failed to create out buffer sink filter");
        return false;
    }

    if (!linkFilters())
    {
        LogError("Failed to link filters");
        return false;
    }

    // 配置过滤器图
    int ret = avfilter_graph_config(m_filter_graph, nullptr);
    if (ret < 0)
    {
        LogError("Failed to configure filter graph {}", ret);
        cleanup();
        return false;
    }

    m_initialized = true;
    LogInfo("Initialized video filter graph");
    return true;
}

bool ScaleFilterGraph::createBufferSourceFilter(int width, int height, int format)
{
    std::string args = FMT_STRING("video_size={}x{}:pix_fmt={}:time_base=1/1", width, height, format);

    int ret = avfilter_graph_create_filter(&m_buffersrc_ctx, avfilter_get_by_name("buffer"), "in", args.c_str(), nullptr, m_filter_graph);
    if (ret < 0)
    {
        LogError("Failed to create buffer source filter {}", ret);
        return false;
    }
    LogInfo("Created buffer source filter");
    return true;
}

bool ScaleFilterGraph::createHardwareUploadFilter(const AVBufferRef *hw_device_ctx, AVHWDeviceType type)
{
    // int ret = avfilter_graph_create_filter(&m_hwupload_ctx,
    //                                      avfilter_get_by_name("hwupload"),
    //                                      "upload",
    //                                      "extra_hw_frames=64",
    //                                      nullptr,
    //                                      m_filter_graph);
    auto avfilterName = getUploadFilterName(type);
    int ret = avfilter_graph_create_filter(
        &m_hwupload_ctx, avfilter_get_by_name(avfilterName.c_str()), "upload", "extra_hw_frames=64", nullptr, m_filter_graph);
    if (ret < 0)
    {
        LogError("Failed to create hwupload filter {}", ret);
        return false;
    }

    if (nullptr != hw_device_ctx)
    {
        // std::cout << "11111111111111111111111111111111" <<  m_hw_device_ctx  << ":" << m_hw_device_ctx->data<< std::endl;
        m_hwupload_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        LogInfo("copy  m_hw_device_ctx filter");
    }
    LogInfo("Created hwupload filter");
    return true;
}

bool ScaleFilterGraph::createScaleFilter(const int width, const int height, const AVBufferRef *hw_device_ctx, AVHWDeviceType type)
{
    std::string scale_args = FMT_STRING("w={}:h={}", width, height);

    // const AVFilter* scale_filter = avfilter_get_by_name("scale_cuda");
    // if (!scale_filter) {
    //     LogError("scale_cuda 过滤器不可用，尝试使用 scale_npp");
    //     scale_filter = avfilter_get_by_name("scale_npp");
    // }

    // // 根据硬件类型选择合适的缩放过滤器
    // int ret = avfilter_graph_create_filter(&m_scale_ctx,
    //                                      avfilter_get_by_name("scale_cuda"),
    //                                      "scale_cuda",
    //                                      scale_args.c_str(),
    //                                      nullptr,
    //                                      m_filter_graph);
    // int ret = avfilter_graph_create_filter(&m_scale_ctx, avfilter_get_by_name("scale_qsv"),"scale", scale_args.c_str(), NULL,
    // m_filter_graph);
    int ret = initSacaleFilter(scale_args, type);
    if (ret < 0)
    {
        LogError("Failed to create scale filter {}", ret);
        return false;
    }

    if (nullptr != hw_device_ctx)
    {
        m_scale_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    }
    LogInfo("Created scale filter");
    return true;
}

bool ScaleFilterGraph::createBufferSinkFilter(const AVBufferRef *hw_device_ctx)
{
    int ret = avfilter_graph_create_filter(&m_buffersink_ctx, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, m_filter_graph);
    if (ret < 0)
    {
        LogError("Failed to create buffer sink filter {}", ret);
        return false;
    }

    if (nullptr != hw_device_ctx)
    {
        m_buffersink_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    }
    // m_buffersink_ctx->hw_frames_ctx = m_hw_frames_ctx;
    LogInfo("Created buffer sink filter");
    return true;
}

bool ScaleFilterGraph::linkFilters()
{
    // 连接到硬件上传
    auto ret = avfilter_link(m_buffersrc_ctx, 0, m_hwupload_ctx, 0);
    if (ret < 0)
        return false;

    // 连接到缩放
    ret = avfilter_link(m_hwupload_ctx, 0, m_scale_ctx, 0);
    if (ret < 0)
        return false;

    // 连接到输出
    ret = avfilter_link(m_scale_ctx, 0, m_buffersink_ctx, 0);
    if (ret < 0)
        return false;

    // }

    return true;
}

void ScaleFilterGraph::cleanup()
{
    if (m_filter_graph)
    {
        avfilter_graph_free(&m_filter_graph);
    }

    m_buffersrc_ctx = nullptr;
    m_buffersink_ctx = nullptr;
    m_hwupload_ctx = nullptr;
    m_scale_ctx = nullptr;
    // av_buffer_unref(&m_hw_device_ctx);
    m_initialized = false;
}

std::string ScaleFilterGraph::getUploadFilterName(AVHWDeviceType type)
{
    switch (type)
    {
    case AV_HWDEVICE_TYPE_CUDA:
        return "hwupload_cuda";
    case AV_HWDEVICE_TYPE_QSV:
        return "hwupload";
    default:
        return "hwupload";
    }
    return "hwupload";
}

int ScaleFilterGraph::initSacaleFilter(const std::string &args, AVHWDeviceType type)
{
    int ret = -1;
    switch (type)
    {
    case AV_HWDEVICE_TYPE_CUDA: {
        ret = avfilter_graph_create_filter(
            &m_scale_ctx, avfilter_get_by_name("scale_cuda"), "scale_cuda", args.c_str(), nullptr, m_filter_graph);
    }
    break;
    case AV_HWDEVICE_TYPE_QSV: {
        ret = avfilter_graph_create_filter(&m_scale_ctx, avfilter_get_by_name("scale_qsv"), "scale", args.c_str(), NULL, m_filter_graph);
    }
    case AV_HWDEVICE_TYPE_D3D11VA:
        ret = avfilter_graph_create_filter(&m_scale_ctx, avfilter_get_by_name("scale"), "scale", args.c_str(), NULL, m_filter_graph);
        break;
    default:
        break;
    }
    return ret;
}

} // namespace _Kits
