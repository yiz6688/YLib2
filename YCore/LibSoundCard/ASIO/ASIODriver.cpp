#include"base_config.hpp"
#include"ASIODriver.h"
#include"asiosdk/asio.h"
#include<array>
#include<process.h>
#include<cstring>
#include<algorithm>
#include<chrono>
#include"../../NumUtils.h"
#include"../../SampleConv.h"
#include"../../BitConverter.h"

using namespace std;
using exp_ns::expected;
using exp_ns::unexpected;
using fmt_ns::println;
using fmt_ns::print;

constexpr unsigned MAX_DRIVER_NUM = 2;  //最大支持的驱动数量


struct ASIOEntity
{
    unique_ptr<ASIODriver> pASIODriver = nullptr;
    ASIOCallbacks callback = {};
};


//回调函数模板, 批量生成回调函数
template<unsigned N>
struct __ASIOCALLBACK__
{
    static void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
    {
        asioEntity->pASIODriver->bufferProcess(doubleBufferIndex, directProcess);
    }
    static void sampleRateDidChange(ASIOSampleRate sRate)
    {
    }
    static long asioMessage(long selector, long value, void* message, double* opt)
    {
        long ret = 0;
        switch (selector)
        {
        case kAsioSelectorSupported:
            if (value == kAsioResetRequest
                || value == kAsioEngineVersion
                || value == kAsioResyncRequest
                || value == kAsioLatenciesChanged
                || value == kAsioSupportsTimeInfo
                || value == kAsioSupportsTimeCode
                || value == kAsioSupportsInputMonitor)
                ret = 1L;
            break;
        case kAsioResetRequest:
            ret = 1L;
            break;
        case kAsioResyncRequest:
            ret = 1L;
            break;
        case kAsioLatenciesChanged:
            ret = 1L;
            break;
        case kAsioEngineVersion:
            ret = 2L;
            break;
        case kAsioSupportsTimeInfo:
            ret = 0;
            break;
        case kAsioSupportsTimeCode:
            ret = 0;
            break;
        }
        return ret;
    }
    static ASIOTime* bufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
    {
        return nullptr;
    }
    //绑定的对象
    static inline ASIOEntity* asioEntity = nullptr;
};

template<unsigned... ids>
array<ASIOEntity, MAX_DRIVER_NUM> registerCallbacks(std::integer_sequence<unsigned, ids...>)
{
    array<ASIOEntity, MAX_DRIVER_NUM> entitys;
    ([&object = entitys[ids]]()
        {
            object.callback.bufferSwitch = __ASIOCALLBACK__<ids>::bufferSwitch;
            object.callback.asioMessage = __ASIOCALLBACK__<ids>::asioMessage;
            object.callback.bufferSwitchTimeInfo = __ASIOCALLBACK__<ids>::bufferSwitchTimeInfo;
            object.callback.sampleRateDidChange = __ASIOCALLBACK__<ids>::sampleRateDidChange;
            __ASIOCALLBACK__<ids>::asioEntity = &object;
            fmt_ns::println("注册:{}", ids);
        }(), ...);

    return entitys;
}

//对象数组, 根据支持的板卡数生成数组
array<ASIOEntity, MAX_DRIVER_NUM> ASIOEntitys = registerCallbacks(std::make_integer_sequence<unsigned, MAX_DRIVER_NUM>());
//驱动锁, 用于全局获取驱动时进行锁定
static mutex gMTX;


ASIODriver::ASIODriver(ASIOCallbacks* callbacks, CLSID clsid, int notifyMills, int maxDelayMills, bool exclusiveMode)
    : exclusiveMode(exclusiveMode), asioID(clsid)
{
    //notifyMills/maxDelayMills 有效范围由 ASIODevice 构造函数校验(超出抛异常)
    this->pAsioDevice = std::make_unique<ASIODevice>(callbacks, clsid, notifyMills, maxDelayMills);

    //引擎通知/退出事件(系统句柄, 等待期间不持锁)
    this->hNotify = CreateEvent(NULL, FALSE, FALSE, NULL);   //自动复位: 通知单脉冲
    this->hExit = CreateEvent(NULL, TRUE, FALSE, NULL);      //手动复位: 停止
}

ASIODriver::~ASIODriver()
{
    //停止引擎线程
    this->processFlag.store(false);
    if (this->hNotify != INVALID_HANDLE_VALUE) SetEvent(this->hNotify);
    if (this->hExit != INVALID_HANDLE_VALUE) SetEvent(this->hExit);
    if (this->hThread != INVALID_HANDLE_VALUE)
    {
        WaitForSingleObject(this->hThread, INFINITE);
        CloseHandle(this->hThread);
        this->hThread = INVALID_HANDLE_VALUE;
    }

    //设备相关操作统一在STA线程释放
    if (this->pAsioDevice != nullptr)
    {
        auto future = this->staWorker.submit(
            [this] {
                println("主动释放资源");
                this->pAsioDevice.reset();
                return STAType();
            });
        (void)future.get();
    }

    if (this->hNotify != INVALID_HANDLE_VALUE)
    {
        CloseHandle(this->hNotify);
        this->hNotify = INVALID_HANDLE_VALUE;
    }
    if (this->hExit != INVALID_HANDLE_VALUE)
    {
        CloseHandle(this->hExit);
        this->hExit = INVALID_HANDLE_VALUE;
    }

    this->clients.clear();
    this->gcClients.clear();
    this->cleanupGC();   //释放旧视图
    if (auto* v = this->chViews.load(std::memory_order_relaxed))
    {
        delete v;   //释放当前视图
        this->chViews.store(nullptr, std::memory_order_relaxed);
    }
}

