/*
Asio驱动的具体实现


缓冲区操作:
录音模式:
1、仅支持单通道读，读取raw格式，调用方自行进行转换。
2、共享读，暂未实现。

播放模式:
1、仅支持独占写模式，写入raw格式，调用方转换为对应格式后写入。
2、共享写，暂未实现。


*/
#include<print>
#include"./asiosdk/asiosys.h"
#include"./asiosdk/asio.h"
#include"./asiosdk/iasiodrv.h"

#include "ASIODevice.h"
#include<memory>
#include<array>
#include<format>
#include<bitset>

using namespace std;


static constexpr int notifyMills = 20;   //系统通知的间隔
static constexpr int bufferMills = 100;  //不超过缓冲区


int nextpow2(int num)
{
	int result = 0x1;
	num -= 1;
	do
	{
		result <<= 1;
	} while (num >>= 1);
	return result;
}

static std::vector<int> toVec(unsigned value)
{
	std::vector<int> vec;
	int num = 0;
	while (value > 0)
	{
		num++;
		if ((value & 0x1) == 0x1)
		{
			vec.push_back(num);
		}
		value >>= 1;
	}
	return vec;
}

static SampleType _getSampleType(ASIOSampleType asioType)
{
	if (asioType == ASIOSTInt16LSB)
	{
		return SampleType::INT16;
	}
	else if (asioType == ASIOSTInt24LSB)
	{
		return SampleType::INT24;
	}
	else if (asioType == ASIOSTInt32LSB)
	{
		return SampleType::INT32;
	}
	else if (asioType == ASIOSTFloat32LSB)
	{
		return SampleType::IEEE32;
	}
	else
	{
		return SampleType::UNKNOWN;
	}
}

static ASIOChannel toChannel(ASIOChannelInfo info)
{
	ASIOChannel value;
	value.channel = info.channel;
	value.name = info.name;
	value.sampleType = _getSampleType(info.type);
	value.channelType = info.isInput;

	return value;
}



//针对每一个单独的板卡驱动，该类只会实例化一次
//单例模式
//对象创建时，iasio实例同步创建，整个生命周期中只会失效，不会为null
ASIODevice::ASIODevice(ASIOCallbacks* _callbacks, CLSID clsid)
	: callbacks(_callbacks),driverID(clsid),
	bufferMinSize(0), bufferMaxSize(0), bufferPreferredSize(0),
	bufferSize(512), iasio(nullptr), num_of_capture(0), bufferReady(false)
{
}


//析构函数中进行驱动的释放以及引用的清空
ASIODevice::~ASIODevice()
{
	println("ASIODevice析构");

	this->deviceRelease();

}



/**
 * 安全加载驱动
 * 1、如果驱动不存在，直接进行加载
 * 2、如果驱动已存在，检测是否失效。确认失效后释放旧资源后重新加载。
 */
std::expected<void, std::string> ASIODevice::loadInstance()
{
	std::expected<void, std::string> result;
	if(this->iasio != nullptr)
	{
		result = this->getSampleRate();
		if (!result)
		{
			this->deviceRelease(); //释放驱动
		}		
	}


	this->start_counter = 0;
	this->bufferReady = false;

	HRESULT hResult = CoCreateInstance(this->driverID, 0, CLSCTX_INPROC_SERVER, this->driverID, (LPVOID*)(&this->iasio));
	if (hResult != S_OK)
	{
		return std::unexpected(std::format("CoCreateInstance Fail, result={}", hResult));
	}
	void* handle = nullptr;
	auto initResult = this->iasio->init(handle);
	if (initResult != ASIOTrue)  //驱动初始化
	{
		this->iasio->Release();  //释放驱动
		this->iasio = nullptr;
		return std::unexpected("asio init Fail!");
	}

	return {};
}




