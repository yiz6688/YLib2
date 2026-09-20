#pragma once
#include<Windows.h>
#include<vector>
#include<atomic>
#include"../../TResult.h"
#include"../../WaveFormat.h"
//注意: asiosys.h 必须先于 asio.h, 否则 ASIOSampleRate 会退化为 struct 而非 double
#include"./asiosdk/asiosys.h"
#include"./asiosdk/asio.h"

struct IASIO;
class ASIOObject;

//单个通道的软件缓冲描述
// 输入通道: 覆盖式环形区(_captureCounter 写, 引擎读), 允许消费不及时时一直覆盖采集
// 输出通道: 精确门限环形区(引擎写, 回调读), 不可覆盖, 填满即止
struct ASIOBuffer2
{
    ASIOBuffer2(int _channel, char* buf, int _type)
        : channel{_channel}, type{_type}, _buf{buf}
    {}

    int channel;            //物理通道号
    int type;               //ASIOTrue=输入, ASIOFalse=输出
    char* _buf;             //该通道环形缓冲区的起始地址
    void* buffers[2] { nullptr, nullptr };  //ASIO硬件双缓冲指针(createBuffers 后有效)
};

class ASIODevice
{

public:

    //notifyMills   : 通知时长(ms), 默认10ms, 有效范围10-50ms, 超出抛 std::invalid_argument
    //maxDelayMills : 最大延迟(ms), 默认0=自动(2*通知时长, 上限100ms, 实际硬件缓冲更大则以实际为准);
    //                非0则必须 >= notifyMills 且 <= 100ms, 否则抛 std::invalid_argument
    ASIODevice(ASIOCallbacks* _callbacks, CLSID clsid, int notifyMills = 10, int maxDelayMills = 0);

    ~ASIODevice();

public:

    //加载驱动, 不创建缓冲区
    TResult<void> loadInstance();

    //驱动初始化(查询采样率/通道信息)
    TResult<void> deviceInit();

    //释放驱动, 析构时调用
    TResult<void> deviceRelease();

    //以指定采样率打开声卡(加载驱动 + 创建缓冲区)
    TResult<void> driverOpen(int sampleRate);

    //设置通道掩码: 输入/输出各最多支持32个通道, bit为1表示启用
    //  - 掩码可以为0(部分场景只使用输入或只使用输出), 但两个不能同时为0
    //  - 默认 -1(0xFFFFFFFF) 即全部通道开启, 未设置时走默认
    //  - 缓冲区创建后不允许修改
    TResult<void> setChannelMask(unsigned inputMask, unsigned outputMask);

public:

    //创建缓冲区(硬件双缓冲 + 软件环形缓冲)
    TResult<void> createBuffer();

    TResult<void> getSampleRate();

    TResult<void> supportSampleRate(long value);

    TResult<void> setSampleRate(long value);

    TResult<void> start();

    TResult<void> stop();

    TResult<void> getHaParam();

public:

    //底层驱动线程回调(由 ASIODriver::bufferProcess 转发):
    // 输入: 采集数据填充到输入环形区(覆盖式)
    // 输出: 从输出环形区取数据(不可覆盖, 欠载补零)
    void bufferProcess(long doubleBufferIndex);

    //复位流计数器(输入写位置/输出读写位置), 用于重启设备时保证数据对齐
    void resetCounters();

public:
    //通知时长(ms): 构造传入, 默认10ms, 有效范围10-50ms, 超出抛 std::invalid_argument
    const int notifyMills;
    //最大延迟(ms): 0=自动(2*通知时长, 上限100ms, 硬件缓冲更大则以实际为准); 非0=显式覆盖, 范围[notifyMills,100]
    const int maxDelayMills;

public:
    //播放延迟(ms): 输出预填量对应的延迟(输出精确门限), 引擎建立的可控播放延迟
    int getPlaybackLatencyMs() const
    {
        return (int)((long long)this->outputLimitFrames * 1000 / this->sampleRate);
    }
    //采集延迟(ms): 通知批量边界对应的延迟(引擎每通知周期处理一次采集)
    int getCaptureLatencyMs() const
    {
        return (int)((long long)this->notifyFrames * 1000 / this->sampleRate);
    }

public:
    //缓冲区是否就绪标志
    bool bufferReady;
    //驱动运行标志
    bool driverRuning;