TResult<ASIORender*> ASIODriver::createRender(int channelMask, int bufferMills)
{
    if (this->pAsioDevice == nullptr || !this->pAsioDevice->bufferReady)
    {
        return exp_ns::unexpected("驱动缓冲区未就绪");
    }
    if (channelMask == 0)
    {
        return exp_ns::unexpected("通道掩码不能为0");
    }
    int outputMask = (int)this->pAsioDevice->outputMask;
    if ((channelMask & outputMask) != channelMask)
    {
        return exp_ns::unexpected("通道掩码超出激活的输出通道");
    }

    //独占模式: 每个输出通道只允许一个客户端(原子占用, 失败即已占用)
    if (this->exclusiveMode)
    {
        unsigned cur = this->_claimedOutputChannels.load(std::memory_order_acquire);
        while (true)
        {
            if (cur & (unsigned)channelMask)
            {
                return exp_ns::unexpected("独占模式下输出通道已被其他客户端占用");
            }
            if (this->_claimedOutputChannels.compare_exchange_weak(cur, cur | (unsigned)channelMask,
                std::memory_order_acq_rel, std::memory_order_acquire))
            {
                break;
            }
        }
    }

    ASIORender* pRender = new ASIORender(this, channelMask, bufferMills);
    return pRender;
}

TResult<ASIOCapture*> ASIODriver::createCapture(int channelMask, int bufferMills)
{
    if (this->pAsioDevice == nullptr || !this->pAsioDevice->bufferReady)
    {
        return exp_ns::unexpected("驱动缓冲区未就绪");
    }
    if (channelMask == 0)
    {
        return exp_ns::unexpected("通道掩码不能为0");
    }
    int inputMask = (int)this->pAsioDevice->inputMask;
    if ((channelMask & inputMask) != channelMask)
    {
        return exp_ns::unexpected("通道掩码超出激活的输入通道");
    }

    //独占模式: 每个输入通道只允许一个客户端(原子占用, 失败即已占用)
    if (this->exclusiveMode)
    {
        unsigned cur = this->_claimedInputChannels.load(std::memory_order_acquire);
        while (true)
        {
            if (cur & (unsigned)channelMask)
            {
                return exp_ns::unexpected("独占模式下输入通道已被其他客户端占用");
            }
            if (this->_claimedInputChannels.compare_exchange_weak(cur, cur | (unsigned)channelMask,
                std::memory_order_acq_rel, std::memory_order_acquire))
            {
                break;
            }
        }
    }

    ASIOCapture* pCapture = new ASIOCapture(this, channelMask, bufferMills);
    return pCapture;
}

_Client2* ASIODriver::registerClient(const std::vector<int>& channels, int type, int bufferMills)
{
    if (this->pAsioDevice == nullptr || !this->pAsioDevice->bufferReady)
    {
        return nullptr;
    }

    auto client = std::make_unique<_Client2>();
    client->type = type;
    client->chs = channels;
    //客户端缓冲与引擎缓冲独立: 按时间(bufferMills)换算帧数, 0=默认用引擎环形容量
    int frames = 0;
    if (bufferMills > 0)
    {
        frames = (int)((long long)bufferMills * this->pAsioDevice->sampleRate / 1000);
        if (frames < this->pAsioDevice->notifyFrames)
        {
            frames = this->pAsioDevice->notifyFrames;  //至少容纳一个通知周期
        }
    }
    else
    {
        frames = this->pAsioDevice->haBufferSize;
    }
    for (size_t k = 0; k < channels.size(); k++)
    {
        auto wb = std::make_unique<WaveBuffer>(this->pAsioDevice->sampleType, frames, 1);
        client->wbs.push_back(std::move(wb));
    }

    _Client2* raw = client.get();
    {
        std::lock_guard<std::mutex> lk(this->mtx);
        this->clients.push_back(std::move(client));

        //COW增量更新视图: 拷贝当前视图, 只修改新增客户端涉及的通道, 原子换新, 旧视图入GC
        if (auto* cur = this->chViews.load(std::memory_order_acquire))
        {
            auto* newView = new _ChannelView(*cur);
            for (size_t k = 0; k < channels.size(); k++)
            {
                int idx = this->channelViewIndex(channels[k], type);
                if (idx >= 0)
                {
                    (*newView)[idx].wbs.push_back(raw->wbs[k].get());
                }
            }
            this->viewGC.push_back(cur);
            this->chViews.store(newView, std::memory_order_release);
        }
    }
    if (this->hNotify != INVALID_HANDLE_VALUE)
    {
        SetEvent(this->hNotify);   //唤醒引擎抓取新视图
    }
    return raw;
}

