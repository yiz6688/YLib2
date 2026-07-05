#include<print>
#include"StreamTest.h"
#include<vector>
#include<array>
#include"../YCore/WaveReader.h"
#include"../YCore/WaveWriter.h"
#include"../YCore/Utils.h"
#include<numbers>
using namespace std;



int waveReadWriteTest()
{

	//"44.1k@16bit_mono.wav"

	std::string path = R"(D:\wave\)";
	std::string fileName = "48k@i32_stream.wav";

	std::string out_path = R"(D:\wave\out\)";
	std::string out_fileName = "test.wav";


	fileName = "Spk.wav";

	std::string fullPath = path + fileName;
	std::string out_fullPath = out_path + out_fileName;
	WaveReader reader(fullPath);
	auto& fmt = reader.getWaveFormat();
	println("{}", fmt.toString());
	println("wav长度: {}, 帧数: {}", reader.getLength(), reader.getFrameCount());


	WaveWriter writer(fmt, out_fullPath);


	char buffer[4096];

	while (true)
	{
		auto nread = reader.read(buffer, 4096);
		int num = nread.value();
		if (num <= 0)
		{
			break;
		}
		auto result = writer.write(buffer, num);
		if (!result)
		{
			println("写入失败: {}", result.error());
			break;
		}
	}


	return 0;
}

void createSineTest()
{
	int mills = 1000;
	WaveFormat fmt(48000, 32, 1);
	fmt = WaveFormat::createFloatWaveFormat(48000, 1);
	float freq = 1000.0f; // A4音符的频率
	float amplitude = 0.5f;
	int nSamples = fmt.getSampleRate() * mills / 1000;

	std::vector<float> samples(nSamples);
	WaveWriter writer1(fmt, R"(D:\wave\out\sine1.wav)");
	for(int i=0; i<nSamples; i++)
	{
		double t = static_cast<double>(i) / fmt.getSampleRate(); //当前的时间点
		double sampleValue = amplitude * sin(2.0 * numbers::pi * freq * t);
		samples[i] = static_cast<float>(sampleValue);
		auto result = writer1.writeSample(samples[i]);
	}

	WaveWriter writer2(fmt, R"(D:\wave\out\sine2.wav)");
	auto result = writer2.writeSamples(samples.data(), nSamples);
	if (!result)
	{
		println("写入失败: {}", result.error());
	}
}



#include<mutex>
#include<condition_variable>
#include<thread>
#include<vector>
#include<future>
#include<functional>




using namespace std;



bool flag = true;
mutex mtx;
condition_variable cv;


struct  rst
{

	int test1(int c)
	{
		return c;
	}

	double test2()
	{
		return 1.4;
	}
};


vector<move_only_function<void(rst* r)>> vec;


void threadFunc()
{

	rst rr;

	println("线程启动");
	while (flag)
	{

		unique_lock<mutex> lk(mtx);
		cv.wait(lk);
		println("信号触发");
		if (flag == false)
		{
			break;
		}
		for (auto& func : vec)
		{
			func(&rr);
		}

		//vec.clear();
	}

	println("线程退出");

}

std::expected<int, std::string> func1(int x)
{
	println("input:{}", x);
	if (x == 1)
	{
		return 123;
	}
	else
	{
		return std::unexpected("错误");
	}
}

struct  mmp
{
private:
	mmp()
	{
		vaue = 0;
		err = "fail4556746787686786788768768";
		println("构造 p_value={:#016x}, p_err={:#016x}", reinterpret_cast<uintptr_t>(&vaue), reinterpret_cast<uintptr_t>(err.data()));
	}

public:
	mmp(int k)
	{
		vaue = k;
		err = "fail86868678678686868678686787686";
		println("构造 p_value={:#016x}, p_err={:#016x}", reinterpret_cast<uintptr_t>(&vaue), reinterpret_cast<uintptr_t>(err.data()));
	}

public:
	mmp(const mmp& value)
	{
		vaue = value.vaue;
		err = value.err;
		println("拷贝构造函数被调用");
	}

	mmp(mmp&& value)
	{
		vaue = value.vaue;
		err = std::move(value.err);
		println("移动构造函数被调用 p_err={:#016x}", reinterpret_cast<uintptr_t>(err.data()));
	}

	mmp& operator=(const mmp& value)
	{
		vaue = value.vaue;
		err = value.err;
		println("拷贝赋值运算符被调用");
		return *this;
	}

	mmp& operator=(mmp&& value)
	{
		vaue = value.vaue;
		err = std::move(value.err);
		println("移动赋值运算符被调用 p_err={:#016x}", reinterpret_cast<uintptr_t>(err.data()));
		return *this;
	}


	void show()
	{
		println("p_value={:#016x}, p_err={:#016x}", reinterpret_cast<uintptr_t>(&vaue), reinterpret_cast<uintptr_t>(err.data()));
	}

