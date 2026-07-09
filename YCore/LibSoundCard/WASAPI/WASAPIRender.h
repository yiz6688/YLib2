#include"../IRender.h"
#include"../STAWorker.h"
#include<Windows.h>
#include"../../WaveRingBuffer.h"




class IMMDevice;
class IAudioClient;
class IAudioRenderClient;
class WASAPIRender : public IRender
{
public:
	WASAPIRender(WaveFormat fmt);
	~WASAPIRender();


	STAType initSTA(std::string_view id);

	std::expected<void, std::string> init(std::string_view id);

	std::expected<void, std::string> release();

	std::expected<void, std::string> doPlay();

	STAType fillBuffer(int frameSize);

	std::expected<void, std::string> playAsync(WaveReader* waveReader) override;

	std::expected<void, std::string> waitPlayDone() override;

	std::expected<void, std::string> stopPlay() override;

	std::expected<void, std::string> play(WaveReader* waveReader) override;

	PlaybackState getPlaybackState() override
	{
		return this->playbackState;
	}



private:
	WaveRingBuffer* ringBuffer = nullptr;
	WaveReader* waveReader = nullptr;
	STAWorker staWorker;
	PlaybackState playbackState = PlaybackState::Stopped;
	IMMDevice* pDevice = nullptr;
	IAudioClient* pAudioClient = nullptr;
	IAudioRenderClient* pRenderClient = nullptr;
	WaveFormat waveFormat;
	HANDLE hEvent = nullptr;
	HANDLE hExit = nullptr;
	STAFuture renderFuture;

	long latencyMills = 100;

	UINT32 bufferFrameSize = 0;

	WaveReader *reader = nullptr;
};