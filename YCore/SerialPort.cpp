#include "SerialPort.h"
#include<Windows.h>

using namespace std;

SerialPort::SerialPort(const std::string portName)
	:hCom(INVALID_HANDLE_VALUE), portName(portName), 
	baudRate(9600), parity(NOPARITY), byteSize(8), stopBits(ONESTOPBIT)
{

}

SerialPort::~SerialPort()
{}

std::expected<void, std::string> SerialPort::openPort()
{
	string name = "\\\\.\\" + string(portName);
	HANDLE handle = CreateFileA(name.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return {};
	}
	this->hCom = handle;
	DCB dcbParam;

	BOOL vv = GetCommState(this->hCom, &dcbParam);
	if (!vv)
	{
		CloseHandle(this->hCom);
		this->hCom = INVALID_HANDLE_VALUE;
		return {};
	}


	auto result = setParam(921600, 8, ONESTOPBIT, NOPARITY);

	result = setTimeOut(1000, 1000);

	result = setbufferSize(4096, 1024);

	return {};
}

void SerialPort::closePort()
{
	if (this->hCom != INVALID_HANDLE_VALUE)
	{
		CloseHandle(this->hCom);
		this->hCom = INVALID_HANDLE_VALUE;
	}
	return ;
}

std::expected<void, std::string> SerialPort::setBaudRate(int baudRate)
{
	return {};
}

std::expected<int, std::string> SerialPort::writeBytes(const unsigned char* data, int length, int offset, int writeSize)
{
	return 0;
}

std::expected<int, std::string> SerialPort::readBytes(unsigned char* buffer, int length, int offset, int readSize)
{
	return 0;
}

std::expected<void, std::string> SerialPort::setTimeOut(int readTimeout, int writeTimeout)
{
	//GetCommTimeouts

	//TotalReadTimeout=ReadTotalTimeoutConstant+(ReadTotalTimeoutMultiplier×请求字节数)
	//一般两个字符之间超时设置为最大，字符超时乘数设置为0 ，然后设置一个总的读取超时常量即可。  总超时设置为0 就立即返回
	COMMTIMEOUTS comTimeOuts = { 0, 0 , 0, 0, 0 };
	comTimeOuts.ReadIntervalTimeout = MAXDWORD;      // 禁用间隔超时   两个字符最大间隔 单位ms
	comTimeOuts.ReadTotalTimeoutMultiplier = 0;		//每个字节的超时乘数
	comTimeOuts.ReadTotalTimeoutConstant = readTimeout;     // 最多等 5 秒   总读取等待时间
	comTimeOuts.WriteTotalTimeoutMultiplier = 0;
	comTimeOuts.WriteTotalTimeoutConstant = writeTimeout;    // 写最多等 2 秒    写超时设置为0就无限等待，待确认

	if (readTimeout == 0)  //不设超时，立即返回
	{
		comTimeOuts.ReadTotalTimeoutConstant = 0;
		comTimeOuts.ReadTotalTimeoutMultiplier = 0;
		comTimeOuts.ReadIntervalTimeout = MAXDWORD;
	}
	else if (readTimeout < 0)  //相当于不限超时，  另一种说法是这3个都设置成0
	{
		comTimeOuts.ReadTotalTimeoutConstant = MAXDWORD - 1;  //这里可能是 -1
		comTimeOuts.ReadTotalTimeoutMultiplier = MAXDWORD;
		comTimeOuts.ReadIntervalTimeout = MAXDWORD;
	}
	else
	{
		comTimeOuts.ReadTotalTimeoutConstant = readTimeout;
		comTimeOuts.ReadTotalTimeoutMultiplier = MAXDWORD;
		comTimeOuts.ReadIntervalTimeout = MAXDWORD;
	}

	comTimeOuts.WriteTotalTimeoutMultiplier = 0;
	if (writeTimeout < 0)
	{
		comTimeOuts.WriteTotalTimeoutConstant = 0;  //不设超时
	}
	else
	{
		comTimeOuts.WriteTotalTimeoutConstant = writeTimeout; //制定写超时
	}


	SetCommTimeouts(this->hCom, &comTimeOuts);
	return {};
}

std::expected<void, std::string> SerialPort::setbufferSize(int inSize, int outSize)
{

	//PurgeComm
	//PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR); //清空收发缓冲区
	SetupComm(this->hCom, inSize, outSize);

	return {};
}

