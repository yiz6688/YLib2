#include"ASIODriver.h"
#include"asiosdk/asio.h"
#include<ranges>
#include<array>
#include<print>

using namespace std;

constexpr unsigned MAX_DRIVER_NUM = 2;  //最大支持的驱动数量

constexpr int BUFFER_NOTIFY_MILLS = 20;   //系统通知的间隔
constexpr int BUFFER_MAX_MILLS = 100;     //最大系统缓冲区



static int nextpow2(int num)
{
	int result = 0x1;
	num -= 1;
	do
	{
		result <<= 1;
	} while (num >>= 1);
	return result;
}


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
	if (this->fu.valid())
	{
		this->fu.get();
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


	std::unique_ptr<ASIORender> pRender = std::make_unique<ASIORender>(this, channelMask);

	auto& vec = pRender->_channels;
	for (auto v : vec)
	{
		for (auto& buf : this->pAsioDevice->outputRing)
		{
			if (v == buf.channel)
			{
				WaveRingBuffer waveRing(buf.sampleType, this->maxBufferSize);
				pRender->_buffers.push_back(std::move(waveRing));
			}
		}
	}

	{
		lock_guard<mutex> lg(this->mtx);
		this->renderLsts.push_back(std::move(pRender));
	}



	return pRender.get();
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


	std::unique_ptr<ASIOCapture> pCapture = std::make_unique<ASIOCapture>(this, channelMask);

	auto& vec = pCapture->_channels;
	for (auto v : vec)
	{
		for (auto& buf : this->pAsioDevice->outputRing)
		{
			if (v == buf.channel)
			{
				WaveRingBuffer waveRing(buf.sampleType, this->maxBufferSize);
				pCapture->_buffers.push_back(std::move(waveRing));
			}
		}
	}

	{
		lock_guard<mutex> lg(this->mtx);
		this->captureLsts.push_back(std::move(pCapture));
	}


	return pCapture.get();
}


