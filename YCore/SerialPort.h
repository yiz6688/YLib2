#pragma once
#include<string>
#include<expected>
#include"TResult.h"
class SerialPort
{
public:
	SerialPort(const std::string portName);
	~SerialPort();

public:
	TResult<void> openPort();

	void closePort();

	TResult<void> setBaudRate(int baudRate);

	TResult<int> writeBytes(const unsigned char* data, int length, int offset, int writeSize);

	TResult<int> readBytes(unsigned char* buffer, int length, int offset, int readSize);

	TResult<void> setTimeOut(int readTimeout, int writeTimeout);

	TResult<void> setbufferSize(int inSize, int outSize);

	TResult<void> setParam(int baudRate, int byteSize, int stopBits, int parity);

	TResult<void> setDTR(int value);

	TResult<void> setRTS(int value);

	TResult<int> readableBytes();

public:
	//static TResult<SerialPort> create(std::string_view portName);

private:
	//HANDLE hCom;
	void* hCom;
	std::string portName;
	int baudRate;
	int parity;
	int byteSize;
	int stopBits;
	//DCB dcbParam;


};
