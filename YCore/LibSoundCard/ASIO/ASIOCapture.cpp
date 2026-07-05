#include "ASIOCapture.h"
#include"ASIODriver.h"
#include"../../Utils.h"


ASIOCapture::ASIOCapture(ASIODriver* driver, int channelMask)
	:pDriver{driver}
{
	this->_channels = Utils::getBitPos(channelMask);
}

ASIOCapture::~ASIOCapture()
{}

std::expected<void, std::string> ASIOCapture::captureAsync(WaveWriter * waveWriter, int maxRecordMills)
{
	return std::expected<void, std::string>();
}

std::expected<void, std::string> ASIOCapture::waitCaptureDone()
{
	return std::expected<void, std::string>();
}

std::expected<void, std::string> ASIOCapture::stopCapture()
{
	return std::expected<void, std::string>();
}

std::expected<void, std::string> ASIOCapture::capture(WaveWriter* waveWriter, int maxRecordMills)
{
	return std::expected<void, std::string>();
}

