#include "EncodeVideoOperate.h"
#include "kits/utility/log/CRossLogger.h"
#include <chrono>
#include <ranges>

namespace _Kits {
EncodeVideoOperate::EncodeVideoOperate()
{
    m_muxingVideo = std::make_unique<MuxingVideo>();
}

void EncodeVideoOperate::initEncoderInfo(const TIS_Info::EncoderInfo &encoderinfo, const TIS_Info::SubtitleInputInfo &subtitleinfo)
{

    m_encoderInfo = encoderinfo;
    if (nullptr == m_muxingVideo)
    {
        m_muxingVideo = std::make_unique<MuxingVideo>();
    }

#ifdef _WIN32
    {
        if (nullptr == m_encodeVideo)
        {
            m_encodeVideo = std::make_unique<FFMPEGEncodeVideo>();
        }
        auto writecallfunck = std::bind(&EncodeVideoOperate::writeVideofunc, this, std::placeholders::_1);

        m_encodeVideo->Initencoder(encoderinfo, writecallfunck, subtitleinfo);
    }
#else
    {
        m_jestonEncode = std::make_unique<JestonEncode>();
        if (nullptr != m_jestonEncode)
        {
            m_jestonEncode->InitEncoder(encoderinfo, [this](uint8_t *rgb, uint32_t size) {
                if (nullptr != m_muxingVideo)
                {
                    if (m_first)
                    {
                        m_first = false;
                        m_muxingVideo->SetH264Header(rgb, size);
                    }
                    m_muxingVideo->WriteH264Packet(rgb, size);
                    // LogError("jeston callback : {}",size);
                }
            });
        }
    }

#endif

    m_bRunFlag.store(true);
    m_encodeVideFlag.store(true);
    m_encodeThread = std::thread(&EncodeVideoOperate::encodeThreadFunc, this);
    LogInfo("EncodeVideoOperate::initEncoderInfo finish");
}
EncodeVideoOperate::~EncodeVideoOperate()
{
    stopRecord();
    m_bRunFlag.store(false);
    m_imagebufferQueue.stop();
    if (m_encodeThread.joinable())
    {
        m_encodeThread.join();
    }
}
void EncodeVideoOperate::writeVideofunctest(int no, AVPacket &pkt, int flag)
{
    if (nullptr != m_sendLiveStreamCallback)
    {
        m_sendLiveStreamCallback(pkt.data, pkt.size);
    }
}
void EncodeVideoOperate::writeVideofunc(AVPacket &pkt)
{
    if (nullptr != m_sendLiveStreamCallback)
    {
        m_sendLiveStreamCallback(pkt.data, pkt.size);
    }
    if (nullptr != m_muxingVideo)
    {
        m_muxingVideo->writeVideo(pkt);
    }
}

void EncodeVideoOperate::saveImage(std::shared_ptr<TIS_Info::ImageBuffer> imagebuffer)
{
    if (!m_bRunFlag.load())
    {
        return;
    }
    m_imagebufferQueue.pushItemNoBlocking(imagebuffer);
}

void EncodeVideoOperate::startRecord(const std::string &savePath)
{
    if (m_bSaveVideoFlag.load())
    {
        return;
    }
    m_muxingVideo->finished();
    m_muxingVideo->Init(savePath, m_encoderInfo);

    m_bSaveVideoFlag.store(true);
}
void EncodeVideoOperate::stopRecord()
{
    if (!m_bSaveVideoFlag.load())
    {
        return;
    }
    m_muxingVideo->finished();
    m_bSaveVideoFlag.store(false);
}
void EncodeVideoOperate::updateSubtitles(const std::string &subtitles)
{
    m_currentSubtitles = subtitles;
}
void EncodeVideoOperate::encodeThreadFunc()
{
    LogInfo("EncodeVideoOperate::encodeThreadFunc start");
    while (m_bRunFlag.load())
    {
        auto imagebuffer = m_imagebufferQueue.popItem();

        if (m_encodeVideFlag.load() && std::nullopt != imagebuffer)
        {
#ifdef ARM_PLATFORM
            {
                m_jestonEncode->WriteRGBData((*imagebuffer)->data.data(),
                                             (*imagebuffer)->width,
                                             (*imagebuffer)->height,
                                             (*imagebuffer)->pixFormat,
                                             m_currentSubtitles.c_str());
            }
#else
            {
                m_encodeVideo->WriteRGBData((*imagebuffer)->data.data(),
                                            (*imagebuffer)->width,
                                            (*imagebuffer)->height,
                                            (*imagebuffer)->pixFormat,
                                            m_currentSubtitles.c_str());
            }
#endif
        }
    }
}

void EncodeVideoOperate::initLiveStreamerCallback(SendLiveStreamCallBack callback)
{
    m_sendLiveStreamCallback = callback;
}

} // namespace _Kits