/**
* 1、能够按照缓冲时间来，就无限期等待，等待驱动通知
* 2、按照超长时刻，就按照一半时刻来进行等待，主动超时唤醒。
* 3、每个通知周期有两次唤醒，第一次是超时，第二次是主动唤醒，第二次无限等待。
*/
void ASIODriver::processor()
{

	auto& pDevice = this->pAsioDevice;
	auto& inputRing = pDevice->inputRing;
	auto& outputRing = pDevice->outputRing;
	int inputSize = inputRing.size();
	int outputSize = outputRing.size();

	int notifyByteSize = this->notifyBufferSize * pDevice->bitDepth; //缓冲区字节数
	std::vector<std::unique_ptr<char[]>> inputTemp;
	std::vector<std::unique_ptr<char[]>> outputTemp;

	for (int i = 0; i < inputSize; i++)
	{
		inputTemp.emplace_back(new char[notifyByteSize]);

		this->waveInputBuffers.emplace_back(new WaveRingBuffer(pDevice->sampleType, this->maxBufferSize));
		this->waveInputBuffers.emplace_back(new WaveRingBuffer(pDevice->sampleType, this->maxBufferSize));
	}

	for (int i = 0; i < outputSize; i++)
	{
		outputTemp.emplace_back(new char[notifyByteSize]);
		this->waveOutputBuffers.emplace_back(new WaveRingBuffer(pDevice->sampleType, this->maxBufferSize));
		this->waveOutputBuffers.emplace_back(new WaveRingBuffer(pDevice->sampleType, this->maxBufferSize));
	}

	float* fxx = new float[notifyBufferSize];



	println("监听线程启动:{}, input={}, output={}", this->processFlag, inputRing.size(), outputRing.size());
	int n = 0;
	while (this->processFlag)
	{
		std::unique_lock lk(this->mtx);
		this->cv.wait(lk);//在这里等待回调的通知。

		auto bufferSize = this->_bufferCounter.load(memory_order_acquire);

		while (bufferSize > this->deviceBufferSize)
		{

			bufferSize = this->_bufferCounter.fetch_sub(this->deviceBufferSize, memory_order_acq_rel); //减去空间，循环执行。
		
		
		
		
		
		
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
		for (int i=0; i< inputSize; i++)
		{
			auto& ptr1 = inputTemp[i];
			auto& buf = inputRing[i];
			int size = buf.pRingBuffer->read(ptr1.get(), notifyByteSize);
			if (i == 0)
			{
				println("缓冲区余量:{},  读到的:{} ", buf.pRingBuffer->getReadableBytes(), size);
			}
		}

		for (int i = 0; i < outputSize; i++)
		{
			auto& ptr1 = outputTemp[i];
			auto& buf = outputRing[i];
			int size = buf.pRingBuffer->write(ptr1.get(), notifyByteSize);
			if (i == 0)
			{
				println("缓冲区可写:{},  写入的:{} ", buf.pRingBuffer->getWriteableBytes(), size);
			}
		}
		

		
		for (int i = 0; i < inputSize; i++)
		{
			auto& ptr1 = inputTemp[i];
			auto* buf1 = this->waveInputBuffers[i * 2];
			auto* buf2 = this->waveInputBuffers[i * 2 + 1];
			buf1->writeBytes(ptr1.get(), notifyByteSize);
			buf2->writeBytes(ptr1.get(), notifyByteSize);
		}

		for (int i = 0; i < outputSize; i++)
		{
			auto& ptr1 = outputTemp[i];
			auto* buf1 = this->waveOutputBuffers[i * 2];
			auto* buf2 = this->waveOutputBuffers[i * 2 + 1];
			//buf1->readBytes(ptr1.get(), notifyByteSize);
			//buf2->readBytes(ptr1.get(), notifyByteSize);
			buf1->writeFloat(fxx, notifyBufferSize);
			buf2->writeFloat(fxx, notifyBufferSize);
		}

		println("拷贝耗时: {}", sw1.ElapsedMillis());
	}

	println("监听线程退出:{}", this->processFlag);


}



void ASIODriver::bufferProcess(long doubleBufferIndex, ASIOBool directProcess)
{
	//

	this->pAsioDevice->bufferProcess(doubleBufferIndex, directProcess);



	auto bufferCounter = this->_bufferCounter.fetch_add(this->deviceBufferSize, memory_order_acq_rel);
	
	if (bufferCounter == 0)
	{
		this->sw.Start();
	}

	bufferCounter += this->deviceBufferSize;

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
	this->deviceBufferSize = this->pAsioDevice->bufferSize;   //采样点数
	this->deviceByteSize = this->deviceBufferSize * this->pAsioDevice->bitDepth; //采样字节数


	this->notifyBufferSize = BUFFER_NOTIFY_MILLS * sampleRate / 1000; //乘以缓冲区
	this->notifyBufferSize = nextpow2(this->notifyBufferSize);  //最接近的一个2的N次方
	println("通知缓冲区1:{}", this->notifyBufferSize);

	if (this->notifyBufferSize < this->deviceBufferSize)
	{
		this->notifyBufferSize = this->deviceBufferSize;
	}

	long notifyMills = this->notifyBufferSize * 1000 / sampleRate;
	int num = BUFFER_MAX_MILLS / notifyMills;
	this->maxBufferSize = num * this->notifyBufferSize;
	if (num < 2)
	{
		maxBufferSize *= 2;
	}
	println("系统缓冲区:{}, 通知缓冲区:{}, 最大缓冲区:{}", this->deviceBufferSize, this->notifyBufferSize, this->maxBufferSize);

	this->pAsioDevice->ringBufferSize = this->maxBufferSize;

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

	if (this->fu.valid() == false)
	{
		this->processFlag = true;
		this->fu = std::async(std::launch::async, &ASIODriver::processor, this);
	}

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
