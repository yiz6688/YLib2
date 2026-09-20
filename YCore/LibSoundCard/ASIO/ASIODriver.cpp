#include"ASIODriver.h"
#include"asiosdk/asio.h"
#include<ranges>
#include<array>
#include<print>
#include"NumUtils.h"

using namespace std;

constexpr unsigned MAX_DRIVER_NUM = 2;  //最大支持的驱动数量

constexpr int BUFFER_NOTIFY_MILLS = 20;   //系统通知的间隔
constexpr int BUFFER_MAX_MILLS = 100;     //最大系统缓冲区



struct ASIOEntity
{
	unique_ptr<ASIODriver> pASIODriver = nullptr;
	//CLSID clsid;
	ASIOCallbacks callback = {};
};


//回调函数模板，批量生成回调函数
template<unsigned N>
struct __ASIOCALLBACK__
{
	static void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
	{
		asioEntity->pASIODriver->bufferProcess(doubleBufferIndex, directProcess);
	}
	static void sampleRateDidChange(ASIOSampleRate sRate)
	{

	}
	static long asioMessage(long selector, long value, void* message, double* opt)
	{
		long ret = 0;
		switch (selector)
		{
		case kAsioSelectorSupported:
			if (value == kAsioResetRequest
				|| value == kAsioEngineVersion
				|| value == kAsioResyncRequest
				|| value == kAsioLatenciesChanged
				// the following three were added for ASIO 2.0, you don't necessarily have to support them
				|| value == kAsioSupportsTimeInfo
				|| value == kAsioSupportsTimeCode
				|| value == kAsioSupportsInputMonitor)
				ret = 1L;
			break;
		case kAsioResetRequest:
			// defer the task and perform the reset of the driver during the next "safe" situation
			// You cannot reset the driver right now, as this code is called from the driver.
			// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
			// Afterwards you initialize the driver again.
			//ASIODriverInfo.stopped;  // In this sample the processing will just stop
			ret = 1L;
			break;
		case kAsioResyncRequest:
			// This informs the application, that the driver encountered some non fatal data loss.
			// It is used for synchronization purposes of different media.
			// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
			// Windows Multimedia system, which could loose data because the Mutex was hold too long
			// by another thread.
			// However a driver can issue it in other situations, too.
			ret = 1L;
			break;
		case kAsioLatenciesChanged:
			// This will inform the host application that the drivers were latencies changed.
			// Beware, it this does not mean that the buffer sizes have changed!
			// You might need to update internal delay data.
			ret = 1L;
			break;
		case kAsioEngineVersion:
			// return the supported ASIO version of the host application
			// If a host applications does not implement this selector, ASIO 1.0 is assumed
			// by the driver
			ret = 2L;
			break;
		case kAsioSupportsTimeInfo:
			// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
			// is supported.
			// For compatibility with ASIO 1.0 drivers the host application should always support
			// the "old" bufferSwitch method, too.
			ret = 0;
			break;
		case kAsioSupportsTimeCode:
			// informs the driver wether application is interested in time code info.
			// If an application does not need to know about time code, the driver has less work
			// to do.
			ret = 0;
			break;
		}
		return ret;
	}
	static ASIOTime* bufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
	{
		return nullptr;
	}
	//绑定的对象
	static inline ASIOEntity* asioEntity = nullptr;
};

template<unsigned... ids>
array<ASIOEntity, MAX_DRIVER_NUM> registerCallbacks(std::integer_sequence<unsigned, ids...>)
{
	array<ASIOEntity, MAX_DRIVER_NUM> entitys;
	([&object = entitys[ids]]()
		{
			object.callback.bufferSwitch = __ASIOCALLBACK__<ids>::bufferSwitch;
			object.callback.asioMessage = __ASIOCALLBACK__<ids>::asioMessage;
			object.callback.bufferSwitchTimeInfo = __ASIOCALLBACK__<ids>::bufferSwitchTimeInfo;
			object.callback.sampleRateDidChange = __ASIOCALLBACK__<ids>::sampleRateDidChange;
			__ASIOCALLBACK__<ids>::asioEntity = &object;
			std::println("注册:{}", ids);
		}(), ...);

	return entitys;
};

