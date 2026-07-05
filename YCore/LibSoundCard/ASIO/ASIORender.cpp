#include "ASIORender.h"
#include"../../Utils.h"

ASIORender::ASIORender(ASIODriver* driver, int channelMask)
	: pDriver(driver)
{
	this->_channels = Utils::getBitPos(channelMask);
}

std::expected<void, std::string> ASIORender::playAsync(WaveReader* waveReader)
{
	return std::expected<void, std::string>();
}

std::expected<void, std::string> ASIORender::waitPlayDone()
{
	return std::expected<void, std::string>();
}

std::expected<void, std::string> ASIORender::stopPlay()
{
	return std::expected<void, std::string>();
}

std::expected<void, std::string> ASIORender::play(WaveReader* waveReader)
{
	return std::expected<void, std::string>();
}

