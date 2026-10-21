
#include "Muxingvideo.h"
#include <iostream>
#include "kits/utility/log/CRossLogger.h"
// #include <Windows.h>

#define SCALE_FLAGS SWS_BICUBIC
#define STREAM_DURATION 10.0
#define STREAM_FRAME_RATE 25

namespace _Kits {

MuxingVideo::MuxingVideo()
{
    m_extradata.clear();
}

MuxingVideo::~MuxingVideo()
{
    Release();
    m_initialized = false;
}

void MuxingVideo::finished()
{
    if (!m_initialized || nullptr == m_outformt_ctx)
    {
        // LogInfo("video is not started\n");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        av_write_trailer(m_outformt_ctx);
        LogTrace("av_write_trailer");
    }

    Release();
    
    m_initialized = false;
    m_framecount = 0;
    m_start_pts = AV_NOPTS_VALUE;
    m_start_dts = AV_NOPTS_VALUE;
}

void MuxingVideo::Release()
{
    if (!m_initialized)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mtx);
    if (nullptr != m_outformt_ctx && !(m_outformt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(m_outformt_ctx->pb);
    }

    if (nullptr != m_outputstream->codecContext)
    {
        avcodec_close(m_outputstream->codecContext);
        m_outputstream->codecContext = nullptr;
    }

    m_outputstream = nullptr;
    if (nullptr != m_outformt_ctx)
    {
        avformat_free_context(m_outformt_ctx);
        m_outformt_ctx = nullptr;
    }
}

int MuxingVideo::WriteH264Packet(uint8_t* data,int size)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if(!m_initialized ||!m_outformt_ctx)
    { 
        return -1;
    }
    std::vector<uint8_t> firstdata;
    if( m_framecount == 0 && size >5 && !(data[4] == 103 || data[4] == 101))
    {
        //LogError("invalid h264 packet {} {} {}",m_framecount,size, data[4]);
        return -1;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    if(m_framecount == 0)
    {
        if(data[4] == 101)
        {
            firstdata = m_h264header;
            firstdata.insert(firstdata.end(),data,data+size);
            size += m_h264header.size();
            pkt.data= firstdata.data();  
        }
        else 
        {
            pkt.data=data;  
        }

    }
    else
    {
        pkt.data=data;
    }
    
    pkt.size=size;
    pkt.stream_index =   m_outputstream->stream->index;
    pkt.pts =     m_framecount;
    pkt.dts =       m_framecount;
    pkt.duration=1;

    av_packet_rescale_ts(&pkt,AVRational{1,m_encoderInfo.fps},m_outputstream->stream->time_base);
    int ret =av_interleaved_write_frame(m_outformt_ctx, &pkt);
    if(ret<0)
    {
        LogError("write h264 packet failed {}", ret);
        return -1;
    }
    m_framecount++;
    return 0;

}

void MuxingVideo::writeVideo(AVPacket &pkt)
{

    std::lock_guard<std::mutex> lock(m_mtx);
    if (nullptr == m_outputstream || !m_initialized || nullptr == m_outformt_ctx || nullptr == m_outputstream->stream)
    {
        LogTrace("m_outputstream is nullptr");
        return;
    }

    AVPacket local_pkt;
    if (av_packet_ref(&local_pkt, &pkt) < 0)
    {
        LogError("Failed to copy packet");
        return;
    }

    local_pkt.pts = m_framecount;
    local_pkt.dts = m_framecount;

    // if (0 == m_framecount)
    // {
    //     //local_pkt.pts = 0;
    //      if (local_pkt.pts != AV_NOPTS_VALUE)
    //     {
    //         m_start_pts = local_pkt.pts;
    //     }
    //     if (local_pkt.dts != AV_NOPTS_VALUE)
    //     {
    //         m_start_dts = local_pkt.dts;
    //     }
    //     if(local_pkt.pts != local_pkt.dts)
    //     {
    //         av_packet_unref(&local_pkt);
    //         return;
    //     }
    //     local_pkt.pts = 0;
    //     local_pkt.dts = 0;
    // }
    // else
    // {
    //     if (local_pkt.pts != AV_NOPTS_VALUE)
    //     {
    //         local_pkt.pts -= m_start_pts;
    //     }

    //     if (local_pkt.dts != AV_NOPTS_VALUE)
    //     {
    //         local_pkt.dts -= m_start_dts;
    //     }
    // }
   // av_packet_rescale_ts(&local_pkt, m_outputstream->codecContext->time_base, m_outputstream->stream->time_base);
    m_framecount++;
    local_pkt.stream_index = 0;
    local_pkt.pos = -1;

    int ret = av_interleaved_write_frame(m_outformt_ctx, &local_pkt);
    av_packet_unref(&local_pkt);
    if (ret < 0)
    {

        LogError("error while writing output packet: {}", ret);
        return;
    }
    LogTrace("writeVideo:{}", m_framecount);
    return;
}

int MuxingVideo::Init(const std::string &filenamepath, const TIS_Info::EncoderInfo &inputinfo)
{
    // auto filename = ConvertToUTF8(filenamepath.c_str());
    if (m_initialized.load())
    {
        return -1;
    }
    m_encoderInfo = inputinfo;
    std::lock_guard<std::mutex> lock(m_mtx);
    m_framecount = 0;
    m_start_pts = AV_NOPTS_VALUE;
    m_start_dts = AV_NOPTS_VALUE;

    avformat_alloc_output_context2(&m_outformt_ctx, NULL, "mp4", filenamepath.c_str());
    if (nullptr != m_outformt_ctx)
    {
        avformat_alloc_output_context2(&m_outformt_ctx, NULL, NULL, filenamepath.c_str());
    }
    if (nullptr == m_outformt_ctx)
    {
        return 1;
    }

    AVDictionary *opt = NULL;
    const AVOutputFormat *fmt = m_outformt_ctx->oformat;
    if (fmt->video_codec != AV_CODEC_ID_NONE)
    {
        m_outputstream = std::make_shared<OutputStream>();
        AddStream(m_outputstream, inputinfo);
    }

    av_dump_format(m_outformt_ctx, 0, filenamepath.c_str(), 1);
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        int ret = avio_open(&m_outformt_ctx->pb, filenamepath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            LogError("Could not open :{},ret:{}", filenamepath.c_str(), ret);
            return 1;
        }
    }
    int ret = avformat_write_header(m_outformt_ctx, NULL);
    if (ret < 0)
    {
        LogError("Error occurred when opening output file: {}", ret);
        return 1;
    }
    LogTrace("avformat_write_header:{}", filenamepath.c_str());
    m_initialized = true;
    return 0;
}

