#pragma once
#include<string>
#include<guiddef.h>
#include<vector>
#include"ASIOClient.h"
#include<expected>

//驱动信息
struct asioInfo
{
	const CLSID clsid;
	const std::string driverName;
};

class ASIOManager
{


public:
	ASIOManager();
	~ASIOManager();

	long getDeviceNum();

	asioInfo* getDeviceInfo(unsigned index);

	asioInfo* operator[](unsigned index);

	std::expected<ASIOClient*, std::string> createClient(unsigned index);

private:
	std::vector<asioInfo> asioInfos;

};