	int vaue;
	std::string err;
};


#include"../YCore/RingBuffer.h"




#include"../YCore/LibSoundCard/ASIO/Asiodevice.h"

#include"../YCore/LibSoundCard/ASIO/ASIOManager.h"
#include"../YCore/LibSoundCard/ASIO/AsioDriver.h"

#include"../YCore/WaveRingBuffer.h"



int WaveRingTest()
{
	int sampleRate = 48000;
	int mills = 1000;
	WaveFormat fmt(sampleRate, 32, 1);
	fmt = WaveFormat::createFloatWaveFormat(sampleRate, 1);
	float freq = 1000.0f; // A4音符的频率
	float amplitude = 0.5f;
	int nSamples = fmt.getSampleRate() * mills / 1000;

	std::vector<float> samples(nSamples);
	
	for (int i = 0; i < nSamples; i++)
	{
		double t = static_cast<double>(i) / fmt.getSampleRate(); //当前的时间点
		double sampleValue = amplitude * sin(2.0 * numbers::pi * freq * t);
		samples[i] = static_cast<float>(sampleValue);
	}


	string names[] = { "int16", "int24", "int32", "ieee32" };
	SampleType types[] = { SampleType::INT16, SampleType::INT24, SampleType::INT32, SampleType::IEEE32 };

	for (int i = 0; i < 4; i++)
	{
		SampleType type = types[i];
		string name = names[i];

		WaveRingBuffer wb(type, sampleRate);
		int writeSample = wb.writeFloat(samples.data(), nSamples);
		println("写入点数:{}, 成功点数:{}", nSamples, writeSample);

		
		//按照int16读取
		string path = std::format(R"(D:\wave\out\sine_{}_i16.wav)", name);
		std::vector<short> i16Data(sampleRate);
		auto readSample = wb.readInt16(i16Data.data(), i16Data.size());
		println("{} 读取的数量:{}",path,  readSample);


		WaveFormat i16Fmt(sampleRate, 16, 1);
		WaveWriter i16Writer(i16Fmt, path);
		auto result = i16Writer.write(i16Data.data(), i16Data.size());
		if (result)
		{
			println("{} 写入成功:{}", path, result.value());
		}
		else
		{
			println("{} 写入失败:{}", path, result.error());
			break;
		}

		//按照int24读取
		writeSample = wb.writeFloat(samples.data(), nSamples);
		println("写入点数:{}, 成功点数:{}", nSamples, writeSample);
		path = std::format(R"(D:\wave\out\sine_{}_i24.wav)", name);
		std::vector<char> i24Data(sampleRate * 3);
		readSample = wb.readInt24Bytes(i24Data.data(), i24Data.size() / 3);
		println("{} 读取的数量:{}", path, readSample);


		WaveFormat i24Fmt(sampleRate, 24, 1);
		WaveWriter i24Writer(i24Fmt, path);
		result = i24Writer.write(i24Data.data(), i24Data.size());
		if (result)
		{
			println("{} 写入成功:{}", path, result.value());
		}
		else
		{
			println("{} 写入失败:{}", path, result.error());
			break;
		}


		//按照int32读取
		writeSample = wb.writeFloat(samples.data(), nSamples);
		println("写入点数:{}, 成功点数:{}", nSamples, writeSample);
		path = std::format(R"(D:\wave\out\sine_{}_i32.wav)", name);
		std::vector<int> i32Data(sampleRate);
		readSample = wb.readInt32(i32Data.data(), i32Data.size());
		println("{} 读取的数量:{}", path, readSample);


		WaveFormat i32Fmt(sampleRate, 32, 1);
		WaveWriter i32Writer(i32Fmt, path);
		result = i32Writer.write(i32Data.data(), i32Data.size());
		if (result)
		{
			println("{} 写入成功:{}", path, result.value());
		}
		else
		{
			println("{} 写入失败:{}", path, result.error());
			break;
		}

		//按照float读取
		writeSample = wb.writeFloat(samples.data(), nSamples);
		println("写入点数:{}, 成功点数:{}", nSamples, writeSample);
		path = std::format(R"(D:\wave\out\sine_{}_float.wav)", name);
		std::vector<float> f32Data(sampleRate);
		readSample = wb.readFloat(f32Data.data(), f32Data.size());
		println("{} 读取的数量:{}", path, readSample);

		auto floatFmt = WaveFormat::createFloatWaveFormat(sampleRate, 1);
		WaveWriter floatWriter(floatFmt, path);
		result = floatWriter.write(f32Data.data(), f32Data.size());
		if (result)
		{
			println("{} 写入成功:{}", path, result.value());
		}
		else
		{
			println("{} 写入失败:{}", path, result.error());
			break;
		}



	}
	

	return 0;

}

