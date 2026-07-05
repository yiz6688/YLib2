#pragma once
#include<string>
#include<vector>



class LogBuffer
{

public:


	void writeLog(const LogBuffer& lb);
	void writeINFO(const std::string& log);
	void writeERROR(const std::string& log);
	void writeDEBUG(const std::string& log);

	void append(const LogBuffer& lb);
	void prepend(const LogBuffer& lb);


private:
	void append(int level, const std::string& log);

public:
	std::vector<std::string> logLst;
};