#pragma once
#include "tis_global/struct_details/encode_input_struct.h"

#ifdef _WIN32
extern "C"
{
#include "ffmpeg/libavformat/avformat.h"
#include "ffmpeg/libavcodec/avcodec.h"
};
#endif

#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>
#include <vector>

namespace _Kits {

class MuxingVideo final
{

    typedef struct OutputStream
    {
        AVCodec *encodecodec = nullptr;
        AVCodecContext *codecContext = nullptr;
        AVStream *stream = nullptr;
        uint8_t extradata[256];
        int rkflag = 0;
    } OutputStream;

  private:
    AVFormatContext *m_outformt_ctx = nullptr;
    std::atomic_bool m_initialized = false;
    std::shared_ptr<OutputStream> m_outputstream = nullptr;
    std::mutex m_mtx;
    int m_framecount = 0;
    int64_t m_start_pts = AV_NOPTS_VALUE; // 记录当前录制段的起始PTS
    int64_t m_start_dts = AV_NOPTS_VALUE; // 记录当前录制段的起始DTS
    TIS_Info::EncoderInfo m_encoderInfo;

  public:
    MuxingVideo();
    ~MuxingVideo();

    /** * @brief 结束存储
     * @param
     * @return  ** **/
    void finished();

    /** * @brief 初始化编码信息
     * @param   filename   编码后文件名**/
    int Init(const std::string &filename, const TIS_Info::EncoderInfo &inputinfo);

    void writeVideo(AVPacket &pkt);

    int WriteH264Packet(uint8_t *data, int size);

    void SetH264Header(uint8_t *data, int size);

  private:
    std::vector<uint8_t> m_extradata;
    std::vector<uint8_t> m_h264header;
    /** * @brief 写入视频头部信息
     * @return ** **/
    int WriteHeader();

    /** * @brief 释放资源
     * @return ** **/
    void Release();

    /** * @brief ffmpeg添加视频流信息
     * @param   optstream   视频流信息
     * @param   InputInfo   视频输入参数
     * @return   ** **/
    void AddStream(std::shared_ptr<OutputStream> optstream, const TIS_Info::EncoderInfo &inputinfo);

    void JetsonAddStream(std::shared_ptr<OutputStream> optstream, const TIS_Info::EncoderInfo &inputinfo);
};

} // namespace _Kits
