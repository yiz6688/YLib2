#include"base_config.hpp"
#include "ASIORender.h"
#include"ASIODriver.h"
#include"../../Utils.h"

ASIORender::ASIORender(ASIODriver* driver, int channelMask, int bufferMills)
    : pDriver(driver)
{
    this->_channels = Utils::getBitPos(channelMask);
    //注册客户端(缓冲由驱动统一持有, 注销时移入GC, 等引擎放弃旧视图后释放)
    this->_client = driver->registerClient(this->_channels, ASIOFalse, bufferMills);
}

ASIORender::~ASIORender()
{
    if (this->_client != nullptr)
    {
        this->pDriver->removeClient(this->_client);
        this->_client = nullptr;
    }
}

WaveBuffer* ASIORender::getBuffer(int channelIndex)
{
    if (this->_client == nullptr || channelIndex < 0 || channelIndex >= (int)this->_client->wbs.size())
    {
        return nullptr;
    }
    return this->_client->wbs[channelIndex].get();
}

//底层架构已改动: 音频交换统一走 getBuffer(每通道 WaveBuffer), 文件级播放由上层基于 getBuffer 实现,
//WaveReader 流式播放接口不再支持, 启动即失败以尽早暴露误用
TResult<void> ASIORender::playAsync(WaveReader* waveReader)
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 写入通道缓冲, WaveReader 接口不再支持");
}

TResult<void> ASIORender::waitPlayDone()
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 写入通道缓冲, 无流式播放任务");
}

TResult<void> ASIORender::stopPlay()
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 写入通道缓冲, 无流式播放任务");
}

TResult<void> ASIORender::play(WaveReader* waveReader)
{
    return exp_ns::unexpected("架构已改动: 请通过 getBuffer 写入通道缓冲, WaveReader 接口不再支持");
}
