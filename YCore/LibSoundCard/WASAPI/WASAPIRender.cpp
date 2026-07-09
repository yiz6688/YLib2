#include<windows.h>
#include"../../Encoding.h"
#include<mmdeviceapi.h>
#include<format>
#include"WASAPIRender.h"
#include<Audioclient.h>
#include<print>


static long ReftimesPerSec = 10000000;
static long ReftimesPerMillisec = 10000;

WASAPIRender::WASAPIRender(WaveFormat fmt)
	:waveFormat(fmt)
{

}

WASAPIRender::~WASAPIRender()
{
	if (this->renderFuture.valid())
	{
		(void)this->stopPlay();  //丢弃返回值
		this->renderFuture.wait();

				CloseHandle(this->hEvent);
		CloseHandle(this->hExit);
		this->hEvent = NULL;
		this->hExit = NULL;
	}
}

STAType WASAPIRender::initSTA(std::string_view id)
{
	std::println("调用initSTA");
	auto fu = this->staWorker.submit([this, id]
	{
		return this->init(id);
	});
	return fu.get();
}

std::expected<void, std::string> WASAPIRender::init(std::string_view id)
{

	IMMDeviceCollection* pCollection;
	IMMDeviceEnumerator* pEnumerator;
	HRESULT hr;

	//无安全属性，手动重置 初始无信号
	this->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (this->hEvent == INVALID_HANDLE_VALUE)
	{
		return std::unexpected(std::format("{},hr={}", "CreateEvent fail", GetLastError()));
	}
	this->hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (this->hExit == INVALID_HANDLE_VALUE)
	{
		return std::unexpected(std::format("{},hr={}", "CreateEvent fail", GetLastError()));
	}



	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "获取Enumerator失败", hr));
	}

	auto u16Id = Encoding::UTF8ToUTF16(id.data());
	hr = pEnumerator->GetDevice(u16Id.c_str(), &this->pDevice);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "获取Device失败", hr));
	}

	hr = this->pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&this->pAudioClient);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "获取AudioClient失败", hr));
	}


	//初始化音频客户端
	//auto latencyRefTimes = this->latencyMilliseconds * 10000L;


	long hnsBufferDuration = this->latencyMills * 10000L;
	long hnsPeriodicity = 0;

	WAVEFORMATEX fmtEx = this->waveFormat.toWaveFormatEx();
	int streamFlags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	hr = this->pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, hnsBufferDuration, hnsPeriodicity, &fmtEx, NULL);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient Initialize fail", hr));
	}

	REFERENCE_TIME latency_time;
	hr = pAudioClient->GetStreamLatency(&latency_time);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetStreamLatency fail", hr));
	}

	hr = pAudioClient->GetBufferSize(&this->bufferFrameSize);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetBufferSize fail", hr));
	}

	hr = pAudioClient->SetEventHandle(hEvent);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient SetEventHandle fail", hr));
	}

	//IID_IAudioRenderClient
	hr = this->pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&this->pRenderClient);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetService fail", hr));
	}


	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPIRender::release()
{
	if (this->pRenderClient)
	{
		this->pRenderClient->Release();
		this->pRenderClient = nullptr;
	}

	if (this->pAudioClient)
	{
		delete this->pAudioClient;
		this->pAudioClient = nullptr;

	}

	if (this->pDevice != nullptr)
	{
		this->pDevice->Release();
		this->pDevice = nullptr;
	}

	return std::expected<void, std::string>();
}


