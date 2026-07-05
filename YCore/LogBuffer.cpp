#include "LogBuffer.h"
#include<format>
#include<chrono>

void LogBuffer::writeLog(const LogBuffer& lb)
{
	this->append(lb);
}

void LogBuffer::writeINFO(const std::string& log)
{
	this->append(0, log);
}

void LogBuffer::writeERROR(const std::string& log)
{
	this->append(1, log);
}

void LogBuffer::writeDEBUG(const std::string& log)
{
	this->append(2, log);
}

void LogBuffer::append(const LogBuffer& lb)
{
	if(this == &lb)
	{
		return;
	}
	this->logLst.insert(this->logLst.end(), lb.logLst.begin(), lb.logLst.end());
}

void LogBuffer::prepend(const LogBuffer & lb)
{
	if (this == &lb)
	{
		return;
	}
	this->logLst.insert(this->logLst.begin(), lb.logLst.begin(), lb.logLst.end());
}





void LogBuffer::append(int level, const std::string& log)
{
	std::string header = "[INFO]";
	if (level == 1)
	{
		header = "[ERROR]";
	}
	else if (level == 2)
	{
		header = "[DEBUG]";
	}

	auto now = std::chrono::system_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	auto data = std::format("{:%Y-%m-%d %H:%M:%S}.{:03} {:<8}{}", std::chrono::floor<std::chrono::seconds>(now), ms.count(), header, log);

	this->logLst.push_back(data);
}