#include"../YCore/StopWatch.h"
#include"../YCore/Utils.h"
#include"../YCore/LibSoundCard/WASAPI/WASAPIManager.h"
int main()
{

	auto lst = WASAPIManager::getEndPoints(EDataFlow::eRender, DEVICE_STATE_ACTIVE);

	println("设备数量:{}", lst.size());
	for (auto& ll : lst)
	{
		println("index:{} id:{} frindlyName:{}", ll.index, ll.id, ll.frindlyName);
	}




	return 0;
	auto vvv = Utils::getBitPos(0x34FF);
	for (auto v : vvv)
	{
		println("{}", v);
	}
	
	auto k = Utils::parseBitPos(vvv);

	println("*{:X}*", k);


	return 0;
	
	//WaveRingTest();

	//return 0;

	ASIOManager manager;
	long num = manager.getDeviceNum();
	if (num == 0)
	{
		println("无设备！！！");
		return 0;
	}
	for (int i = 0; i < num; i++)
	{
		auto  vv = manager.getDeviceInfo(0);

		println("{} {}", i, vv->driverName);
	}


	auto clsid = manager.getDeviceInfo(0)->clsid;


	auto driverResult = ASIODriver::createDriver(clsid);
	if (!driverResult)
	{
		println("{}", driverResult.error());
		return 0;
	}
	else
	{

		//auto& device = driverResult.value()->pAsioDevice;
		//println("构建成功");

		//println("名称:{} 输入通道:{}, 输出通道:{}, 采样率:{}， 缓冲区:{}", string(device->driverName),
		//	device->num_of_capture, device->num_of_render, device->sampleRate, device->bufferSize);

		//for (int i = 0; i < device->num_of_capture; i++)
		//{
		//	println("输入 通道:{} 名称:{} 采样类型:{}", device->inputChannels[i].channel, 
		//		device->inputChannels[i].name, (int)device->inputChannels[i].sampleType);
		//}
		//println("*******");
		//for (int i = 0; i < device->num_of_render; i++)
		//{
		//	println("输入 通道:{} 名称:{} 采样类型:{}", device->outputChannels[i].channel, 
		//		device->outputChannels[i].name, (int)device->outputChannels[i].sampleType);
		//}
	}

	auto kk = (*driverResult)->setChannelMask(0xFF, 0xF);
	if (!kk)
	{
		println("{}", kk.error());
		return 0;
	}



	
	//return 0;
	auto createResult = (*driverResult)->createBuffer();
	if (!createResult)
	{
		println("{}", createResult.error());
		return 0;
	}


	auto startResult = driverResult.value()->start();
	if (!startResult)
	{
		println("{}", startResult.error());
		return 0;
	}


	Sleep(5000);

	auto stopResult = driverResult.value()->stop();
	if (!stopResult)
	{
		println("{}", stopResult.error());
		return 0;
	}

	Sleep(1000);

	return 0;
	char buffer2[17] = { 0 };

	RingBuffer rrbb(buffer2, 16);
	char buffer[16];
	memset(buffer, 0, sizeof(buffer));

	for (int i = 0; i < 16; i++)
	{
		int size = rrbb.write("123456", 6);
		//println("write size = {}", size);
		int rsize = rrbb.read(buffer, 5);
		println("{}  read size = {} value = {}", i, rsize, string_view(buffer, rsize));
	}


	return 0;
	std::vector<std::packaged_task<mmp()>> vecc;

	std::packaged_task<mmp()> task([]() -> mmp
	{
		mmp mp(20);
		return mp;
		});

	vecc.push_back(std::move(task));

	auto fu2 = vecc[0].get_future();

	vecc[0]();
	
	auto v2x = fu2.get();
	v2x.show();

	println();

	return 0;

	auto rrr = func1(1).
		and_then([](int v) 
			{
				println("and_then1 v: {}", v);
				return func1(1); 
			}).
		or_else([](const std::string& err) 
			{
				println("error: {}", err); 
				return std::expected<int, std::string>(3);
			}).
		and_then([](int k)
			{
				println("and_then2:{}", k);
				return func1(23);
			});


	


	return 0;

	auto fu = std::async(std::launch::async, threadFunc);
	
	Sleep(1000);

	promise<int> p1;
	auto fux = p1.get_future();

	std::move_only_function<void(rst*)> func1 = [p = std::move(p1)](rst* r) mutable
	{
			Sleep(3000);
			auto vv = r->test1(123);
			p.set_value(vv);
	};


	println("开始调用任务");
	vec.push_back(std::move(func1));

	cv.notify_one();

	auto aa = fux.get();

	println("{}", aa);

	Sleep(1000);

	flag = false;
	cv.notify_one();

	Sleep(3000);

	//createSineTest();

	return 0;
}