void ASIODriver::removeClient(_Client2* client)
{
    if (client == nullptr)
    {
        return;
    }

    std::unique_lock<std::mutex> lk(this->mtx);
    auto it = std::find_if(this->clients.begin(), this->clients.end(),
        [client](const std::unique_ptr<_Client2>& u) { return u.get() == client; });
    if (it == this->clients.end())
    {
        return;
    }

    //移入客户端GC: 缓冲仍需存活, 等引擎每轮结束清理时释放(旧视图引用不悬垂)
    this->gcClients.push_back(std::move(*it));
    this->clients.erase(it);

    //独占模式: 释放该客户端占用的通道(原子归还)
    if (this->exclusiveMode && !client->chs.empty())
    {
        unsigned mask = 0;
        for (int ch : client->chs)
        {
            mask |= (1u << ch);
        }
        if (client->type == ASIOTrue)
        {
            this->_claimedInputChannels.fetch_and(~mask, std::memory_order_acq_rel);
        }
        else
        {
            this->_claimedOutputChannels.fetch_and(~mask, std::memory_order_acq_rel);
        }
    }

    //COW增量更新视图: 拷贝当前视图, 只移除被删客户端涉及的通道, 原子换新, 旧视图入GC
    if (auto* cur = this->chViews.load(std::memory_order_acquire))
    {
        auto* newView = new _ChannelView(*cur);
        for (size_t k = 0; k < client->chs.size(); k++)
        {
            int idx = this->channelViewIndex(client->chs[k], client->type);
            if (idx >= 0)
            {
                auto& wbs = (*newView)[idx].wbs;
                wbs.erase(std::remove(wbs.begin(), wbs.end(), client->wbs[k].get()), wbs.end());
            }
        }
        this->viewGC.push_back(cur);
        this->chViews.store(newView, std::memory_order_release);
    }

    lk.unlock();
    if (this->hNotify != INVALID_HANDLE_VALUE)
    {
        SetEvent(this->hNotify);   //唤醒引擎抓取新视图
    }

    //引擎未运行时, 立即清理GC(旧视图/已删客户端无人使用)
    if (!this->processFlag.load())
    {
        std::lock_guard<std::mutex> lk2(this->mtx);
        this->cleanupGC();
    }
}

/**
 * 物理通道号 -> 视图索引
 * 视图顺序与 _cbBuffers 一致: 激活输入通道在前, 输出通道在后
 */
int ASIODriver::channelViewIndex(int channel, int type) const
{
    auto& dev = this->pAsioDevice;
    if (dev == nullptr)
    {
        return -1;
    }
    if (type == ASIOTrue)
    {
        for (int i = 0; i < dev->_iActiveNum; i++)
        {
            if (dev->_cbBuffers[i].channel == channel)
            {
                return i;
            }
        }
    }
    else
    {
        for (int i = 0; i < dev->_oActiveNum; i++)
        {
            if (dev->_cbBuffers[dev->_iActiveNum + i].channel == channel)
            {
                return dev->_iActiveNum + i;
            }
        }
    }
    return -1;
}

/**
 * 清理GC: 释放旧视图快照与已移除客户端缓冲。
 * 引擎每轮处理结束后调用; 未来可转发到线程池异步执行, 此处先同步清理。
 */
void ASIODriver::cleanupGC()
{
    for (auto* v : this->viewGC)
    {
        delete v;
    }
    this->viewGC.clear();
    this->gcClients.clear();
}

/**
 * 引擎线程:
 * 1、启动前的初始化(复位计数器/分配缓冲/构建视图/预填输出)由 start() 外部串行完成
 * 2、等待系统事件(hNotify通知脉冲 / hExit退出), 等待期间不持锁
 * 3、唤醒后原子抓取当前聚合视图引用, 再处理数据:
 *    - 输入: 从输入环形区拷贝采集数据到各录音客户端(仅当有整批数据, 通知对应帧数的整倍数)
 *    - 输出: 从各播放客户端对应通道混频数据到输出环形区(按回调已消费量补充, 与通知节奏解耦)
 * 4、本轮处理结束后清理GC(旧视图快照/已移除客户端缓冲)
 *
 * 说明: 通知只是"单脉冲唤醒", 工作量由计数器(capCounter - iReadPos)决定;
 *      客户端增删在对应函数内COW增量更新视图并原子换新, 引擎每轮抓取即可始终拿到有效视图.
 */
