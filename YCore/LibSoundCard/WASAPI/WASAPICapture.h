#include"../ICapture.h"
#include"../STAWorker.h"
#include<Windows.h>
#include"../../WaveRingBuffer.h"

class IMMDevice;
class IAudioClient;
class IAudioCaptureClient;
class WASAPICapture : public ICapture
{
public:
	WASAPICapture(WaveFormat _waveFormat);
	~WASAPICapture();


	std::expected<void, std::string> init(std::string_view id);

	std::expected<void, std::string> release();

	std::expected<void, std::string> doCapture();

	STAType readNextPacket();

	std::expected<void, std::string> captureAsync(WaveWriter* waveWriter, int maxRecordMills) override;

	std::expected<void, std::string> waitCaptureDone() override;

	std::expected<void, std::string> stopCapture() override;

	std::expected<void, std::string> capture(WaveWriter* waveWriter, int maxRecordMills) override;

	CaptureState getCaptureState()
	{
		return this->captureState;
	}



private:
	WaveRingBuffer *ringBuffer = nullptr;
	WaveWriter* waveWriter = nullptr;
	STAWorker staWorker;
	CaptureState captureState = CaptureState::Stopped;
	IMMDevice* pDevice = nullptr;
	IAudioClient* pAudioClient = nullptr;
	IAudioCaptureClient* pCaptureClient = nullptr;
	WaveFormat waveFormat;
	HANDLE hEvent = nullptr;
	STAFuture captureFuture;
	
	long latencyMills = 100;


	UINT32 bufferFrameSize = 0;

};