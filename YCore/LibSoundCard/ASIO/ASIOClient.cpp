#include"ASIOClient.h"


#include"ASIODriver.h"

ASIOClient::ASIOClient(CLSID clsid)
    :pDriver{nullptr}
{

}

TResult<void> ASIOClient::init(CLSID clsid)
{
    auto aa = ASIODriver::createDriver(clsid); //创建驱动

    if(!aa)
    {
        return std::unexpected(aa.error());  //返回失败。
    }





    return TResult<void>();
}

int ASIOClient::getCaptureCount()
{
    int size1 = this->pDriver->pAsioDevice->inputChannels.size();
    return 0;
}

int ASIOClient::getRenderCount()
{
    int size1 = this->pDriver->pAsioDevice->inputChannels.size();
    return 0;
}

std::string ASIOClient::getCaptureName(int ch)
{
    auto& channels = this->pDriver->pAsioDevice->inputChannels;
    auto name = channels[ch];
    return std::string();
}

std::string ASIOClient::getRenderName(int channel)
{
    return std::string();
}

int ASIOClient::getSampleRate()
{
    return 0;
}

TResult<void> ASIOClient::setSampleRate(int sampleRate)
{
    this->pDriver->pAsioDevice->setSampleRate(sampleRate);
    return TResult<void>();
}