/// <summary>
/// 使用48k采样率加载驱动程序，
/// 如果不支持48k采样率，就使用默认采样率加载驱动。
/// 加载完毕后相关参数保存在成员变量中。
/// 热加载功能会判断输入和输出通道是否和初始化构造的一致，不一致则要求重新加载驱动。
/// </summary>
std::expected<void, std::string> ASIODevice::deviceInit()
{

	ASIOError error;
	string errInfo = "";

	auto result = this->loadInstance();
	if (!result)
	{
		return result;
	}

	//获取驱动名称
	this->iasio->getDriverName(this->driverName); 

	if (this->supportSampleRate(48000))  //默认设置为48k
	{
		error = this->iasio->setSampleRate(48000);
		if (error != ASE_OK)
		{
			return std::unexpected(std::format("setSampleRate Fail, code: {}", error));
		}
	}

	result = this->getSampleRate();
	if (!result)
	{
		return result;
	}
	
	//获取到缓冲区后。 后续根据延迟设置缓冲区
	//获取缓冲区, granularity=-1 的话 缓冲区就是2的n次方
	error = this->iasio->getBufferSize(&this->bufferMinSize, &this->bufferMaxSize,
		&this->bufferPreferredSize, &this->bufferGranularity);
	if (error != ASE_OK)
	{
		return std::unexpected(std::format("getBufferSize Fail, code: {}", error));
	}
	this->bufferSize = this->bufferPreferredSize;
	//cout << "默认缓冲区: " << this->bufferSize << endl;

	if (this->bufferSize < 512)
	{
		this->bufferSize = 512;
	}


	long notifySize = notifyMills * this->sampleRate / 1000; //乘以缓冲区
	notifySize = nextpow2(notifySize);  //最接近的一个2的N次方
	println("缓冲区1:{}", notifySize);
	float xxx = notifySize * 1000 / this->sampleRate;
	int k = bufferMills / xxx;
	println("k数目:{}", k);
	if (k < 2)
	{
		k = 2;
	}

	if (notifySize < this->bufferSize)
	{
		notifySize = this->bufferSize;
		k = 2;
	}
	



	//获取输入输出数量
	error = this->iasio->getChannels(&this->num_of_capture, &this->num_of_render);
	if (error != ASE_OK)
	{
		return std::unexpected(std::format("getChannels Fail, code: {}", error));
	}

	this->inputChannels.clear();
	this->inputChannels.reserve(this->num_of_capture);
	this->outputChannels.clear();
	this->outputChannels.reserve(this->num_of_render);

	ASIOChannelInfo value;
	ASIOSampleType type = -1;

	for (int i = 0; i < this->num_of_capture; i++)
	{

		value.channel = i;
		value.isInput = ASIOTrue;
		error = this->iasio->getChannelInfo(&value);
		if (error != ASE_OK)
		{
			return std::unexpected(std::format("getChannelInfo Fail, inputchannel={} code: {}",i,  error));
		}
		if (type == -1)
		{
			type = value.type;
			if (type == ASIOSTInt16LSB)
			{
				this->sampleType = SampleType::INT16;
				this->bitDepth = 2;
			}
			else if (type == ASIOSTInt24LSB)
			{
				this->sampleType = SampleType::INT24;
				this->bitDepth = 3;
			}
			else if (type == ASIOSTInt32LSB)
			{
				this->sampleType = SampleType::INT32;
				this->bitDepth = 4;
			}
			else if (type == ASIOSTFloat32LSB)
			{
				this->sampleType = SampleType::IEEE32;
				this->bitDepth = 4;
			}
			else
			{
				return std::unexpected(std::format("unsupport SampleType"));
			}
		}

		if (value.type != type)
		{
			return std::unexpected(std::format("inputChannel:{}, sampleType error", i, type));
		}

		this->inputChannels.push_back(value);
	}
	for (int i = 0; i < this->num_of_render; i++)
	{
		value.channel = i;
		value.isInput = ASIOFalse;
		error = this->iasio->getChannelInfo(&value);
		if (error != ASE_OK)
		{
			return std::unexpected(std::format("getChannelInfo Fail, outputchannel={} code: {}", i, error));
		}
		if (value.type != type)
		{
			return std::unexpected(std::format("outputChannel:{}, sampleType error", i, type));
		}
		this->outputChannels.push_back(value);
	}

	return {};

}

std::expected<void, std::string> ASIODevice::deviceRelease()
{
	if (this->iasio != nullptr)
	{
		this->stop(); // 调用一次停止

		if (this->bufferReady)
		{
			this->iasio->disposeBuffers();  //释放缓冲区
			this->bufferReady = false;
		}
		this->iasio->Release();
	}
	return {};
}