void ASIODriver::processor()
{
    auto& dev = this->pAsioDevice;
    if (dev == nullptr || !dev->bufferReady)
    {
        println("引擎退出: 缓冲区未就绪");
        this->processFlag.store(false);
        return;
    }

    //引擎主循环: 等待系统事件(hNotify通知 / hExit退出), 等待期间不持锁
    HANDLE waits[2] = { this->hNotify, this->hExit };
    while (this->processFlag.load())
    {
        DWORD wr = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wr == WAIT_FAILED || wr == WAIT_OBJECT_0 + 1)
        {
            break;   //失败或退出事件
        }
        if (!this->processFlag.load())
        {
            break;   //hNotify唤醒但已停止(可能同时置位了hExit)
        }

        //2.1 原子抓取当前聚合视图指针(客户端增删已COW换新, 无需持锁; 本轮结束前由viewGC持有旧视图, 不会悬垂)
        _ChannelView* view = this->chViews.load(std::memory_order_acquire);

        //2.2 处理数据(不持锁)
        if (view)
        {
            auto t0 = std::chrono::steady_clock::now();

            unsigned notifyBuffers = (unsigned)(dev->notifyFrames / dev->deviceFrameSize);  //每次通知的回调次数
            auto capCounter = dev->_captureCounter.load(std::memory_order_acquire);         //回调计数
            unsigned available = capCounter - this->iReadPos;                               //未读回调数
            //输入覆盖式: 落后太多时丢弃旧数据, 只保留最新通知帧, 保持数据对齐
            unsigned maxLag = (unsigned)dev->bufferCount - notifyBuffers;
            if (available > maxLag)
            {
                unsigned dropped = available - notifyBuffers;   //被覆盖丢弃的回调数
                this->_inputDroppedFrames.fetch_add((long long)dropped * dev->deviceFrameSize, std::memory_order_relaxed);
                this->iReadPos = capCounter - notifyBuffers;
                available = notifyBuffers;
            }
            //inBatches: 输入整批数(通知对应帧数的整倍数; 为0时表示无可读输入, 不读)
            int inBatches = (int)(available / notifyBuffers);
            this->processData(*view, inBatches);

            auto t1 = std::chrono::steady_clock::now();
            this->_procUs.fetch_add((long long)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(),
                std::memory_order_relaxed);
            this->_procCount.fetch_add(1, std::memory_order_relaxed);
        }

        //2.3 本轮结束清理GC(旧视图/已移除客户端, 引擎本轮视图已不再引用)
        {
            std::lock_guard<std::mutex> lk(this->mtx);
            this->cleanupGC();
        }
    }

    {
        std::lock_guard<std::mutex> lk(this->mtx);
        this->cleanupGC();
    }
    println("监听线程退出");
}

/**
 * 构建聚合视图快照(返回新分配对象, 调用方负责交给原子指针或释放):
 * 视图顺序与 _cbBuffers 一致: 激活的输入通道在前, 输出通道在后
 * 每个通道关联所有订阅该通道的客户端缓冲
 */
_ChannelView* ASIODriver::buildView()
{
    auto& dev = this->pAsioDevice;
    int iActive = dev->_iActiveNum;
    int oActive = dev->_oActiveNum;
    int total = dev->totalActiveNum;

    auto* view = new _ChannelView(total);
    for (int i = 0; i < total; i++)
    {
        (*view)[i].channel = dev->_cbBuffers[i].channel;
        (*view)[i].type = dev->_cbBuffers[i].type;
    }

    //物理通道号 -> 视图索引
    std::vector<int> inMap((size_t)dev->num_of_capture, -1);
    std::vector<int> outMap((size_t)dev->num_of_render, -1);
    for (int i = 0; i < iActive; i++) inMap[(*view)[i].channel] = i;
    for (int i = 0; i < oActive; i++) outMap[(*view)[iActive + i].channel] = iActive + i;

    for (auto& c : this->clients)
    {
        for (size_t k = 0; k < c->chs.size(); k++)
        {
            int ch = c->chs[k];
            int idx = -1;
            if (c->type == ASIOTrue)
            {
                if (ch >= 0 && ch < (int)dev->num_of_capture) idx = inMap[ch];
            }
            else
            {
                if (ch >= 0 && ch < (int)dev->num_of_render) idx = outMap[ch];
            }
            if (idx >= 0)
            {
                (*view)[idx].wbs.push_back(c->wbs[k].get());
            }
        }
    }
    return view;
}

/**
 * 处理数据:
 * 输入: 读 inBatches 个整批(每批 = notifyBuffers 个槽, 需处理槽回绕);
 *       仅当 inBatches>0 且激活输入通道时读, 避免客户端增删唤醒时读到脏数据
 * 输出: 按回调已消费量(drained)补充混频, 与通知节奏解耦, 天然匹配任何唤醒时机
 */