STAType WASAPIRender::doPlay()
{
	HRESULT hr;
	auto result = this->fillBuffer(this->bufferFrameSize);
	if (!result)
	{
		return result;
	}

	ResetEvent(this->hEvent);  //重置事件
	ResetEvent(this->hExit);  //重置事件

	hr = pAudioClient->Start();
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient Start fail", hr));
	}
	this->playbackState = PlaybackState::Playing;
	long numFramesPadding;

	long actualDuration = (long)((double)ReftimesPerSec *
		this->bufferFrameSize / this->waveFormat.getSampleRate());
	int waitMilliseconds = (int)(actualDuration / ReftimesPerMillisec);

	HANDLE handles[2] = { this->hEvent, this->hExit };
	while (this->playbackState == PlaybackState::Playing)
	{
		DWORD dwSignalledIndex;
		hr = CoWaitForMultipleHandles(
					COWAIT_ALERTABLE,  // 允许在等待期间处理 APC（异步过程调用）
					waitMilliseconds,  // 超时时间：无限超时
					2,                 // 句柄数量
					handles,     // 句柄数组
					&dwSignalledIndex  // 输出：触发返回的句柄索引
				);

		if (hr == S_OK) {
			if(dwSignalledIndex == 0)  //信号事件
			{
				ResetEvent(this->hEvent);  //重置事件
			}else if(dwSignalledIndex == 1)  //退出事件
			{
				break;
			}
		}


		UINT32 numFramesPadding;
		hr = this->pAudioClient->GetCurrentPadding(&numFramesPadding);
		if (FAILED(hr))
		{
			return std::unexpected(std::format("{},hr={}", "AudioClient GetCurrentPadding fail", hr));
		}

		if (numFramesPadding < 0)
		{
			break;
		}

		auto numFramesAvaliable = this->bufferFrameSize - numFramesPadding;

		if (numFramesAvaliable > 10)
		{
			result = this->fillBuffer(numFramesAvaliable);
			if (!result)
			{
				break;
			}
		}
	}


	hr = this->pAudioClient->Stop();
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient Stop fail", hr));
	}
	this->playbackState = PlaybackState::Stopped;

	return STAType();
}

STAType WASAPIRender::fillBuffer(int frameSize)
{
	if (frameSize == 0)
	{
		return STAType();
	}

	HRESULT hr;
	
	long bytePerFrame = this->waveFormat.getBlockAlign();

	auto byteSize = frameSize * bytePerFrame;
	BYTE* pData;
	hr = this->pRenderClient->GetBuffer(frameSize, &pData);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetBuffer fail", hr));
	}

	auto readResult = this->waveReader->read((char*)pData, byteSize, 0, byteSize);
	if (!readResult)
	{
		return std::unexpected(std::format("{},hr={}", "WaveReader read fail", readResult.error()));
	}

	int readSize = readResult.value();
	for (int i = readSize; i < byteSize; i++)  //不足部分补0
	{
		pData[i] = 0;
	}
	hr = this->pRenderClient->ReleaseBuffer(frameSize, 0);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient ReleaseBuffer fail", hr));
	}
	if(readSize == 0)
	{
		this->playbackState = PlaybackState::Stopping;
	}


	return STAType();
}

std::expected<void, std::string> WASAPIRender::playAsync(WaveReader* waveReader)
{
	if (this->playbackState == PlaybackState::Starting || this->playbackState == PlaybackState::Playing)
	{
		return std::unexpected("Capture is already running");
	}
	auto fmt = waveReader->getWaveFormat();
	if (fmt != this->waveFormat)
	{
		return std::unexpected("WaveFormat mismatch");
	}

	this->waveReader = waveReader;
	this->playbackState = PlaybackState::Starting;

	this->renderFuture = this->staWorker.submit(
		[this] {
			STAType result = this->doPlay();
			if (!result)
			{
				this->playbackState = PlaybackState::Stopped;
			}
			return result;
		});



	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPIRender::waitPlayDone()
{
	if (this->renderFuture.valid())
	{
		return this->renderFuture.get();
	}
	else
	{
		return STAType();
	}
}

std::expected<void, std::string> WASAPIRender::stopPlay()
{
	if (this->playbackState != PlaybackState::Stopped && this->playbackState != PlaybackState::Stopping)
	{
		this->playbackState = PlaybackState::Stopping; //标记为停止中
		SetEvent(this->hExit);  //触发退出事件
	}
	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPIRender::play(WaveReader* waveReader)
{
	auto result = this->playAsync(waveReader);
	if (!result)
	{
		return result;
	}
	result = this->waitPlayDone();
	return result;
}