void MuxingVideo::AddStream(std::shared_ptr<OutputStream> optstream, const TIS_Info::EncoderInfo &inputinfo)
{
    optstream->encodecodec = const_cast<AVCodec *>(avcodec_find_encoder((AVCodecID)inputinfo.avcodeid));
    if (nullptr == optstream->encodecodec)
    {
        // Logger->Info( "Could not find encoder for %s", avcodec_get_name((AVCodecID)inputinfo.avcodeid));
        LogError("Could not find encoder for {}", avcodec_get_name((AVCodecID)inputinfo.avcodeid));
        return;
    }

    optstream->stream = avformat_new_stream(m_outformt_ctx, NULL);
    if (nullptr == optstream->stream)
    {
        //  Logger->Info( "Could not allocate stream");
        LogError("Could not allocate stream");
        return;
    }
    optstream->stream->id = m_outformt_ctx->nb_streams - 1;
    optstream->codecContext = avcodec_alloc_context3(optstream->encodecodec);
    if (nullptr == optstream->codecContext)
    {
        //   Logger->Info( "Could not alloc an encoding context\n");
        LogError("Could not alloc an encoding context");
        return;
    }

    switch ((optstream->encodecodec)->type)
    {
    case AVMEDIA_TYPE_VIDEO: {
        LogInfo("add video stream{},{}", inputinfo.height, inputinfo.width);
        optstream->codecContext->codec_id = (AVCodecID)inputinfo.avcodeid;
        optstream->codecContext->bit_rate = 1000000;
        optstream->codecContext->width = inputinfo.width;
        optstream->codecContext->height = inputinfo.height;
        optstream->stream->time_base = {1, inputinfo.fps};
        optstream->codecContext->framerate = {inputinfo.fps, 1};
        optstream->codecContext->time_base = {1, inputinfo.fps};
        optstream->codecContext->gop_size = inputinfo.fps / 2; /* emit one intra frame every twelve frames at most */
        optstream->codecContext->pix_fmt = (AVPixelFormat)inputinfo.pixfmt;
        if (optstream->codecContext->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        {
            optstream->codecContext->max_b_frames = 2;
        }
        break;
    }
    default:
        break;
    }
    if (m_outformt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        optstream->codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
   // avpriv_set_pts_info(optstream->stream,64,1,inputinfo.fps);
    if(!m_extradata.empty())
    {

        optstream->codecContext->extradata=(uint8_t*)av_malloc(m_extradata.size()+AV_INPUT_BUFFER_PADDING_SIZE);
        memset(optstream->codecContext->extradata+m_extradata.size(),0,AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(optstream->codecContext->extradata,m_extradata.data(),m_extradata.size());
        optstream->codecContext->extradata_size=m_extradata.size();

    }

    AVDictionary *opt = NULL;
    av_dict_copy(&opt, nullptr, 0);
    int ret = avcodec_open2(optstream->codecContext, optstream->encodecodec, &opt);
    // int ret = avcodec_open2(codecContext, encodecodec, nullptr);
    av_dict_free(&opt);
    if (ret < 0)
    {
        //  Logger->Info("Could not open video codec: %d", ret);
        LogError("Could not open video codec: {}", ret);
        return;
    }

    ret = avcodec_parameters_from_context(optstream->stream->codecpar, optstream->codecContext);
    if (ret < 0)
    {
        //  Logger->Info("Could not copy the stream parameters\n");
        LogError("Could not copy the stream parameters");
        return;
    }
}

void MuxingVideo::SetH264Header(uint8_t* data,int size)
{
    int sps=-1;
    int pps=-1;
    int pps_end = size;
    for(int i=0;i<size-4;i++)
    {
        if(data[i]==0x00 && data[i+1]==0x00 && data[i+2]==0x00 && data[i+3]==0x01)
        {
            int type=data[i+4]&0x1f;
            if(type==7)
            {
                sps=i;
            }
            if(type==8)
            {
                pps=i;
            }
            if(type==5)
            {
                pps_end=i;
                break;
            }
        }

    }

    if(sps<0 || pps<0)
    {
        LogError("not find SPS PPS");
        return;
    }
    m_h264header.insert(m_extradata.end(),data,data+pps_end);
    for (size_t i = 0; i < m_h264header.size(); i++)
    {
         LogError("m_h264header,i {} {}",i ,m_h264header[i]);
    }
    
    uint8_t* sps_data=data+sps+4;
    uint8_t* pps_data=data+pps+4;

    int sps_size=pps-sps-4;
    int pps_size=pps_end-pps-4;

    _Kits::LogInfo("sps size {},pps size {}",sps_size,pps_size);

    m_extradata.clear();
    m_extradata.push_back(0x01);
    m_extradata.push_back(sps_data[1]);
    m_extradata.push_back(sps_data[2]);
    m_extradata.push_back(sps_data[3]);
    m_extradata.push_back(0xff);
    m_extradata.push_back(0xe1);
    m_extradata.push_back((sps_size>>8)&0xff);
    m_extradata.push_back(sps_size&0xff);

    m_extradata.insert(m_extradata.end(),sps_data,sps_data+sps_size);

    m_extradata.push_back(0x01);
    m_extradata.push_back((pps_size>>8)&0xff);
    m_extradata.push_back(pps_size&0xff);
    m_extradata.insert(m_extradata.end(),pps_data,pps_data+pps_size);
    LogInfo( "extradata size {}",m_extradata.size());

}

} // namespace _Kits