void ASIODriver::processData(const _ChannelView& view, int inBatches)
{
    auto& dev = this->pAsioDevice;
    int bitDepth = dev->bitDepth;
    int notifyBuffers = dev->notifyFrames / dev->deviceFrameSize;   //每次通知的回调次数
    int bufferCount = dev->bufferCount;
    int deviceByteSize = dev->deviceByteSize;
    int iActive = dev->_iActiveNum;
    int inBytes = notifyBuffers * deviceByteSize;   //每次通知的字节数

    //----- 输入: 环形区(按槽) -> 各录音客户端 -----
    if (iActive > 0 && inBatches > 0)
    {
        for (int b = 0; b < inBatches; b++)
        {
            unsigned startSlot = this->iReadPos & (unsigned)(bufferCount - 1);
            int tailSlots = bufferCount - (int)startSlot;
            int seg1 = tailSlots < notifyBuffers ? tailSlots : notifyBuffers;   //到尾部的槽数
            int seg2 = notifyBuffers - seg1;                                     //回绕到头部的槽数

            for (int i = 0; i < iActive; i++)
            {
                auto& chView = view[i];
                if (chView.wbs.empty())
                {
                    continue;
                }
                const char* ring = dev->_cbBuffers[i]._buf;
                std::memcpy(this->_ioTemp.data(), ring + (size_t)startSlot * deviceByteSize,
                    (size_t)seg1 * deviceByteSize);
                if (seg2 > 0)
                {
                    std::memcpy(this->_ioTemp.data() + (size_t)seg1 * deviceByteSize, ring,
                        (size_t)seg2 * deviceByteSize);
                }
                for (WaveBuffer* wb : chView.wbs)
                {
                    wb->writeBytes(this->_ioTemp.data(), inBytes);
                }
            }
            this->iReadPos += (unsigned)notifyBuffers;
        }
    }

    //----- 输出: 补充回调已消费的量(与通知节奏解耦, 不多不少, 避免门限打满误报) -----
    auto rpos = dev->_renderReadPos.load(std::memory_order_acquire);
    unsigned drained = rpos - this->lastRenderReadPos;
    this->lastRenderReadPos = rpos;
    if (drained > 0)
    {
        int frames = (int)drained * dev->deviceFrameSize;
        if (this->exclusiveMode)
        {
            this->copyOutputDirect(view, frames);   //独占: 免混频直拷
        }
        else
        {
            this->mixOutput(view, frames);          //共享: 浮点混频
        }
    }
}

/**
 * 从播放客户端混频写入输出环形区:
 * 1、混频: 同一通道的多个客户端缓冲按float相加
 * 2、精确门限: 只填充到 outputLimitFrames, 不覆盖已写入数据, 精确控制播放延迟
 * 3、float -> 设备采样格式字节后写入环形区(按缓冲区槽组织, 支持槽回绕)
 */
int ASIODriver::mixOutput(const _ChannelView& view, int frames)
{
    auto& dev = this->pAsioDevice;
    int iActive = dev->_iActiveNum;
    int oActive = dev->_oActiveNum;
    if (oActive == 0)
    {
        return 0;
    }

    auto rpos = dev->_renderReadPos.load(std::memory_order_acquire);    //回调计数
    auto wpos = dev->_renderWritePos.load(std::memory_order_relaxed);   //回调计数
    int inFlight = (int)(wpos - rpos);                                  //在途回调数
    int limitBuffers = dev->outputLimitFrames / dev->deviceFrameSize;   //精确门限对应的回调数
    int wantBuffers = frames / dev->deviceFrameSize;
    int toWriteBuffers = limitBuffers - inFlight;
    if (toWriteBuffers > wantBuffers)
    {
        toWriteBuffers = wantBuffers;
    }
    if (toWriteBuffers <= 0)
    {
        //精确门限已打满: 数据滞留客户端, 播放延迟已达上限
        this->_outputFullFrames.fetch_add((long long)wantBuffers * dev->deviceFrameSize, std::memory_order_relaxed);
        return 0;  //已填满精确门限, 不再写入
    }
    if (toWriteBuffers < wantBuffers)
    {
        this->_outputFullFrames.fetch_add((long long)(wantBuffers - toWriteBuffers) * dev->deviceFrameSize,
            std::memory_order_relaxed);
    }
    int toWriteFrames = toWriteBuffers * dev->deviceFrameSize;

    int bufferCount = dev->bufferCount;
    int deviceByteSize = dev->deviceByteSize;
    unsigned startSlot = (unsigned)(wpos & (unsigned)(bufferCount - 1));
    int tailSlots = bufferCount - (int)startSlot;
    int seg1 = tailSlots < toWriteBuffers ? tailSlots : toWriteBuffers;   //到尾部的槽数
    int seg2 = toWriteBuffers - seg1;                                     //回绕到头部的槽数
    int outBytes = toWriteFrames * dev->bitDepth;

    for (int j = 0; j < oActive; j++)
    {
        auto& chView = view[iActive + j];
        if (chView.wbs.size() == 1)
        {
            //快路径: 单客户端且采样格式与设备一致, 直接拷贝设备原始字节(免浮点往返)
            int got = chView.wbs[0]->readBytes(this->_oConvert.data(), outBytes);
            if (got < outBytes)
            {
                std::memset(this->_oConvert.data() + got, 0, outBytes - got);
            }
        }
        else
        {
            //混频路径: float求和 + 峰值限幅 + 转设备格式
            std::fill(this->_mixBuf.begin(), this->_mixBuf.end(), 0.0f);
            for (WaveBuffer* wb : chView.wbs)
            {
                int n = wb->readFloat(this->_tmpBuf.data(), toWriteFrames);
                for (int m = 0; m < n; m++)
                {
                    this->_mixBuf[m] += this->_tmpBuf[m];
                }
            }

            //峰值限幅(防削波): 峰值超过1.0时整块等比缩放, 避免转整数时削波
            if (this->mixPeakLimit)
            {
                float peak = 0.0f;
                for (int m = 0; m < toWriteFrames; m++)
                {
                    float v = this->_mixBuf[m];
                    if (v < 0.0f) v = -v;
                    if (v > peak) peak = v;
                }
                if (peak > 1.0f)
                {
                    float gain = 1.0f / peak;
                    for (int m = 0; m < toWriteFrames; m++)
                    {
                        this->_mixBuf[m] *= gain;
                    }
                }
            }

            this->mixToBytes(this->_mixBuf.data(), this->_oConvert.data(), toWriteFrames);
        }

        char* ring = dev->_cbBuffers[iActive + j]._buf;
        std::memcpy(ring + (size_t)startSlot * deviceByteSize, this->_oConvert.data(),
            (size_t)seg1 * deviceByteSize);
        if (seg2 > 0)
        {
            std::memcpy(ring, this->_oConvert.data() + (size_t)seg1 * deviceByteSize,
                (size_t)seg2 * deviceByteSize);
        }
    }

    dev->_renderWritePos.fetch_add((unsigned)toWriteBuffers, std::memory_order_release);
    return toWriteFrames;
}

