#pragma once
#include<string>
#include"../../TResult.h"
#include<memory>
#include"../ICapture.h"
#include"../IRender.h"

struct ASIOInfo
{
	const CLSID clsid;
	const std::string driverName;
};


class ASIODriver;
//ASIO客户端, 提供给外部调用的接口, 负责与ASIODriver进行交互
class ASIOClient
{
public:
	ASIOClient(ASIODriver* pDriver)
		:pDriver{pDriver}
	{};



private:
	TResult<void> init(CLSID clsid);


public:

	//获取录音器数量
	int getCaptureCount();
	//获取播放器数量
	int getRenderCount();

	std::string getCaptureName(int channel);

	std::string getRenderName(int channel);

			//获取采样率
	TResult<int> getSampleRate();
	//设置采样率
	TResult<void> setSampleRate(long sampleRate);

public:

	TResult<ICapture*> getCapture(std::initializer_list<int> lst);

	TResult<IRender*> getRender(std::initializer_list<int> lst);

	TResult<void> Initialize(unsigned inputMask, unsigned outputMask);

	TResult<void> Release();


public:

	ASIODriver* pDriver;  //驱动的指针。
};
