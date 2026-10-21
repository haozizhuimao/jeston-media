#include "VideoManage.h"
#include <filesystem>
#include "kits/utility/log/CRossLogger.h"

namespace _Kits {
VideoManage::VideoManage(const std::string &name) : m_name(name)
{
    m_dealThread = std::thread(&VideoManage::run, this);
}

VideoManage::~VideoManage()
{
    m_isRun.store(false);
    m_imagebufferQueue.stop();
    if (m_dealThread.joinable())
    {
        m_dealThread.join();
    }
}

void VideoManage::init(const TIS_Info::EncoderInfo &encoderinfo, const TIS_Info::SubtitleInputInfo &subtitleinfo)
{
    if (m_isInit.load())
    {
        LogInfo("VideoManage::is inited");
        return;
    }
    if (nullptr == m_encodeVideoOperate)
    {
        m_encodeVideoOperate = std::make_unique<EncodeVideoOperate>();
    }
    m_encoderInfo = encoderinfo;
    m_encoderInfo.subtitleInputInfo = subtitleinfo;
    if (0 >= m_encoderInfo.height || 0 >= m_encoderInfo.width)
    {
        LogError("VideoManage::init height or width is 0");
        return;
    }
    m_encodeVideoOperate->initEncoderInfo(encoderinfo, subtitleinfo);
    m_isInit.store(true);
    LogInfo("VideoManage::init finish");
}
bool VideoManage::isInit() const
{
    return m_isInit.load();
}
void VideoManage::setVideoSaveRootPath(const std::string &path)
{
    m_videoRootPath = path + "/" + m_name;
    createDirectory(m_videoRootPath);
}

void VideoManage::createDirectory(const std::string &strpath)
{
    std::filesystem::path path = std::filesystem::u8path(strpath);
    if (!std::filesystem::exists(path))
    {
        if (std::filesystem::create_directories(path))
        {
            LogInfo("Directories created successfully:{}", path.string());
        }
        else
        {
            LogError("Failed to create directories:{}", path.string());
        }
    }
    else
    {
        LogInfo("Directories already exist:{}", path.string());
    }
}
void VideoManage::start()
{
    m_isRun.store(true);
}
void VideoManage::updateSubtitles(const std::string &subtitles)
{
    if (nullptr != m_encodeVideoOperate)
    {
        m_encodeVideoOperate->updateSubtitles(subtitles);
    }
}
void VideoManage::updataVideoName(const std::string &name)
{
    auto videoName = m_videoRootPath + "/" + name;
    if (name.empty())
    {
        if (nullptr != m_encodeVideoOperate)
        {
            m_encodeVideoOperate->stopRecord();
        }
        return;
    }

    if (nullptr != m_encodeVideoOperate)
    {
        m_encodeVideoOperate->stopRecord();
        m_encodeVideoOperate->startRecord(videoName);
    }
}
void VideoManage::stop()
{
    m_isRun.store(false);
    if (nullptr != m_encodeVideoOperate)
    {
        m_encodeVideoOperate->stopRecord();
    }
}

void VideoManage::pushImage(std::shared_ptr<TIS_Info::ImageBuffer> imagebuffer)
{
    //_Kits::LogInfo("pushImage {}",imagebuffer->name);     
    m_imagebufferQueue.pushItemNoBlocking(imagebuffer);
}

void VideoManage::setLiveStreamCallBack(SendLiveStreamCallBack callback)
{
    if(nullptr != m_encodeVideoOperate)
    {
        m_encodeVideoOperate->initLiveStreamerCallback(callback);
    }
}

void VideoManage::run()
{
    int count = 0;
    while (m_isRun)
    {
        auto imagebuffer = m_imagebufferQueue.popItem();
        if (std::nullopt == imagebuffer)
        {
            break;
        }
        if (nullptr != m_encodeVideoOperate)
        {
            m_encodeVideoOperate->saveImage(*imagebuffer);
        }
        count++;
    }
}

} // namespace _Kits