    //当前使用的硬件缓冲区大小(帧)
    long bufferSize;

    //驱动句柄
    IASIO* iasio;

    char driverName[32];
    //输入通道数
    long num_of_capture{ 0 };
    //输出通道数
    long num_of_render{ 0 };

    //缓冲区最小大小
    long bufferMinSize{ 0 };
    //缓冲区最大大小
    long bufferMaxSize{ 0 };
    //首选缓冲区大小
    long bufferPreferredSize{ 0 };
    //缓冲区精度
    long bufferGranularity{ 0 };
    //采样率
    long sampleRate{ 48000 };

    //采样类型
    SampleType sampleType;
    //采样位深(字节)
    int bitDepth;

public:
    //输入通道信息
    std::vector<ASIOChannelInfo> inputChannels;
    //输出通道信息
    std::vector<ASIOChannelInfo> outputChannels;

public:

    //通知门限(帧), bufferSize 的整数倍
    int notifyFrames = 0;
    //最大延迟对应的帧数
    int maxDelayFrames = 0;
    //环形缓冲的缓冲区槽数量(单次回调为1槽, 2的幂次, 掩码按此取模)
    int bufferCount = 0;
    //单通道环形缓冲容量(帧) = bufferCount * bufferSize, 本身不要求2的幂次
    int haBufferSize = 0;
    //输出精确门限(帧), bufferSize 的整数倍(4舍5入), 用于精确控制播放延迟
    int outputLimitFrames = 0;

    //激活的输入通道数
    int _iActiveNum = 0;
    //激活的输出通道数
    int _oActiveNum = 0;
    //总激活的通道数
    int totalActiveNum = 0;

    unsigned inputMask = 0xFFFFFFFFu;  //输入通道掩码, 默认 -1 全开
    unsigned outputMask = 0xFFFFFFFFu; //输出通道掩码, 默认 -1 全开

    //设备的缓冲区字节数(单次硬件缓冲 = bufferSize * bitDepth)
    int deviceByteSize = 0;
    //设备的帧数(单次硬件缓冲 = bufferSize)
    int deviceFrameSize = 0;
    //每个硬件通道的字节数(单次硬件缓冲字节数)
    int perChBufferByteSize = 0;

    //总输入缓冲区(按通道连续排布, 每通道 perChBufferByteSize)
    std::vector<char> _iTotalBuffers;
    //总输出缓冲区(按通道连续排布, 每通道 perChBufferByteSize)
    std::vector<char> _oTotalBuffers;

    //回调缓冲区(激活通道: 输入在前, 输出在后)
    std::vector<ASIOBuffer2> _cbBuffers;

    //输入缓冲区写位置(计数单元=单次回调, 单调递增, 驱动线程写入)
    std::atomic<unsigned> _captureCounter{ 0 };
    //输出缓冲区读位置(计数单元=单次回调, 单调递增, 驱动线程读出)
    std::atomic<unsigned> _renderReadPos{ 0 };
    //输出缓冲区写位置(计数单元=单次回调, 单调递增, 引擎线程写入)
    std::atomic<unsigned> _renderWritePos{ 0 };

    //输出欠载统计(帧): 回调因环形区无数据而播放静音的次数(排查用)
    std::atomic<long long> _outputUnderrunFrames{ 0 };

    long long getOutputUnderrunFrames() const
    {
        return this->_outputUnderrunFrames.load(std::memory_order_relaxed);
    }

private:
    ASIOObject* object;
    ASIOCallbacks* callbacks;
    CLSID driverID;
};