/**
 * 独占模式输出: 每通道至多一个客户端, 直接拷贝设备原始字节(免浮点转换/免混频/免限幅)
 * 精确门限/不可覆盖语义与 mixOutput 一致, 返回实际写入帧数
 */
int ASIODriver::copyOutputDirect(const _ChannelView& view, int frames)
{
    auto& dev = this->pAsioDevice;
    int iActive = dev->_iActiveNum;
    int oActive = dev->_oActiveNum;
    if (oActive == 0)
    {
        return 0;
    }

    auto rpos = dev->_renderReadPos.load(std::memory_order_acquire);
    auto wpos = dev->_renderWritePos.load(std::memory_order_relaxed);
    int inFlight = (int)(wpos - rpos);
    int limitBuffers = dev->outputLimitFrames / dev->deviceFrameSize;
    int wantBuffers = frames / dev->deviceFrameSize;
    int toWriteBuffers = limitBuffers - inFlight;
    if (toWriteBuffers > wantBuffers)
    {
        toWriteBuffers = wantBuffers;
    }
    if (toWriteBuffers <= 0)
    {
        this->_outputFullFrames.fetch_add((long long)wantBuffers * dev->deviceFrameSize, std::memory_order_relaxed);
        return 0;
    }
    if (toWriteBuffers < wantBuffers)
    {
        this->_outputFullFrames.fetch_add((long long)(wantBuffers - toWriteBuffers) * dev->deviceFrameSize,
            std::memory_order_relaxed);
    }
    int toWriteFrames = toWriteBuffers * dev->deviceFrameSize;

    int bufferCount = dev->bufferCount;
    int deviceByteSize = dev->deviceByteSize;
    unsigned startSlot = (unsigned)(wpos & (unsigned)(bufferCount - 1));
    int tailSlots = bufferCount - (int)startSlot;
    int seg1 = tailSlots < toWriteBuffers ? tailSlots : toWriteBuffers;
    int seg2 = toWriteBuffers - seg1;
    int outBytes = toWriteFrames * dev->bitDepth;

    for (int j = 0; j < oActive; j++)
    {
        auto& chView = view[iActive + j];
        if (chView.wbs.size() == 1)
        {
            //单客户端: 直接拷贝设备原始字节(独占模式保证每通道至多一个)
            int got = chView.wbs[0]->readBytes(this->_oConvert.data(), outBytes);
            if (got < outBytes)
            {
                std::memset(this->_oConvert.data() + got, 0, outBytes - got);
            }
        }
        else
        {
            //无客户端: 静音
            std::memset(this->_oConvert.data(), 0, outBytes);
        }

        char* ring = dev->_cbBuffers[iActive + j]._buf;
        std::memcpy(ring + (size_t)startSlot * deviceByteSize, this->_oConvert.data(),
            (size_t)seg1 * deviceByteSize);
        if (seg2 > 0)
        {
            std::memcpy(ring, this->_oConvert.data() + (size_t)seg1 * deviceByteSize,
                (size_t)seg2 * deviceByteSize);
        }
    }

    dev->_renderWritePos.fetch_add((unsigned)toWriteBuffers, std::memory_order_release);
    return toWriteFrames;
}

//播放数据预填(驱动启动前调用, 建立可控的起始播放延迟)
void ASIODriver::prefillOutput(const _ChannelView& view)
{
    if (this->pAsioDevice->_oActiveNum > 0)
    {
        if (this->exclusiveMode)
        {
            this->copyOutputDirect(view, this->pAsioDevice->outputLimitFrames);
        }
        else
        {
            this->mixOutput(view, this->pAsioDevice->outputLimitFrames);
        }
    }
}

