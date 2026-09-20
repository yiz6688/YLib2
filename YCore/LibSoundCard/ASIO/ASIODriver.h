#pragma once
#include<string>
#include<vector>
#include<memory>
#include<atomic>
#include<mutex>
#include<expected>
#include"../STAWorker.h"
#include"ASIODevice.h"
#include"ASIOCapture.h"
#include"ASIORender.h"
#include"../../WaveBuffer.h"

struct ASIOCallbacks;


//每一个客户端(录音/播放)的通道聚合体, 由 ASIODriver 统一持有
struct _Client2
{
    int type = ASIOFalse;                              //ASIOTrue=输入(录音), ASIOFalse=输出(播放)
    std::vector<int> chs;                              //对应的物理通道号
    std::vector<std::unique_ptr<WaveBuffer>> wbs;      //每通道一个单声道缓冲(与引擎交换数据)
};


//通道聚合视图(快照): 每个激活通道 -> 订阅该通道的客户端缓冲列表
struct _Channel
{
    int channel = 0;              //物理通道号
    int type = ASIOFalse;         //ASIOTrue=输入, ASIOFalse=输出
    std::vector<WaveBuffer*> wbs; //订阅该通道的客户端缓冲(非拥有)
};


//ASIO引擎类:
// 1、底层驱动(ASIODevice)回调线程: 采集填充输入环形区 / 输出环形区取数, 达到通知门限后唤醒引擎
// 2、引擎线程(processor): 条件变量等待, 收到通知后
//    从输入环形区拷贝采集数据到各录音客户端; 从各播放客户端对应通道混频数据到输出环形区
// 3、客户端增删后触发通道聚合视图(COW快照)变化, 旧视图放入GC列表,
//    引擎唤醒后先获取当前聚合视图引用, 处理结束后退出时释放GC列表
class ASIODriver
{

public:
    //notifyMills   : 通知时长(ms), 默认10ms, 有效范围10-50ms, 超出抛 std::invalid_argument
    //maxDelayMills : 最大延迟(ms), 默认0=自动(2*通知时长, 上限100ms, 硬件缓冲更大则以实际为准); 非0=显式覆盖
    ASIODriver(ASIOCallbacks* callbacks, CLSID clsid, int notifyMills = 10, int maxDelayMills = 0);
    ~ASIODriver();

public:
    //获取播放客户端; bufferMills: 客户端每通道缓冲时长(ms), 0=默认(引擎环形容量), 与引擎缓冲独立
    std::expected<ASIORender*, std::string> createRender(int channelMask, int bufferMills = 0);
    //获取录音客户端; bufferMills: 客户端每通道缓冲时长(ms), 0=默认(引擎环形容量), 与引擎缓冲独立
    std::expected<ASIOCapture*, std::string> createCapture(int channelMask, int bufferMills = 0);

    //注册/注销客户端(客户端对象构造/析构时调用), 触发聚合视图变化
    _Client2* registerClient(const std::vector<int>& channels, int type, int bufferMills = 0);
    void removeClient(_Client2* client);

public:
    //ASIO回调函数, 底层驱动线程调用
    void bufferProcess(long doubleBufferIndex, ASIOBool directProcess);

    //播放延迟(ms) / 采集延迟(ms): 上机验证用(延迟预算)
    int getPlaybackLatencyMs() const
    {
        return this->pAsioDevice ? this->pAsioDevice->getPlaybackLatencyMs() : 0;
    }
    int getCaptureLatencyMs() const
    {
        return this->pAsioDevice ? this->pAsioDevice->getCaptureLatencyMs() : 0;
    }

private:
    //引擎线程
    void processor();
    //重建通道聚合视图(COW快照, 用于启动时外部初始化)
    std::shared_ptr<std::vector<_Channel>> buildView();
    //物理通道号 -> 视图索引(增删客户端COW时定位受影响的通道)
    int channelViewIndex(int channel, int type) const;
    //清理GC: 释放旧视图快照/已移除客户端缓冲(引擎每轮结束后调用, 未来可转发线程池)
    void cleanupGC();
    //处理数据: inBatches=输入整批数(0表示无可读输入), 输出按回调已消费量补充
    void processData(const std::vector<_Channel>& view, int inBatches);
    //从播放客户端混频写入输出环形区(精确门限, 不可覆盖), 返回实际写入帧数
    int mixOutput(const std::vector<_Channel>& view, int frames);
    //播放数据预填(驱动启动前由start串行调用, 建立可控的起始延迟)
    void prefillOutput(const std::vector<_Channel>& view);
    //float -> 设备采样格式字节
    void mixToBytes(const float* src, char* dst, int frames);