/**
*驱动创建缓冲区
*/
std::expected<void, std::string> ASIODevice::createBuffer()
{
	string errInfo = "";
	bool bSuccess = false;
	ASIOError error;
	

	//创建缓冲区数组,由createbuffer分配对应缓冲区的内存
	//unique_ptr<ASIOBufferInfo[]> bufferInfos(new ASIOBufferInfo[this->total_channel]);

	if (this->inputMask == 0 && this->outputMask == 0)
	{
		this->inputMask = 1 << this->num_of_capture;
		this->inputMask -= 1;
		this->outputMask = 1 << this->num_of_render;
		this->outputMask -= 1;
	}

	auto inputs = toVec(this->inputMask);
	auto outputs = toVec(this->outputMask);
	int inputSize = inputs.size();
	int outputSize = outputs.size();

	if (this->allocFlag == 0)
	{
		this->inputRing.reserve(inputSize);
		this->outputRing.reserve(outputSize);
		int byteSize = this->ringBufferSize * this->bitDepth;
		for (int i = 0; i < inputSize; i++)
		{
			ASIOBuffer buf(inputs[i], this->sampleType, this->bufferSize, this->ringBufferSize);
			this->inputRing.push_back(std::move(buf));
		}

		for (int i = 0; i < outputSize; i++)
		{
			ASIOBuffer buf(outputs[i], this->sampleType, this->bufferSize, this->ringBufferSize);
			this->outputRing.push_back(std::move(buf));
		}
		this->allocFlag = 1;
	}


	auto totalChannel = inputSize + outputSize;

	vector<ASIOBufferInfo> bufferInfos;
	bufferInfos.resize(totalChannel);


	int offset = 0;
	for (int i = 0; i < inputSize; i++)
	{
		offset = i;
		auto input = inputs[i];
		//this->bufferInfos.push_back(ASIOBufferInfo(i, ASIOTrue, {nullptr, nullptr}));
		bufferInfos[offset] = { ASIOTrue, input - 1, {nullptr, nullptr} };
	}

	for(int i=0; i< outputSize; i++)
	{
		offset = inputSize + i;
		//this->bufferInfos.push_back(ASIOBufferInfo(i, ASIOFalse, {nullptr, nullptr}));
		auto output = outputs[i];
		bufferInfos[offset] = { ASIOFalse, output - 1, {nullptr, nullptr} };
	}

	println("开始createbuffer");
	error = this->iasio->createBuffers(bufferInfos.data(), totalChannel,
		this->bufferSize, this->callbacks);

	if (error != ASE_OK)
	{
		return std::unexpected(std::format("createBuffers Fail, code: {}", error));
	}


	println("createbuffer 成功");
	//this->inputBuffers.reserve(inputSize);
	//this->outputBuffers.reserve(outputSize);

	for (int i = 0; i < inputSize; i++)
	{
		offset = i;
		ASIOBufferInfo& bufferInfo = bufferInfos[offset];
		//this->inputBuffers.push_back(bufferInfo);
		this->inputRing[i].buffers[0] = bufferInfo.buffers[0];
		this->inputRing[i].buffers[1] = bufferInfo.buffers[1];
	}
	

	for (int i = 0; i < outputSize; i++)
	{
		offset = i + inputSize;
		ASIOBufferInfo& bufferInfo = bufferInfos[offset];
		//this->outputBuffers.push_back(bufferInfo);
		this->outputRing[i].buffers[0] = bufferInfo.buffers[0];
		this->outputRing[i].buffers[1] = bufferInfo.buffers[1];
	}


	this->bufferReady = true;

	return {};
}



/// <summary>
/// 打开驱动，如驱动已打开且资源已创建就不重复执行，否则重新构建资源。
/// </summary>
/// <returns></returns>
std::expected<void, std::string> ASIODevice::driverOpen(int _sampleRate)
{
	bool flag = false; //是否需要加载驱动的标志位
	auto result = this->getSampleRate(); //检查驱动存活性
	if (result)
	{
		if (this->bufferReady)
		{
			if (this->sampleRate == _sampleRate) //采样率相同且驱动已创建，不需要额外处理直接返回即可。
			{
				return {};
			}
			else
			{
				if (this->runningCounter != 0)  //运行中不能处理
				{
					return std::unexpected("驱动运行中,不允许以不同的采样率打开");
				}
				else
				{
					flag = true;
				}
			}
		}
		else
		{
			flag = true;
		}
	}

	//重新加载驱动,然后创建缓冲区
	this->loadInstance();
	this->createBuffer();
	

}