//对象数组，根据支持的板卡数生成数组
array<ASIOEntity, MAX_DRIVER_NUM> ASIOEntitys = registerCallbacks(std::make_integer_sequence<unsigned, MAX_DRIVER_NUM>());
//驱动锁，用于全局获取驱动时进行锁定
static mutex gMTX;


//std::expected<void, std::string> ASIODriver::start()
//{
//
//    std::packaged_task<std::expected<void, std::string>()> task([this] {
//        //return this->device->start();
//		return std::expected<void, std::string>();
//    });
//    auto future = task.get_future();
//
//    this->taskList.push_back(std::move(task));
//
//
//
//
//    return future.get();
//}
//
//std::future<std::expected<void, std::string>> ASIODriver::submitTask(task_type task)
//{
//    auto future = task.get_future();
//
//    {
//        std::lock_guard<std::mutex> lock(this->mtx);
//        //加锁
//        this->taskList.push_back(std::move(task));
//    }
//
//    cv.notify_one();
//
//    return future;
//}


constexpr int maxDelayMills = 50;
constexpr int minDelayMills = 40;









ASIODriver::ASIODriver(ASIOCallbacks* callbacks, CLSID clsid)
	:asioID(clsid)
{
	//int sampleRate = 48000;
	//int sampleSize = maxDelayMills * sampleRate / 1000.0f;  //最大采样点
	//int num = 1;
	//sampleSize >>= 1;
	//while (sampleSize > 0)
	//{
	//	sampleSize >>= 1;
	//	num <<= 1;
	//}
	////num 就是比maxDelay小的2的整次方。
	//
	//float delay = num * 1000.0f / sampleRate;
	//if (delay < minDelayMills)
	//{
	//	num *= 2;
	//}

	this->pAsioDevice = std::make_unique<ASIODevice>(callbacks, clsid);

}

ASIODriver::~ASIODriver()
{
	this->processFlag = false;
	this->cv.notify_one();
	// if (this->fu.valid())
	// {
	// 	this->fu.get();
	// }
	if(this->hThread != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(this->hThread, INFINITE);
		CloseHandle(this->hThread);
		this->hThread = INVALID_HANDLE_VALUE;
	}


	if (this->pAsioDevice != nullptr)
	{
		auto future = this->staWorker.submit(
						[this] {
							println("主动释放资源");
							this->pAsioDevice.reset();
							return STAType();
						});
		auto xxx = future.get();
	}


}

std::expected<void, std::string> ASIODriver::create(CLSID clsid)
{


	static array<ASIOEntity, MAX_DRIVER_NUM> ASIOOBJECTS; // = registerCallbacks(std::make_index_sequence<MAX_DRIVER_NUM>{});


	//创建对象
	this->pAsioDevice = std::make_unique<ASIODevice>(nullptr, clsid);

	auto result = this->pAsioDevice->deviceInit();  //初始化驱动
	if (!result)
	{
		return std::expected<void, std::string>(std::unexpect, "Failed to initialize ASIO device");
	}

	return std::expected<void, std::string>();
}

std::expected<ASIORender*, std::string>ASIODriver::createRender(int channelMask)
{	
	int outputMask = this->pAsioDevice->outputMask;
	if (channelMask > outputMask)
	{
		return std::unexpected("不支持的通道值1");
	}

	if ((channelMask & outputMask) != channelMask)
	{
		return std::unexpected("不支持的通道2");
	}


	ASIORender* pRender = new ASIORender(this, channelMask);

	auto& vec = pRender->_channels;
	for (auto v : vec)
	{
		// for (auto& buf : this->pAsioDevice->outputRing)
		// {
		// 	if (v == buf.channel)
		// 	{
		// 		WaveRingBuffer waveRing(buf.sampleType, this->maxBufferSize);
		// 		pRender->_buffers.push_back(std::move(waveRing));
		// 	}
		// }
	}

	{
		lock_guard<mutex> lg(this->mtx);
		this->renderLsts.push_back(pRender);
	}



	return pRender;
}

