#pragma once
#include<string_view>
#include"../WaveWriter.h"
#include<expected>
#include"../WaveFormat.h"

//录音的状态
enum class  CaptureState
{
	Starting,  //正在启动

	Stopping,  //正在停止
	//已停止
	Stopped,
	//捕获中
	Capturing,
};

//对外暴露的录音接口类
class ICapture
{

public:
	virtual ~ICapture() = default;
	//启动录制,带回调
	//virtual bool start_capture(std::function<void(ByteArg&)> callback) = 0;

	virtual std::expected<void, std::string> captureAsync(WaveWriter* writer, int maxRecordMills) = 0;

	virtual std::expected<void, std::string> waitCaptureDone() = 0;

	virtual std::expected<void, std::string> stopCapture() = 0;

	virtual std::expected<void, std::string> capture(WaveWriter* writer, int maxRecordMills) = 0;

	virtual CaptureState getCaptureState() = 0;

};