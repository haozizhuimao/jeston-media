#include <stdint.h>
#include "tis_global/struct_details/encode_input_struct.h"
#include "tis_global/enum_public/video_enum.h"
#include <functional>
#include <thread>
#include <atomic>

#include "kits/common/jeston_multimdeia/NvUtils.h"
#include "kits/common/jeston_multimdeia/NvBufSurface.h"
#include "kits/common/jeston_multimdeia/NvVideoEncoder.h"



class JestonEncode
{

public:
    JestonEncode(/* args */);
    ~JestonEncode();

    /** * @brief  初始化编码
     * @param   encoderinfo   视频参数信息
     * @param   writefilecallback   写入文件回调
     * @return  0: sucess ** **/
    int InitEncoder(const TIS_Info::EncoderInfo& encoderinfo, std::function<void(uint8_t*,uint32_t)>  writefilecallback);
    /** * @brief  推入图片数据
     * @param   rgb   rgb图片数据
     * @param   size  图片大小
     * @param   srcpixfmt  图片格式
     * @return  0: sucess ** **/
    int WriteRGBData(const uint8_t *rgb,int width,int height,int srcpixfmt,const std::string& subtitles);

private:
    uint32_t v4l2PixelFormat(TIS_Info::AVPixelFormat pix_fmt, bool multi_planar = true);
    int fill_nvbuffer_from_yuv420p(const uint8_t *src_yuv, NvBuffer &buffer);
    void consumerFunc();

private:
    NvVideoEncoder* m_enc {nullptr};
    std::function<void(uint8_t*,uint32_t)>  m_writefilecallback {nullptr};
    std::thread m_consumer_thread;
    std::atomic_bool m_runEncode {false};
    uint32_t m_frame_index { 0 };
    ///std::ofstream m_h264_file;
};


