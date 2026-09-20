#include"ASIOClient.h"
#include"../../BitConverter.h"
#include"ASIODriver.h"

TResult<void> ASIOClient::init(CLSID clsid)
{
    auto aa = ASIODriver::createDriver(clsid); //创建驱动

    if (!aa)
    {
        return std::unexpected(aa.error());  //返回失败。
    }

    return TResult<void>();
}

int ASIOClient::getCaptureCount()
{
    return this->pDriver->getCaptureCount();
}

int ASIOClient::getRenderCount()
{
    return this->pDriver->getRenderCount();
}

std::string ASIOClient::getCaptureName(int ch)
{
    return this->pDriver->getCaptureName(ch);
}

std::string ASIOClient::getRenderName(int channel)
{
    return this->pDriver->getRenderName(channel);
}

TResult<int> ASIOClient::getSampleRate()
{
    return this->pDriver->getSampleRate();
}

TResult<void> ASIOClient::setSampleRate(long sampleRate)
{
    return this->pDriver->setSampleRate(sampleRate);
}

TResult<ICapture *> ASIOClient::getCapture(std::initializer_list<int> lst)
{
    return this->pDriver->createCapture(BitConverter::setBitIndex(lst));
}

TResult<IRender *> ASIOClient::getRender(std::initializer_list<int> lst)
{
    return this->pDriver->createRender(BitConverter::setBitIndex(lst));
}

//设置通道掩码(启动前, 只能设置一次)并创建缓冲区
TResult<void> ASIOClient::Initialize(unsigned inputMask, unsigned outputMask)
{
    return this->pDriver->Initialize(inputMask, outputMask);
}

TResult<void> ASIOClient::Release()
{
    return this->pDriver->Release();
}
