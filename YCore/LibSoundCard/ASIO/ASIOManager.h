#pragma once
#include"base_config.hpp"
#include<string>
#include<guiddef.h>
#include<vector>
#include"ASIOClient.h"
#include"../../TResult.h"

//驱动信息


class ASIOManager
{


public:
	ASIOManager();
	~ASIOManager();

	long getDeviceNum();

	ASIOInfo* getDeviceInfo(unsigned index);

	ASIOInfo* operator[](unsigned index);

	//创建客户端; notifyMills: 通知时长(ms), 默认10ms, 有效范围10-50ms
	//maxDelayMills: 最大延迟(ms), 0=自动(2*通知时长, 上限100ms, 硬件缓冲更大则以实际为准)
	//exclusiveMode: 独占模式(每通道只允许一个客户端, 输出免混频直拷), 初始化指定不可更改
	TPResult<ASIOClient> createClient(unsigned index, int notifyMills = 10, int maxDelayMills = 0, bool exclusiveMode = false);

private:
	std::vector<ASIOInfo> asioInfos;

};