std::expected<ASIOCapture*, std::string>ASIODriver::createCapture(int channelMask)
{
	int inputMask = this->pAsioDevice->inputMask;
	if (channelMask > inputMask)
	{
		return std::unexpected("不支持的通道值1");
	}

	if ((channelMask & inputMask) != channelMask)
	{
		return std::unexpected("不支持的通道2");
	}


	ASIOCapture* pCapture = new ASIOCapture(this, channelMask);

	auto& vec = pCapture->_channels;
	for (auto v : vec)
	{
		// for (auto& buf : this->pAsioDevice->outputRing)
		// {
		// 	if (v == buf.channel)
		// 	{
		// 		WaveRingBuffer waveRing(buf.sampleType, this->maxBufferSize);
		// 		pCapture->_buffers.push_back(std::move(waveRing));
		// 	}
		// }
	}

	{
		lock_guard<mutex> lg(this->mtx);
		_Client2  cccc;
		//this->captureLsts.push_back(pCapture);
		std::vector<_Client2*> temp = this->clients;  //先拷贝一份
		temp.push_back(nullptr);

		


		//同步刷新单通道的列表，也用COW模式

		//自旋锁
		this->clients.swap(temp); //交换对象
	
	}


	



	return pCapture;
}

void ASIODriver::removeClient(_Client2 *client)
{
	{
		lock_guard<mutex> lg(this->mtx);
		//this->captureLsts.push_back(pCapture);
		std::vector<_Client2*> temp = this->clients;  //先拷贝一份
		temp.push_back(nullptr);
		//自旋锁
		this->clients.swap(temp); //交换对象
	
	}

	//释放移除的对象。
}

/**
* 1、能够按照缓冲时间来，就无限期等待，等待驱动通知
* 2、按照超长时刻，就按照一半时刻来进行等待，主动超时唤醒。
* 3、每个通知周期有两次唤醒，第一次是超时，第二次是主动唤醒，第二次无限等待。
*/
void ASIODriver::processor()
{

	auto& pDevice = this->pAsioDevice;
	auto deviceByteSize = pDevice->deviceByteSize;
	auto deviceFrameSize = pDevice->bufferSize;

	int notifyByteSize = this->notifyBufferSize * pDevice->bitDepth; //缓冲区字节数

	float* temp = new float[notifyBufferSize];

	int offset =  0;
	int _iActiveNum = this->pAsioDevice->_iActiveNum;
	int _oActiveNum = this->pAsioDevice->_oActiveNum;
	int totalActiveNum = this->pAsioDevice->totalActiveNum;

	auto& buffers = this->pAsioDevice->_cbBuffers;
	
	float* mixBuffer = nullptr;

	int n = 0;
	while (this->processFlag)
	{
		std::unique_lock lk(this->mtx);
		this->cv.wait(lk);//在这里等待回调的通知。

		auto frameSize = this->_validFrameNum.load(memory_order_acquire);

		while (frameSize > deviceFrameSize)
		{

			frameSize = this->_validFrameNum.fetch_sub(deviceFrameSize, memory_order_acq_rel); //减去空间，循环执行。
		
			auto capCounter = pDevice->_captureCounter.load(memory_order_acquire);
			auto wpos1 = capCounter & (this->pAsioDevice->_haBuffersize - 1); //写位置。

			//处理输入通道
			for(int i=0; i<_iActiveNum; i++)
			{
				offset = i;
				auto& chView = this->chViews[offset];
				if(chView.wbs.empty())
				{
					continue;
				}
				
				auto& input = buffers[offset];
				auto src = input._buf + wpos1;  //读取的原始位置
				for(auto& wb : chView.wbs)
				{
					wb->writeBytes(src, deviceByteSize);
				}

			}

			//处理输出通道
			auto wpos2 = pDevice->_renderReadPos.load(memory_order_acquire);
			for(int i=0; i<_oActiveNum; i++)
			{
				offset = i + _iActiveNum;
				auto& chView = this->chViews[offset];
				auto size = chView.wbs.size();
				if(size == 0)
				{
					//对应内存写0
				}else
				{
					//如果类型一致就直接拷贝，不走后面的转换了。不然就走转换

					//--
					auto& wb = chView.wbs[0];
					wb->readFloat(mixBuffer, 1024);  //先把第一个通道的内容读出来。
					//如果数据不够就给mixBuffer补0 
					for(int k = 1; k<size; k++)
					{
						auto& wb2 = chView.wbs[k];
						wb2->readFloat(temp, 1024);
						
						for(int m = 0; m< 1024; m++)
						{
							mixBuffer[m]+=temp[m];
						}
					}
					//转码后写入对应缓冲区
				}
			}
		
		
		
		
		}

		if (this->processFlag == false)
		{
			break;
		}

		//引擎接收到回调的通知后开始处理数据

		//先将环形缓冲区中的数据读走。
		long mills = this->sw.ElapsedMillis();
		this->sw.Reset();
		this->sw.Start();
		println("接收到信号:{}", mills);

		Stopwatch sw1;
		sw1.Start();

		println("拷贝耗时: {}", sw1.ElapsedMillis());
	}

	println("监听线程退出:{}", this->processFlag);




	//在这里做增删改查




}



