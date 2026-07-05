#pragma once
#include<string>
#include<expected>

class SerialPort
{
public:
	SerialPort(const std::string portName);
	~SerialPort();

public:
	std::expected<void, std::string> openPort();

	void closePort();

	std::expected<void, std::string> setBaudRate(int baudRate);

	std::expected<int, std::string> writeBytes(const unsigned char* data, int length, int offset, int writeSize);

	std::expected<int, std::string> readBytes(unsigned char* buffer, int length, int offset, int readSize);

	std::expected<void, std::string> setTimeOut(int readTimeout, int writeTimeout);

	std::expected<void, std::string> setbufferSize(int inSize, int outSize);

	std::expected<void, std::string> setParam(int baudRate, int byteSize, int stopBits, int parity);

	std::expected<void, std::string> setDTR(int value);

	std::expected<void, std::string> setRTS(int value);

	std::expected<int, std::string> readableBytes();

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