    static unsigned __stdcall threadProc(void* param)
    {
        ASIODriver* driver = reinterpret_cast<ASIODriver*>(param);
        driver->processor();
        return 0;
    }

public:
    //打开驱动
    STAType driverOpen();
    STAType createBuffer();
    STAType start();
    STAType stop();
    STAType setChannelMask(unsigned inputMask, unsigned outputMask);

    //获取采样率
    TResult<int> getSampleRate();
    //设置采样率
    TResult<void> setSampleRate(long sampleRate);

    //获取录音器数量
    int getCaptureCount();
    //获取播放器数量
    int getRenderCount();

    std::string getCaptureName(int channel);
    std::string getRenderName(int channel);

    TResult<void> Initialize(unsigned inputMask, unsigned outputMask);
    TResult<void> Release();

    //引擎诊断统计(排查丢帧/欠载/延迟打满)
    struct ASIODiagnostics
    {
        long long inputDroppedFrames = 0;    //输入覆盖丢弃: 引擎落后太多, 采集数据被覆盖丢失
        long long outputUnderrunFrames = 0;  //输出欠载: 环形区无数据, 播放静音
        long long outputFullFrames = 0;      //输出门限打满: 引擎想写入但因精确门限写不进(播放延迟已达上限)
        long long processUs = 0;             //引擎处理累计耗时(微秒), 除以 processCount 得平均单次处理耗时
        long long processCount = 0;          //引擎处理次数(每次唤醒)
    };
    ASIODiagnostics getDiagnostics() const;

public:
    //创建驱动, notifyMills: 通知时长(ms), 默认10ms, 有效范围10-50ms; maxDelayMills: 0=自动
    static std::expected<ASIODriver*, std::string> createDriver(CLSID clsid, int notifyMills = 10, int maxDelayMills = 0);
    static std::expected<void, std::string> releaseDriver(ASIODriver* driver);

public:
    //混频峰值限幅(防削波): 多客户端混频求和后峰值超过1.0时, 整块等比缩放
    bool mixPeakLimit = true;

    CLSID asioID;

    //设备驱动
    std::unique_ptr<ASIODevice> pAsioDevice;
    //单线程调度器(STA, 设备相关操作统一在STA线程执行)
    STAWorker staWorker;

    //引擎线程
    HANDLE hThread = INVALID_HANDLE_VALUE;
    std::atomic<bool> processFlag{ false };   //引擎运行标志

    //引擎通知(系统事件, 等待期间不持锁):
    // hNotify: 自动复位事件, 单脉冲即可; 工作量由计数器(capCounter - iReadPos)决定
    // hExit  : 手动复位事件, stop/析构时置位让引擎退出
    HANDLE hNotify = INVALID_HANDLE_VALUE;
    HANDLE hExit = INVALID_HANDLE_VALUE;
    std::mutex mtx;                           //保护客户端注册表/视图临界区(等待期间不持有)

    //客户端注册表(拥有客户端对象)
    std::vector<std::unique_ptr<_Client2>> clients;
    //已移除客户端, 等待引擎每轮结束清理GC时释放(缓冲仍需存活, 保证旧视图引用不悬垂)
    std::vector<std::unique_ptr<_Client2>> gcClients;

    //当前聚合视图快照(COW): 客户端增删时直接增量更新并原子换新, 旧视图入viewGC
    std::shared_ptr<std::vector<_Channel>> chViews;
    //旧视图GC列表, 引擎每轮结束清理
    std::vector<std::shared_ptr<std::vector<_Channel>>> viewGC;

    //输入消费位置(计数单元=单次回调, 单调递增, 引擎线程独占)
    unsigned iReadPos = 0;
    //上次输出读位置(引擎线程独占), 用于按回调已消费量补充输出, 与通知节奏解耦
    unsigned lastRenderReadPos = 0;

    //诊断统计(引擎线程写, 查询线程读)
    std::atomic<long long> _inputDroppedFrames{ 0 };
    std::atomic<long long> _outputFullFrames{ 0 };
    std::atomic<long long> _procUs{ 0 };      //processData累计耗时(微秒)
    std::atomic<long long> _procCount{ 0 };   //processData调用次数

private:
    //引擎临时缓冲(在引擎线程启动时按延迟参数分配)
    std::vector<char> _ioTemp;   //输入临时缓冲(通知帧字节)
    std::vector<float> _mixBuf;  //输出混频缓冲
    std::vector<float> _tmpBuf;  //输出单客户端临时缓冲
    std::vector<char> _oConvert; //输出转换字节缓冲
};
