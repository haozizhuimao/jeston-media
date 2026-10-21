#include "JestonEncode.h"
#include "kits/utility/log/CRossLogger.h"

JestonEncode::JestonEncode(/* args */)
{
}

JestonEncode::~JestonEncode()
{
    m_runEncode.store(false);
    if(m_consumer_thread.joinable())
    {
        m_consumer_thread.join();
    }
}

int JestonEncode::InitEncoder(const TIS_Info::EncoderInfo& encoderinfo, std::function<void(uint8_t*,uint32_t)>  writefilecallback)
{
    m_enc = NvVideoEncoder::createVideoEncoder("enc0");  //阻塞模式
    uint32_t sizeImage = encoderinfo.width * encoderinfo.height * 3;
    // _Kits::LogInfo("(TIS_Info::AVPixelFormat)encoderinfo.pixfmt) {} {} {} {}",v4l2PixelFormat((TIS_Info::AVPixelFormat)encoderinfo.pixfmt),
    // encoderinfo.pixfmt ,encoderinfo.width,encoderinfo.height);  //875967048
    auto ret =  m_enc->setCapturePlaneFormat(V4L2_PIX_FMT_H264, encoderinfo.width,encoderinfo.height, sizeImage);

    ret =  m_enc->setOutputPlaneFormat(v4l2PixelFormat((TIS_Info::AVPixelFormat)encoderinfo.pixfmt), encoderinfo.width,encoderinfo.height);
    uint32_t bitrate = 4 * 1024 * 1024;
    ret = m_enc->setBitrate(bitrate);
    _Kits::LogInfo("m_enc->setBitrate {}",ret);
    ret = m_enc->setFrameRate(encoderinfo.fps, 1);
    _Kits::LogInfo("m_enc->setFrameRate {}",ret);
    ret = m_enc->setRateControlMode (V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
    _Kits::LogInfo("m_enc->setRateControlMode {}",ret);

    ret = m_enc->setProfile (V4L2_MPEG_VIDEO_H264_PROFILE_MAIN);
    _Kits::LogInfo("m_enc->setProfile {}",ret);
    ret = m_enc->setLevel (V4L2_MPEG_VIDEO_H264_LEVEL_5_1);
    _Kits::LogInfo("m_enc->setLevel {}",ret);

    ret = m_enc->output_plane.setupPlane (V4L2_MEMORY_MMAP, 10, true, false);
    _Kits::LogInfo("m_enc->output_plane.setupPlane {}",ret);

    ret = m_enc->capture_plane.setupPlane (V4L2_MEMORY_MMAP,10,true, false);
    _Kits::LogInfo("m_enc->output_plane.setupPlane {}",ret);

    ret = m_enc->output_plane.setStreamStatus (true);
    _Kits::LogInfo("m_enc->capture_plane.setStreamStatus {}",ret);

    ret = m_enc->capture_plane.setStreamStatus (true);
    _Kits::LogInfo("m_enc->capture_plane.setStreamStatus {}",ret);

    ret = m_enc->setInsertSpsPpsAtIdrEnabled (true);
    _Kits::LogInfo("m_enc->setInsertSpsPpsAtIdrEnabled {}",ret);

    for(uint32_t i=0;i<m_enc->capture_plane.getNumBuffers();i++)
    {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[MAX_PLANES];
        memset(&buf,0,sizeof(buf));
        memset(planes,0,sizeof(planes));
        buf.index=i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes=planes;
        ret = m_enc->capture_plane.qBuffer( buf,nullptr);
        if(ret<0)
        {
            _Kits::LogInfo("capture qbuffer failed{}",ret);
            return -1;
        }
        _Kits::LogInfo("capture qbuffer {}",ret);
    }
    m_writefilecallback = writefilecallback;
    m_runEncode.store(true);
    m_consumer_thread = std::thread(&JestonEncode::consumerFunc,this);
    return 0;
}

int JestonEncode::fill_nvbuffer_from_yuv420p(const uint8_t *src_yuv, NvBuffer &buffer) 
{
    // 源数据指针
    const uint8_t *src  = src_yuv;

    for (uint32_t i = 0; i < buffer.n_planes; i++) {
        NvBuffer::NvBufferPlane &plane = buffer.planes[i];
        
        // 源数据每行有效字节数 = 宽度 × 每像素字节数（YUV420 为 1）
        size_t bytes_per_row_src = plane.fmt.bytesperpixel * plane.fmt.width;
        
        // 目标缓冲区起始地址
        char *dst = (char *) plane.data;

        // 逐行拷贝
        for (uint32_t j = 0; j < plane.fmt.height; j++) {
            memcpy(dst, src, bytes_per_row_src);
            src += bytes_per_row_src;        // 源跳到下一行（紧凑，无填充）
            dst += plane.fmt.stride;         // 目标跳到下一行（跳过填充）
        }

        // 记录实际使用大小（包含填充）
        plane.bytesused = plane.fmt.stride * plane.fmt.height;
    }

    return 0;
}

int JestonEncode::WriteRGBData(const uint8_t *rgb,int width,int height,int srcpixfmt,const std::string& subtitles)
{

    uint32_t count = m_enc->output_plane.getNumBuffers();

    struct v4l2_buffer v4l2_output_buf;
    struct v4l2_plane output_planes[MAX_PLANES];
    memset(&v4l2_output_buf, 0, sizeof(v4l2_output_buf));
    memset(output_planes, 0, sizeof(output_planes));
    v4l2_output_buf.m.planes = output_planes;
    NvBuffer* buffer = nullptr;
    if(m_frame_index < m_enc->output_plane.getNumBuffers())
    {
        // 第一次几个buffer直接使用
        buffer = m_enc->output_plane.getNthBuffer(m_frame_index);
        v4l2_output_buf.index= m_frame_index;
        m_frame_index++;
    }
    else
    {
        int ret = m_enc->output_plane.dqBuffer(v4l2_output_buf, &buffer,nullptr, 1000);
        if(ret < 0)
        {
            _Kits::LogError("dqBuffer failed {}",ret);
            return -1;
        }
    }

    fill_nvbuffer_from_yuv420p(rgb, *buffer);

    // 3. 获取 NvBufSurface 句柄
    for (uint32_t j = 0 ; j < buffer->n_planes; j++)
    {
        NvBufSurface *surf = nullptr;
       auto ret = NvBufSurfaceFromFd(buffer->planes[j].fd, (void**)&surf);
        if (ret < 0) {
            _Kits::LogError("NvBufSurfaceFromFd failed {}",ret);
            m_enc->output_plane.qBuffer(v4l2_output_buf, NULL);
            return -1;
        }
        ret = NvBufSurfaceSyncForDevice (surf, 0, j);
    }
    // 6. 将填充好的缓冲区重新入队
   auto  ret = m_enc->output_plane.qBuffer(v4l2_output_buf, NULL);
    if (ret < 0) {
         _Kits::LogError("NvBufSurfaceFromFd failed {}",ret);
        return -1;
    }
    return 0;      
}

void JestonEncode::consumerFunc()
{
    while (m_runEncode.load()) 
    {
        struct v4l2_buffer v4l2_capture_buf;
        struct v4l2_plane capture_planes[MAX_PLANES];
        NvBuffer *capplane_buffer = NULL;
        bool capture_dq_continue = true;

        memset(&v4l2_capture_buf, 0, sizeof(v4l2_capture_buf));
        memset(capture_planes, 0, sizeof(capture_planes));
        v4l2_capture_buf.m.planes = capture_planes;
        v4l2_capture_buf.length = 1;

        //struct v4l2_buffer buf;
        NvBuffer* buffer = nullptr;
        // 1. 阻塞在这里，直到硬件编码完了一帧，吐出码流
        m_enc->capture_plane.dqBuffer(v4l2_capture_buf, &buffer, NULL, -1);

        if (buffer->planes[0].bytesused == 0)
        {
            std::cout << "Got 0 size buffer in capture \n";
            continue;
        }
        if(nullptr != m_writefilecallback)
        {
            m_writefilecallback((uint8_t *) buffer->planes[0].data, buffer->planes[0].bytesused);

        }
        uint8_t *h264_data = buffer->planes[0].data;
        uint32_t h264_size = buffer->planes[0].bytesused;
        m_enc->capture_plane.qBuffer(v4l2_capture_buf, NULL);
    }
}

uint32_t JestonEncode::v4l2PixelFormat(TIS_Info::AVPixelFormat pix_fmt, bool multi_planar) 
{
    // 注意：Jetson硬件通常要求multi-planar格式（后缀M）
    // 若multi_planar为false，可返回单平面格式（但硬件效率低）
    //using namespace v4l2;
    switch (pix_fmt) {
        case TIS_Info::AVPixelFormat::AV_PIX_FMT_YUV420P:
            return multi_planar ? V4L2_PIX_FMT_YUV420M : V4L2_PIX_FMT_YUV420;
        case TIS_Info::AVPixelFormat::AV_PIX_FMT_YUV422P:
            return multi_planar ? V4L2_PIX_FMT_YUV422M : V4L2_PIX_FMT_YUV422P;
        case TIS_Info::AVPixelFormat::AV_PIX_FMT_YUV444P:
            return multi_planar ? V4L2_PIX_FMT_YUV444M : V4L2_PIX_FMT_YUV444;
        case TIS_Info::AVPixelFormat::AV_PIX_FMT_NV12:
            return multi_planar ? V4L2_PIX_FMT_NV12M : V4L2_PIX_FMT_NV12;
        case TIS_Info::AVPixelFormat::AV_PIX_FMT_NV21:
            return multi_planar ? V4L2_PIX_FMT_NV21M : V4L2_PIX_FMT_NV21;
        default:
            return 0; // 不支持的格式
    }
}