std::expected<void, std::string> SerialPort::setParam(int baudRate, int byteSize, int stopBits, int parity)
{


	DCB dcbParam;
	dcbParam.DCBlength = sizeof(DCB);
	dcbParam.BaudRate = baudRate;
	dcbParam.fBinary = TRUE;  //启用二进制模式，win只支持这个，二进制时，eofchar禁用
	dcbParam.fParity = (parity == NOPARITY ? FALSE : TRUE);  //禁用奇偶校验  配合parity使用，打开模式下，关闭场景下，假如有校验位驱动依然会进行校验，但是不检查错误。
	dcbParam.fOutxCtsFlow = FALSE;   //RTS流控开关  一般只使用RTS/CTS硬件流控中的RTS    发送是否受对方 CTS 信号的约束  CTS低电平暂停发送，高电平发送
	dcbParam.fOutxDsrFlow = FALSE;	//DTR流控开关   DTR/DSR硬件流控中的DTR 状态位，设备发送就绪的状态表示。
	dcbParam.fDtrControl = DTR_CONTROL_DISABLE;  //禁用DTR流控
	dcbParam.fDsrSensitivity = FALSE;  //是否启动DSR灵敏度，启用后，接收时会检测DSR信号，DSR为低电平时，禁止发送数据，不影响接收
	dcbParam.fTXContinueOnXoff = FALSE; //在接收XOFF后继续发送数据，启用后，把发送缓冲区中数据发完，不发送新数据，禁止后，立即停止发送，即便发送缓冲区中还有数据，这个场景下可能会丢失数据。
	dcbParam.fOutX = FALSE;   //接收缓冲区软件流控, 发送XON和XOFF
	dcbParam.fInX = FALSE;	//发送缓冲区软件流控, 响应XON和XOFF
	dcbParam.fErrorChar = FALSE;  //禁用错误替换， 打开后如发生错误，替换为ErrorChar
	dcbParam.fNull = FALSE;    //是否丢弃空白字符即0x00 
	dcbParam.fRtsControl = RTS_CONTROL_DISABLE;  //禁用RTS流控
	dcbParam.fAbortOnError = FALSE;    //发生通信错误（如帧错误、溢出）时，是否自动终止所有未完成的读写操作    联动 ClearCommError
	dcbParam.XonLim = 1024;    //软件允许发送阈值	这个值是读缓冲区中可用字节的下限。当可用空间大于这个值时发送XON
	dcbParam.XoffLim = 512;   //软件禁止发送阈值  这个值是读缓冲区中可用字节数的上限。当可用字节数小于这个值时发送XOFF  XOFF要小于XON
	dcbParam.ByteSize = byteSize;
	dcbParam.Parity = parity;
	dcbParam.StopBits = stopBits;
	dcbParam.XonChar = 0x11;    //软件允许发送字符
	dcbParam.XoffChar = 0x13;   //软件禁止发送字符
	dcbParam.ErrorChar = 0x00;     //替换错误字符
	dcbParam.EofChar = 0x1A;		  //接收结束字符
	dcbParam.EvtChar = 0x00;		  //接收事件字符  非0即触发事件。
	dcbParam.wReserved1 = 0;

	SetCommState(this->hCom, &dcbParam);


	return {};
}

std::expected<void, std::string> SerialPort::setDTR(int value)
{
	EscapeCommFunction(this->hCom, value ? SETDTR : CLRDTR);
	return {};
}

std::expected<void, std::string> SerialPort::setRTS(int value)
{
	EscapeCommFunction(this->hCom, value ? SETRTS : CLRRTS);
	return {};
}

std::expected<int, std::string> SerialPort::readableBytes()
{

	//GetCommModemStatus  读取信号线状态DTR DSR等
	DWORD error;
	COMSTAT comstat;
	ClearCommError(this->hCom, &error, &comstat);

	//SetCommMask  设置感兴趣的事件
	//WaitCommEvent  等待串口事件发生
	/*
	EV_RXCHAR：收到字符
EV_TXEMPTY：发送缓冲区空
EV_CTS：CTS 信号变化
EV_RXFLAG：收到 EvtChar*/


	return comstat.cbInQue;
}