void ASIODriver::bufferProcess(long doubleBufferIndex, ASIOBool directProcess)
{

	auto& pDevice = this->pAsioDevice;
	auto deviceByteSize = pDevice->deviceByteSize;  //缓冲区字节数
	auto deviceFrameSize = pDevice->bufferSize;

	int offset = 0;
	int haSize = 0;  //缓冲区的通知总大小
	int _iActiveNum = this->pAsioDevice->_iActiveNum; //激活的输入通道数
	int _oActiveNum = this->pAsioDevice->_oActiveNum; //激活的输出通道数
	int _totalNum = this->pAsioDevice->totalActiveNum;  //总激活的通道数

	auto& buffers = this->pAsioDevice->_cbBuffers;  //回调缓冲区

	int captureCounter = this->pAsioDevice->_captureCounter.load(memory_order_acquire);  //输入缓冲区计数器
	int _wpos = captureCounter & (haSize - 1); //写位置。
	for(int i=0; i< _iActiveNum; i++)
	{
		auto& input = buffers[i];

		void* buffer = input.buffers[doubleBufferIndex];  
        char* ptr = static_cast<char*>(buffer);   //硬件的缓冲区
        auto dest = input._buf + _wpos;
		memcpy(dest, ptr, 1024); //直接拷贝对应数量，这里是帧对齐的，不用计算容量了。
	}
	this->pAsioDevice->_captureCounter.fetch_add(1024, memory_order_release);

	auto rpos = this->pAsioDevice->_renderReadPos.load(memory_order_acquire);  //输出读位置
	//for (auto& output : outputs)
	for(int i=0; i< _oActiveNum; i++)
	{
		offset = _iActiveNum + i;
		auto& output = buffers[offset];
		auto* buffer = output.buffers[doubleBufferIndex];
        char* dest = static_cast<char*>(buffer);
        std::fill_n(dest, deviceByteSize, 0); //清空缓冲区
        auto src = output._buf + rpos;
		memcpy(dest, src, 1024);  //读取对应的数据，引擎保证数据是有效的
	}
	this->pAsioDevice->_captureCounter.fetch_add(deviceFrameSize, memory_order_acq_rel);  //增加计数器



	auto bufferCounter = this->_validFrameNum.fetch_add(deviceFrameSize, memory_order_acq_rel);
	
	if (bufferCounter == 0)
	{
		this->sw.Start();
	}

	bufferCounter += deviceFrameSize;

	if (bufferCounter == this->notifyBufferSize )
	{
		//this->bufferCounter = 0;
		this->cv.notify_one();
	}


}

STAType ASIODriver::driverOpen()
{

	auto future = this->staWorker.submit(
					[this] {
					return this->pAsioDevice->deviceInit();
					});

	auto result = future.get();
	if (!result)
	{
		return result;
	}

	auto sampleRate = this->pAsioDevice->sampleRate;


	return {};
}

STAType ASIODriver::createBuffer()
{
	auto future = this->staWorker.submit(
		[this] {
			return this->pAsioDevice->createBuffer();
		});

	return future.get();
}

STAType ASIODriver::start()
{
	auto future = this->staWorker.submit(
		[this] {
			return this->pAsioDevice->start();
		});

	STAType result = future.get();
	if (!result)
	{
		return result;
	}

	// if (this->fu.valid() == false)
	// {
	// 	this->processFlag = true;
	// 	this->fu = std::async(std::launch::async, &ASIODriver::processor, this);
	// }

	return STAType();
}

STAType ASIODriver::stop()
{
	auto future = this->staWorker.submit(
		[this] {
			return this->pAsioDevice->stop();
		});

	return future.get();
}