std::expected<void, std::string> ASIODevice::setChannelMask(unsigned inputMask, unsigned outputMask)
{
	if (this->inputMask == 0 && this->outputMask == 0)
	{
		//处理输入通道
		if (this->num_of_capture == 0 && inputMask != 0)
		{
			return std::unexpected("inputChannel=0");
		}

		unsigned value = (1 << this->num_of_capture) - 1 ;
		if (value < inputMask)
		{
			return std::unexpected("inputMask>inputChannel");
		}

		value &= inputMask;
		if (value != inputMask)
		{
			return std::unexpected("inputMask invalid");
		}
		this->inputMask = inputMask;

		//处理输出通道
		if (this->num_of_render == 0 && outputMask != 0)
		{
			return std::unexpected("outputChannel=0");
		}

		value = (1 << this->num_of_render) - 1;
		if (value < outputMask)
		{
			return std::unexpected("outputMask>outputChannel");
		}

		value &= outputMask;
		if (value != outputMask)
		{
			return std::unexpected("outputMask invalid");
		}
		this->outputMask = outputMask;
		return {};
	}
	else
	{
		return std::unexpected("channelMask has seted");
	}
}

std::expected<void, std::string> ASIODevice::getSampleRate()
{
	ASIOSampleRate _sampleRate;
	auto error = iasio->getSampleRate(&_sampleRate);
	if (error != ASE_OK)
	{
		return std::unexpected(std::format("supportSampleRate Fail, code: {}", error));
	}
	this->sampleRate = _sampleRate;

	return {};
}

std::expected<void, std::string> ASIODevice::supportSampleRate(long value)
{
	auto error = this->iasio->canSampleRate(sampleRate);
	if (error != ASE_OK)
	{
		return std::unexpected(std::format("supportSampleRate Fail, code: {}", error));
	}
	return {};
}


/**
*设置采样率，运行中不允许设置采样率
*/
std::expected<void, std::string> ASIODevice::setSampleRate(long value)
{
	
	if (this->runningCounter > 0)
	{
		if (value == this->sampleRate)
		{
			return {};
		}
		else
		{
			return std::unexpected("驱动运行中，不允许设置采样率");
		}
	}
	else
	{
		auto error = this->iasio->setSampleRate(value);
		if (error != ASE_OK)
		{
			return std::unexpected(std::format("setSampleRate Fail, code: {}", error));
		}

		this->sampleRate = value;
	}
	
	return {};
}



int ASIODevice::get_running_device()
{

	int counter = 0;

	for (int i = 0; i < this->num_of_capture; i++)
	{
		//if (this->captures[i].start_flag == true)
		{
			counter++;
		}
	}

	for (int i = 0; i < this->num_of_render; i++)
	{
		//if (this->renders[i].start_flag == true)
		{
			counter++;
		}
	}

	return counter;
}


/**
*调用驱动启动录音，重复调用增加引用计数
*/
std::expected<void, std::string> ASIODevice::start()
{
    if(this->runningCounter == 0)
	{
		auto error = this->iasio->start();
		if (error == ASE_OK)
		{
			this->runningCounter++;
			return {};
		}
		else
		{
			this->runningCounter = 0;
			return std::unexpected(std::format("start Fail, code: {}", error));
		}
	}else
	{
		this->runningCounter++;
		return {};
	}
}

/**
*调用驱动停止录音，重复调用减少引用计数，归零后调用停止
*/
std::expected<void, std::string> ASIODevice::stop()
{
	if (this->runningCounter == 0)
	{
		return {};
	}

	this->runningCounter--;
	if (this->runningCounter == 0)
	{
		auto error = this->iasio->stop();
		if (error != ASE_OK)
		{
			return std::unexpected(std::format("stop Fail, code: {}", error));
		}
	}

	return {};

}


void ASIODevice::bufferProcess(int bufferIndex, ASIOBool directProcess)
{
	for (auto& buffer : this->inputRing)
	{
		buffer.inputProcess(bufferIndex);
	}

	for (auto& buffer : this->outputRing)
	{
		buffer.outputProcess(bufferIndex);
	}
}