//float -> 设备采样格式字节
void ASIODriver::mixToBytes(const float* src, char* dst, int frames)
{
    switch (this->pAsioDevice->sampleType)
    {
    case SampleType::IEEE32:
        std::memcpy(dst, src, (size_t)frames * 4);
        break;
    case SampleType::INT16:
        SampleConv::FloattoInt16(const_cast<float*>(src), frames, (short*)dst);
        break;
    case SampleType::INT32:
        SampleConv::FloattoInt32(const_cast<float*>(src), frames, (int*)dst);
        break;
    case SampleType::INT24:
        SampleConv::FloattoInt24Byte(const_cast<float*>(src), frames, dst);
        break;
    default:
        std::memset(dst, 0, (size_t)frames * 4);
        break;
    }
}

/**
 * 底层驱动回调(驱动线程):
 * 1、输入采集填充到输入环形区 / 输出环形区取数
 * 2、达到通知门限(整倍数)后发单脉冲唤醒引擎
 */
void ASIODriver::bufferProcess(long doubleBufferIndex, ASIOBool directProcess)
{
    (void)directProcess;
    auto& dev = this->pAsioDevice;
    if (dev == nullptr || !dev->bufferReady)
    {
        return;
    }

    dev->bufferProcess(doubleBufferIndex);

    //达到通知门限(整倍数)时发脉冲; 计数复用设备 _captureCounter, 无需额外累加器
    //注意: 纯输出设备(无输入通道)时 _captureCounter 恒为0, 0%N==0 恒真, 即每回调唤醒一次,
    //      配合按消费量补充输出, 该行为无碍(纯输出本就按回调节奏补齐即可)
    unsigned notifyBuffers = (unsigned)(dev->notifyFrames / dev->deviceFrameSize);
    auto cc = dev->_captureCounter.load(std::memory_order_relaxed);
    if (cc % notifyBuffers == 0)
    {
        if (this->hNotify != INVALID_HANDLE_VALUE)
        {
            SetEvent(this->hNotify);   //单脉冲唤醒(自动复位事件), 工作量由计数器决定, 事件合并无碍
        }
    }
}

STAType ASIODriver::driverOpen()
{
    auto future = this->staWorker.submit(
        [this] {
            return this->pAsioDevice->deviceInit();
        });

    auto result = future.get();
    if (!result)
    {
        return result;
    }

    return {};
}

STAType ASIODriver::createBuffer()
{
    auto future = this->staWorker.submit(
        [this] {
            return this->pAsioDevice->createBuffer();
        });

    return future.get();
}

STAType ASIODriver::start()
{
    if (this->processFlag.load())
    {
        return {};  //已启动
    }
    if (this->pAsioDevice == nullptr || !this->pAsioDevice->bufferReady)
    {
        return exp_ns::unexpected("缓冲区未创建");
    }

    this->processFlag.store(true);
    if (this->hExit != INVALID_HANDLE_VALUE)
    {
        ResetEvent(this->hExit);   //清除上次stop残留
    }

    //===== 外部串行初始化(不在引擎线程执行, 初始化完成后再启动线程) =====
    this->pAsioDevice->resetCounters();
    int notifyFrames = this->pAsioDevice->notifyFrames;
    int bitDepth = this->pAsioDevice->bitDepth;
    int mixLen = this->pAsioDevice->outputLimitFrames > this->pAsioDevice->notifyFrames
        ? this->pAsioDevice->outputLimitFrames : this->pAsioDevice->notifyFrames;
    this->_ioTemp.resize((size_t)notifyFrames * bitDepth);
    this->_mixBuf.resize(mixLen);
    this->_tmpBuf.resize(mixLen);
    this->_oConvert.resize((size_t)mixLen * bitDepth);
    this->iReadPos = 0;
    this->lastRenderReadPos = 0;
    {
        std::lock_guard<std::mutex> lk(this->mtx);
        auto* view = this->buildView();
        auto* old = this->chViews.exchange(view, std::memory_order_acq_rel);   //重启时替换旧视图
        if (old)
        {
            this->viewGC.push_back(old);
        }
        this->prefillOutput(*view);   //驱动启动前预填, 建立可控的起始播放延迟
    }

    //===== 启动引擎线程 =====
    if (this->hThread == INVALID_HANDLE_VALUE)
    {
        auto th = _beginthreadex(nullptr, 0, &ASIODriver::threadProc, this, 0, nullptr);
        if (th == 0)
        {
            this->processFlag.store(false);
            return exp_ns::unexpected("创建引擎线程失败");
        }
        this->hThread = reinterpret_cast<HANDLE>(th);
        //引擎线程优先级: 高于普通, 保证及时处理; 不用TIME_CRITICAL, 避免与音频回调线程竞争饿死
        SetThreadPriority(this->hThread, THREAD_PRIORITY_HIGHEST);
    }

    //===== 启动底层驱动(回调开始后, 引擎通过通知门限被唤醒) =====
    auto future = this->staWorker.submit(
        [this] {
            return this->pAsioDevice->start();
        });
    return future.get();
}

