#include "SubtitleFilterGraph.h"
#include "kits/utility/log/CRossLogger.h"
#ifdef _WIN32
#include <format> // Windows 用原生
#define TOGEE_FMT_STRING std::format
#else
#include <fmt/core.h> // Linux/macOS 用 fmt
#define TOGEE_FMT_STRING fmt::format
#endif

using namespace _Kits;

SubtitleFilterGraph::~SubtitleFilterGraph()
{
    cleanup();
}

bool SubtitleFilterGraph::initialize(int width, int height, int format, const TIS_Info::SubtitleInputInfo &subtitleinfo)
{
    cleanup();
    m_graph = avfilter_graph_alloc();
    if (!m_graph)
        return false;

    if (!createBufferSource(width, height, format))
    {
        return false;
    }
    if (!createDrawText(subtitleinfo))
    {
        return false;
    }
    if (!createBufferSink())
    {
        return false;
    }
    if (!linkFilters())
    {
        return false;
    }

    int ret = avfilter_graph_config(m_graph, nullptr);
    if (ret < 0)
    {
        LogError("SubtitleFilterGraph config failed {}", ret);
        cleanup();
        return false;
    }
    m_initialized = true;
    return true;
}

bool SubtitleFilterGraph::createBufferSource(int w, int h, int fmt)
{
    std::string args = TOGEE_FMT_STRING("video_size={}x{}:pix_fmt={}:time_base=1/1", w, h, fmt);
    int ret = avfilter_graph_create_filter(&m_src, avfilter_get_by_name("buffer"), "in", args.c_str(), nullptr, m_graph);
    LogInfo("SubtitleFilterGraph::createBufferSource {}", ret);
    return ret >= 0;
}

bool SubtitleFilterGraph::createDrawText(const TIS_Info::SubtitleInputInfo &info)
{
    std::string args = TOGEE_FMT_STRING("fontfile='{}':fontsize={}:fontcolor={}:boxcolor={}:x={}:y={}:text=' '",
                                  info.fontfile,
                                  info.fontsize,
                                  info.fontcolor,
                                  info.boxcolor,
                                  info.posx,
                                  info.posy);
    int ret = avfilter_graph_create_filter(&m_drawtext, avfilter_get_by_name("drawtext"), "drawtext", args.c_str(), nullptr, m_graph);
    LogInfo("SubtitleFilterGraph::createDrawText {}, {}", ret, args);
    return ret >= 0;
}

bool SubtitleFilterGraph::createBufferSink()
{
    int ret = avfilter_graph_create_filter(&m_sink, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, m_graph);
    return ret >= 0;
}

bool SubtitleFilterGraph::linkFilters()
{
    if (avfilter_link(m_src, 0, m_drawtext, 0) < 0)
        return false;
    if (avfilter_link(m_drawtext, 0, m_sink, 0) < 0)
        return false;
    return true;
}

int SubtitleFilterGraph::processFrame(AVFrame *in, AVFrame *&out)
{
    if (!m_initialized)
        return AVERROR(EINVAL);
    std::lock_guard lock(m_mutex);
    int ret = av_buffersrc_write_frame(m_src, in);
    if (ret < 0)
        return ret;
    return av_buffersink_get_frame(m_sink, out);
}
bool SubtitleFilterGraph::updateText(const std::string &txt)
{
    
    
    if (!m_drawtext)
    {
        LogError("SubtitleFilterGraph::updateText drawtext is null");
        return false;
    }
    std::lock_guard lock(m_mutex);
    avfilter_process_command(m_drawtext, "reinit", nullptr, nullptr, 0, 0);
    m_localTxt = txt;
    auto ret = avfilter_process_command(m_drawtext, "text", m_localTxt.c_str(), nullptr, 0, 0);
    if (ret < 0)
    {
        LogError("SubtitleFilterGraph::updateText avfilter_process_command failed {}", ret);
        return false;
    }
    // LogInfo("SubtitleFilterGraph::updateText success, {}" ,m_localTxt);
    return ret >= 0;
}

void SubtitleFilterGraph::cleanup()
{
    if (m_graph)
        avfilter_graph_free(&m_graph);
    m_src = m_sink = m_drawtext = nullptr;
    m_initialized = false;
}
