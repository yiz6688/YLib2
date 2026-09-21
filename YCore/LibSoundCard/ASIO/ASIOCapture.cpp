#include"base_config.hpp"
#include "ASIOCapture.h"
#include"ASIODriver.h"
#include"../../Utils.h"

ASIOCapture::ASIOCapture(ASIODriver* driver, int channelMask, int bufferMills)
    : pDriver{driver}
{
    this->_channels = Utils::getBitPos(channelMask);
    //注册客户端(缓冲由驱动统一持有, 注销时移入GC, 等引擎放弃旧视图后释放)
    this->_client = driver->registerClient(this->_channels, ASIOTrue, bufferMills);
}

ASIOCapture::~ASIOCapture()
{
    if (this->_client != nullptr)
    {
        this->pDriver->removeClient(this->_client);
        this->_client = nullptr;
    }
}

WaveBuffer* ASIOCapture::getBuffer(int channelIndex)
{
    if (this->_client == nullptr || channelIndex < 0 || channelIndex >= (int)this->_client->wbs.size())
    {
        return nullptr;
    }
    return this->_client->wbs[channelIndex].get();
}

//底层架构已改动: 音频交换统一走 getBuffer(每通道 WaveBuffer), 文件级录制由上层基于 getBuffer 实现,
//WaveWriter 流式录制接口不再支持, 启动即失败以尽早暴露误用
TResult<void> ASIOCapture::captureAsync(WaveWriter * waveWriter, int maxRecordMills)
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 读取通道缓冲, WaveWriter 接口不再支持");
}

TResult<void> ASIOCapture::waitCaptureDone()
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 读取通道缓冲, 无流式录制任务");
}

TResult<void> ASIOCapture::stopCapture()
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 读取通道缓冲, 无流式录制任务");
}

TResult<void> ASIOCapture::capture(WaveWriter* waveWriter, int maxRecordMills)
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 读取通道缓冲, WaveWriter 接口不再支持");
}
