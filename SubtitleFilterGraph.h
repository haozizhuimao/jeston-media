#pragma once
#include <string>
#ifdef _WIN32
extern "C"
{
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}
#endif
#include <mutex>
#include "tis_global/struct_details/encode_input_struct.h"

namespace _Kits {

class SubtitleFilterGraph
{
  public:
    SubtitleFilterGraph() = default;
    ~SubtitleFilterGraph();

    bool initialize(int width, int height, int format, const TIS_Info::SubtitleInputInfo &subtitleinfo);
    bool updateText(const std::string &text);
    int processFrame(AVFrame *in, AVFrame *&out);
    void cleanup();

  private:
    bool createBufferSource(int width, int height, int format);
    bool createDrawText(const TIS_Info::SubtitleInputInfo &subtitleinfo);
    bool createBufferSink();
    bool linkFilters();

  private:
    AVFilterGraph *m_graph = nullptr;
    AVFilterContext *m_src = nullptr;
    AVFilterContext *m_sink = nullptr;
    AVFilterContext *m_drawtext = nullptr;
    bool m_initialized = false;
    std::mutex m_mutex;
    std::string m_localTxt = "";
};

} // namespace _Kits
