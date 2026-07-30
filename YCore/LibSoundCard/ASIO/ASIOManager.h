#pragma once
#include<string>
#include<guiddef.h>
#include<vector>
#include"ASIOClient.h"
#include<expected>

//驱动信息


class ASIOManager
{


public:
	ASIOManager();
	~ASIOManager();

	long getDeviceNum();

	ASIOInfo* getDeviceInfo(unsigned index);

	ASIOInfo* operator[](unsigned index);

	std::expected<ASIOClient*, std::string> createClient(unsigned index);

private:
	std::vector<ASIOInfo> asioInfos;

};