STAType ASIODriver::setChannelMask(unsigned inputMask, unsigned outputMask)
{
	return this->pAsioDevice->setChannelMask(inputMask, outputMask);
}

TResult<int> ASIODriver::getSampleRate()
{
    auto future = this->staWorker.submit(
		[this] {
			return this->pAsioDevice->getSampleRate();
		});
	auto result = future.get();
	if (!result)
	{
		return std::unexpected(result.error());
	}

	return this->pAsioDevice->sampleRate;
}

TResult<void> ASIODriver::setSampleRate(long sampleRate)
{
    auto future = this->staWorker.submit(
		[this, sampleRate] {
			return this->pAsioDevice->setSampleRate(sampleRate);
		});

	return future.get();
}

int ASIODriver::getCaptureCount()
{
    return this->pAsioDevice->inputChannels.size();
}

int ASIODriver::getRenderCount()
{
    return this->pAsioDevice->outputChannels.size();
}

std::string ASIODriver::getCaptureName(int channel)
{
    if(channel < 0 || channel >= this->pAsioDevice->inputChannels.size())
	{
		return std::string();
	}

	return this->pAsioDevice->inputChannels[channel].name;
}

std::string ASIODriver::getRenderName(int channel)
{
    if(channel < 0 || channel >= this->pAsioDevice->outputChannels.size())
	{
		return std::string();
	}

	return this->pAsioDevice->outputChannels[channel].name;	
}

TResult<void> ASIODriver::Initialize(unsigned inputMask, unsigned outputMask)
{
	auto result = this->pAsioDevice->setChannelMask(inputMask, outputMask);
	if(!result)
	{
		return result;
	}
	result = this->createBuffer();


	// if(this->hThread == INVALID_HANDLE_VALUE)
	// {
	// 	auto threadHandle = _beginthreadex(nullptr, 0, &ASIODriver::threadProc, this, 0, nullptr);
	// 	if(threadHandle == 0)
	// 	{
	// 		return std::unexpected("Failed to create thread");
	// 	}
	// 	this->hThread = reinterpret_cast<HANDLE>(threadHandle);
	// }

    return result;
}

TResult<void> ASIODriver::Release()
{
    return TResult<void>();
}

std::expected<ASIODriver*, std::string> ASIODriver::createDriver(CLSID clsid)
{

	lock_guard<mutex> lg(gMTX);
	
	//第一个空偏移
	int offset = -1;
	for (auto [index, entity] : views::enumerate(ASIOEntitys))
	{
		auto& pASIODriver = entity.pASIODriver;
		if (pASIODriver != nullptr)
		{
			if (IsEqualCLSID(pASIODriver->asioID, clsid) == TRUE)
			{
				//pASIODriver->refCounter++;
				//return pASIODriver.get();
				return std::unexpected("不允许重复创建驱动");
			}
		}
		else
		{
			if (offset == -1)
			{
				offset = index;
			}
		}
	}

	if (offset == -1)
	{
		return unexpected("can not support so much card"); //超过了最大支持的板卡数量)
	}

	auto& entity = ASIOEntitys[offset];

	auto pASIODriver = std::make_unique<ASIODriver>(&entity.callback, clsid);
	
	auto result = pASIODriver->driverOpen();
	if (!result)
	{
		return std::unexpected(result.error());
	}


	entity.pASIODriver = std::move(pASIODriver);
	//entity.pASIODriver->refCounter++;  //新创建了驱动，引用计数+1




	return entity.pASIODriver.get();

}

std::expected<void, std::string> ASIODriver::releaseDriver(ASIODriver* driver)
{
	if (driver == nullptr)
	{
		return unexpected("不能为空");
	}


	lock_guard<mutex> lg(gMTX);
	for (auto& entity : ASIOEntitys)
	{
		auto& pASIODriver = entity.pASIODriver;
		if (pASIODriver != nullptr)
		{
			if (IsEqualCLSID(pASIODriver->asioID, driver->asioID) == TRUE)
			{
				//pASIODriver->refCounter--;
				//if (pASIODriver->refCounter == 0)
				{
					pASIODriver.reset();//释放资源
					return {};
				}
			}
		}
	}

	return unexpected("没有匹配项目");
}