STAType ASIODriver::stop()
{
    //1. 停止引擎线程
    if (this->processFlag.exchange(false))
    {
        if (this->hNotify != INVALID_HANDLE_VALUE) SetEvent(this->hNotify);
        if (this->hExit != INVALID_HANDLE_VALUE) SetEvent(this->hExit);
        if (this->hThread != INVALID_HANDLE_VALUE)
        {
            WaitForSingleObject(this->hThread, INFINITE);
            CloseHandle(this->hThread);
            this->hThread = INVALID_HANDLE_VALUE;
        }
    }

    //2. 停止底层驱动
    if (this->pAsioDevice != nullptr)
    {
        auto future = this->staWorker.submit(
            [this] {
                return this->pAsioDevice->stop();
            });
        return future.get();
    }
    return {};
}

STAType ASIODriver::setChannelMask(unsigned inputMask, unsigned outputMask)
{
    return this->pAsioDevice->setChannelMask(inputMask, outputMask);
}

TResult<int> ASIODriver::getSampleRate()
{
    auto future = this->staWorker.submit(
        [this] {
            return this->pAsioDevice->getSampleRate();
        });
    auto result = future.get();
    if (!result)
    {
        return exp_ns::unexpected(result.error());
    }

    return this->pAsioDevice->sampleRate;
}

TResult<void> ASIODriver::setSampleRate(long sampleRate)
{
    auto future = this->staWorker.submit(
        [this, sampleRate] {
            return this->pAsioDevice->setSampleRate(sampleRate);
        });

    return future.get();
}

int ASIODriver::getCaptureCount()
{
    return this->pAsioDevice->inputChannels.size();
}

int ASIODriver::getRenderCount()
{
    return this->pAsioDevice->outputChannels.size();
}

ASIODriver::ASIODiagnostics ASIODriver::getDiagnostics() const
{
    ASIODiagnostics d;
    d.inputDroppedFrames = this->_inputDroppedFrames.load(std::memory_order_relaxed);
    d.outputFullFrames = this->_outputFullFrames.load(std::memory_order_relaxed);
    d.outputUnderrunFrames = this->pAsioDevice ? this->pAsioDevice->getOutputUnderrunFrames() : 0;
    d.processUs = this->_procUs.load(std::memory_order_relaxed);
    d.processCount = this->_procCount.load(std::memory_order_relaxed);
    return d;
}

std::string ASIODriver::getCaptureName(int channel)
{
    if (channel < 0 || channel >= (int)this->pAsioDevice->inputChannels.size())
    {
        return std::string();
    }

    return this->pAsioDevice->inputChannels[channel].name;
}

std::string ASIODriver::getRenderName(int channel)
{
    if (channel < 0 || channel >= (int)this->pAsioDevice->outputChannels.size())
    {
        return std::string();
    }

    return this->pAsioDevice->outputChannels[channel].name;
}

TResult<void> ASIODriver::Initialize(unsigned inputMask, unsigned outputMask)
{
    auto result = this->pAsioDevice->setChannelMask(inputMask, outputMask);
    if (!result)
    {
        return result;
    }
    result = this->createBuffer();
    return result;
}

TResult<void> ASIODriver::Release()
{
    auto result = this->stop();
    return result;
}

TResult<ASIODriver*> ASIODriver::createDriver(CLSID clsid, int notifyMills, int maxDelayMills, bool exclusiveMode)
{
    lock_guard<mutex> lg(gMTX);

    //第一个空偏移
    int offset = -1;
    for (int index = 0; index < (int)ASIOEntitys.size(); index++)
    {
        auto& entity = ASIOEntitys[index];
        auto& pASIODriver = entity.pASIODriver;
        if (pASIODriver != nullptr)
        {
            if (IsEqualCLSID(pASIODriver->asioID, clsid) == TRUE)
            {
                return exp_ns::unexpected("不允许重复创建驱动");
            }
        }
        else
        {
            if (offset == -1)
            {
                offset = index;
            }
        }
    }

    if (offset == -1)
    {
        return unexpected("can not support so much card"); //超过了最大支持的板卡数量
    }

    auto& entity = ASIOEntitys[offset];

    auto pASIODriver = std::make_unique<ASIODriver>(&entity.callback, clsid, notifyMills, maxDelayMills, exclusiveMode);

    auto result = pASIODriver->driverOpen();
    if (!result)
    {
        return exp_ns::unexpected(result.error());
    }

    entity.pASIODriver = std::move(pASIODriver);

    return entity.pASIODriver.get();
}

TResult<void> ASIODriver::releaseDriver(ASIODriver* driver)
{
    if (driver == nullptr)
    {
        return unexpected("不能为空");
    }

    lock_guard<mutex> lg(gMTX);
    for (auto& entity : ASIOEntitys)
    {
        auto& pASIODriver = entity.pASIODriver;
        if (pASIODriver != nullptr)
        {
            if (IsEqualCLSID(pASIODriver->asioID, driver->asioID) == TRUE)
            {
                pASIODriver.reset();//释放资源
                return {};
            }
        }
    }

    return unexpected("没有匹配项目");
}
