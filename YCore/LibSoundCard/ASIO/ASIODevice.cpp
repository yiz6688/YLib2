#include"base_config.hpp"
/*
Asio驱动的具体实现

设计思想:
1、通道掩码(setChannelMask): 启动前传入掩码开启有效通道, 输入/输出各最多32个。
   掩码可以为0(仅输入或仅输出), 但不能同时为0; 默认 -1 全开。
2、延迟与缓冲参数:
   - 通知时长 notifyMills 构造传入(默认10ms, 有效范围10-50ms) -> notifyFrames(帧, 硬件缓冲整数倍)
   - 最大延迟 = 2*通知时长, 上限100ms, 实际硬件缓冲更大则以实际为准 -> maxDelayFrames
   - 以单次回调(bufferSize帧)为计数单元, 缓冲区数量对齐到2的幂次 -> bufferCount
     haBufferSize = bufferCount * bufferSize(单通道环形帧容量, 本身不要求2的幂次)
3、输入通道: 以通知缓冲作为门限, 环形区采用覆盖式采集, 消费不及时也一直采集保持数据对齐,
   引擎落后太多时丢弃旧数据只保留最新通知帧。
4、输出通道: 因混频与提前放入的数据, 需要精确的缓冲区, 在容量内设置与硬件缓冲对齐的
   精确门限(outputLimitFrames, 4舍5入到硬件缓冲整数倍), 引擎只填充到该门限为止,
   不可覆盖, 精确控制播放延迟。
5、底层回调: 输入采集填充到输入环形区, 从输出环形区取数, 达到通知门限后通知引擎处理。
*/
#include<cstring>
#include<stdexcept>
#include"./asiosdk/asiosys.h"
#include"./asiosdk/asio.h"
#include"./asiosdk/iasiodrv.h"

#include "ASIODevice.h"
#include<memory>
#include"../../BitConverter.h"
#include"../../NumUtils.h"

using namespace std;
using fmt_ns::println;
using fmt_ns::print;

//采样位深(字节)
static int _getByteDepth(SampleType type)
{
    switch (type)
    {
    case SampleType::IEEE64: return 8;
    case SampleType::IEEE32: return 4;
    case SampleType::INT32:  return 4;
    case SampleType::INT24:  return 3;
    case SampleType::INT16:  return 2;
    default:                 return 0;
    }
}

static SampleType _getSampleType(ASIOSampleType asioType)
{
    if (asioType == ASIOSTInt16LSB)
    {
        return SampleType::INT16;
    }
    else if (asioType == ASIOSTInt24LSB)
    {
        return SampleType::INT24;
    }
    else if (asioType == ASIOSTInt32LSB)
    {
        return SampleType::INT32;
    }
    else if (asioType == ASIOSTFloat32LSB)
    {
        return SampleType::IEEE32;
    }
    else
    {
        return SampleType::UNKNOWN;
    }
}

//针对每一个单独的板卡驱动, 该类只会实例化一次
//对象创建时, iasio实例同步创建, 整个生命周期中只会失效, 不会为null
ASIODevice::ASIODevice(ASIOCallbacks* _callbacks, CLSID clsid, int notifyMills, int maxDelayMills)
    : notifyMills(notifyMills), maxDelayMills(maxDelayMills),
    bufferReady(false), driverRuning(false), bufferSize(512), iasio(nullptr), driverName{},
    num_of_capture{0}, num_of_render{0}, bufferMinSize(0), bufferMaxSize(0),
    bufferPreferredSize(0), bufferGranularity(0), sampleType(SampleType::UNKNOWN), bitDepth(0),
    object(nullptr), callbacks(_callbacks), driverID(clsid)
{
    //通知时长有效范围: 10-50ms
    if (this->notifyMills < 10 || this->notifyMills > 50)
    {
        throw std::invalid_argument(fmt_ns::format("notifyMills 超出有效范围[10,50]ms: {}", this->notifyMills));
    }
    //最大延迟: 0=自动; 非0必须 >= 通知时长 且 <= 100ms
    if (this->maxDelayMills != 0)
    {
        if (this->maxDelayMills < this->notifyMills || this->maxDelayMills > 100)
        {
            throw std::invalid_argument(fmt_ns::format("maxDelayMills 无效: 应为0(自动)或[{},100]ms: {}",
                this->notifyMills, this->maxDelayMills));
        }
    }
}


//析构函数中进行驱动的释放以及引用的清空
ASIODevice::~ASIODevice()
{
    println("ASIODevice析构");

    (void)this->deviceRelease();
}


/**
 * 安全加载驱动
 * 1、如果驱动不存在, 直接进行加载
 * 2、如果驱动已存在, 检测是否失效。确认失效后释放旧资源后重新加载。
 */
exp_ns::expected<void, std::string> ASIODevice::loadInstance()
{
    exp_ns::expected<void, std::string> result;
    if (this->iasio != nullptr)
    {
        result = this->getSampleRate();
        if (!result)
        {
            result = this->deviceRelease(); //释放驱动
        }
    }

    this->bufferReady = false;

    HRESULT hResult = CoCreateInstance(this->driverID, 0, CLSCTX_INPROC_SERVER, this->driverID, (LPVOID*)(&this->iasio));
    if (hResult != S_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("CoCreateInstance Fail, result={}", hResult));
    }
    void* handle = nullptr;
    auto initResult = this->iasio->init(handle);
    if (initResult != ASIOTrue)  //驱动初始化
    {
        char err[256];
        this->iasio->getErrorMessage(err);
        this->iasio->Release();  //释放驱动
        this->iasio = nullptr;
        return exp_ns::unexpected(fmt_ns::format("asio init Fail,err={}", err));
    }

    return {};
}


/// <summary>
/// 使用48k采样率加载驱动程序,
/// 如果不支持48k采样率, 就使用默认采样率加载驱动。
/// 加载完毕后相关参数保存在成员变量中。
/// </summary>
exp_ns::expected<void, std::string> ASIODevice::deviceInit()
{
    ASIOError error;
    string errInfo = "";

    auto result = this->loadInstance();
    if (!result)
    {
        return result;
    }

    //获取驱动名称
    this->iasio->getDriverName(this->driverName);

    if (this->supportSampleRate(48000))  //默认设置为48k
    {
        error = this->iasio->setSampleRate(48000);
        if (error != ASE_OK)
        {
            return exp_ns::unexpected(fmt_ns::format("setSampleRate Fail, code: {}", error));
        }
    }

    result = this->getHaParam();
    if (!result)
    {
        return result;
    }

    //获取输入输出数量
    error = this->iasio->getChannels(&this->num_of_capture, &this->num_of_render);
    if (error != ASE_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("getChannels Fail, code: {}", error));
    }
    if (this->num_of_capture > 32 || this->num_of_render > 32)
    {
        return exp_ns::unexpected(fmt_ns::format("通道数超出32: input={}, output={}, 不支持", this->num_of_capture, this->num_of_render));
    }

    this->inputChannels.clear();
    this->inputChannels.reserve(this->num_of_capture);
    this->outputChannels.clear();
    this->outputChannels.reserve(this->num_of_render);

    ASIOChannelInfo value;
    ASIOSampleType type = -1;

    for (int i = 0; i < this->num_of_capture; i++)
    {
        value.channel = i;
        value.isInput = ASIOTrue;
        error = this->iasio->getChannelInfo(&value);
        if (error != ASE_OK)
        {
            return exp_ns::unexpected(fmt_ns::format("getChannelInfo Fail, inputchannel={} code: {}", i, error));
        }
        if (type == -1)
        {
            type = value.type;
            this->sampleType = _getSampleType(type);
            this->bitDepth = _getByteDepth(this->sampleType);
            if (this->sampleType == SampleType::UNKNOWN || this->bitDepth <= 0)
            {
                return exp_ns::unexpected(fmt_ns::format("unsupport SampleType: {}", type));
            }
        }

        if (value.type != type)
        {
            return exp_ns::unexpected(fmt_ns::format("inputChannel:{}, sampleType error", i));
        }

        this->inputChannels.push_back(value);
    }
    for (int i = 0; i < this->num_of_render; i++)
    {
        value.channel = i;
        value.isInput = ASIOFalse;
        error = this->iasio->getChannelInfo(&value);
        if (error != ASE_OK)
        {
            return exp_ns::unexpected(fmt_ns::format("getChannelInfo Fail, outputchannel={} code: {}", i, error));
        }
        if (value.type != type)
        {
            return exp_ns::unexpected(fmt_ns::format("outputChannel:{}, sampleType error", i));
        }
        this->outputChannels.push_back(value);
    }

    return {};
}

exp_ns::expected<void, std::string> ASIODevice::deviceRelease()
{
    if (this->iasio != nullptr)
    {
        (void)this->stop(); // 调用一次停止

        if (this->bufferReady)
        {
            (void)this->iasio->disposeBuffers();  //释放缓冲区
            this->bufferReady = false;
        }
        (void)this->iasio->Release();
        this->iasio = nullptr;
    }
    this->_iTotalBuffers.clear();
    this->_oTotalBuffers.clear();
    this->_cbBuffers.clear();
    this->driverRuning = false;
    return {};
}


/**
 * 设置通道掩码:
 * 1、输入/输出各最多32个通道, 通道bit位为1表示启用
 * 2、掩码可以为0(部分场景只使用输入或只使用输出), 但两个不能同时为0
 * 3、默认 -1(0xFFFFFFFF) 即全部通道开启, 未调用本方法时走默认全开
 * 4、缓冲区创建后不允许修改
 *
 * 说明: 系统在工厂中创建, 只有能打开的驱动才有效; ASIO打开后基础参数(通道数/位深/采样率)
 * 已经获取且不会变化, 因此这里直接基于 num_of_capture/num_of_render 校验&约束掩码。
 */
TResult<void> ASIODevice::setChannelMask(unsigned inputMask, unsigned outputMask)
{
    if (this->bufferReady)
    {
        return exp_ns::unexpected("缓冲区已创建, 通道掩码不允许修改");
    }
    //设备未初始化(驱动打开失败或不存在)时, 设置掩码无意义
    if (this->num_of_capture == 0 && this->num_of_render == 0)
    {
        return exp_ns::unexpected("设备未初始化, 无法设置通道掩码");
    }
    if (inputMask == 0 && outputMask == 0)
    {
        return exp_ns::unexpected("输入和输出通道掩码不能同时为0");
    }

    unsigned inputValid = (this->num_of_capture >= 32) ? 0xFFFFFFFFu : ((1u << this->num_of_capture) - 1u);
    unsigned outputValid = (this->num_of_render >= 32) ? 0xFFFFFFFFu : ((1u << this->num_of_render) - 1u);

    //-1(全开) 归一化为有效通道全掩码, 避免无效高位导致校验失败
    if (inputMask == 0xFFFFFFFFu)
    {
        inputMask = inputValid;
    }
    if (outputMask == 0xFFFFFFFFu)
    {
        outputMask = outputValid;
    }

    if ((inputMask & inputValid) != inputMask)
    {
        return exp_ns::unexpected("inputMask 越界或包含无效通道");
    }
    if ((outputMask & outputValid) != outputMask)
    {
        return exp_ns::unexpected("outputMask 越界或包含无效通道");
    }

    this->inputMask = inputMask;
    this->outputMask = outputMask;
    return {};
}


/**
 * 驱动创建缓冲区
 * 1、计算延迟相关参数:
 *    notifyFrames   = 通知门限(帧), 按 notifyMills 对齐并向上取整到硬件缓冲的整数倍
 *    maxDelayFrames = 最大延迟(帧)
 *    haBufferSize   = 单通道环形容量(帧), 缓冲区数量按硬件缓冲帧数对齐到2的幂次
 *    outputLimitFrames = 输出精确门限(帧), 与硬件缓冲对齐, 精确控制播放延迟
 * 2、为激活通道开辟软件环形缓冲(输入覆盖式 / 输出精确门限式)
 * 3、创建ASIO硬件双缓冲
 */
TResult<void> ASIODevice::createBuffer()
{
    if (this->bufferReady)
    {
        return exp_ns::unexpected("bufferReady is true");
    }
    if (this->iasio == nullptr)
    {
        return exp_ns::unexpected("驱动未加载");
    }

    auto result = this->getHaParam();
    if (!result)
    {
        return result;
    }

    int byteSize = this->bitDepth;

    //----- 1. 延迟与缓冲参数计算 -----
    //最大延迟: 0=自动(2*通知时长, 上限100ms); 非0=显式覆盖(构造时已校验)
    long maxDelayMs = (long)this->maxDelayMills;
    if (maxDelayMs == 0)
    {
        maxDelayMs = (long)this->notifyMills * 2;
        if (maxDelayMs > 100)
        {
            maxDelayMs = 100;
        }
    }

    //通知门限(帧), 向上对齐到硬件缓冲整数倍, 保证回调通知与处理都是整块硬件缓冲
    this->notifyFrames = (int)((long long)this->notifyMills * this->sampleRate / 1000);
    this->notifyFrames = ((this->notifyFrames + this->bufferSize - 1) / this->bufferSize) * (int)this->bufferSize;
    if (this->notifyFrames < this->bufferSize)
    {
        this->notifyFrames = (int)this->bufferSize;
    }

    //最大延迟(帧); 实际硬件缓冲更大则以实际为准(至少覆盖一个通知周期/硬件缓冲)
    this->maxDelayFrames = (int)((long long)maxDelayMs * this->sampleRate / 1000);
    if (this->maxDelayFrames < this->notifyFrames)
    {
        this->maxDelayFrames = this->notifyFrames;
    }

    //缓冲区数量: 以单次回调(bufferSize帧)为计数单元, 对齐到2的幂次(至少双缓冲)
    //haBufferSize = 缓冲区数量 * 单次回调帧数, 本身不要求2的幂次(掩码按缓冲区数量取模)
    int bufferCount = NumUtils::nextpow2((unsigned)((this->maxDelayFrames + this->bufferSize - 1) / this->bufferSize));
    if (bufferCount < 2)
    {
        bufferCount = 2;
    }
    this->bufferCount = bufferCount;
    this->haBufferSize = bufferCount * (int)this->bufferSize;

    //输出精确门限: 与硬件缓冲对齐, 4舍5入到最近的硬件缓冲整数倍
    //(2的幂次容量可能偏大, 该门限用于更准确控制播放延迟)
    this->outputLimitFrames = ((this->maxDelayFrames + (int)this->bufferSize / 2) / (int)this->bufferSize) * (int)this->bufferSize;
    if (this->outputLimitFrames > this->haBufferSize)
    {
        this->outputLimitFrames = this->haBufferSize;
    }
    if (this->outputLimitFrames < this->notifyFrames)
    {
        this->outputLimitFrames = this->notifyFrames;
    }

    println("notifyMills={}, maxDelayMills={}, sampleRate={}, bufferSize={}, notifyFrames={}, maxDelayFrames={}, bufferCount={}, haBufferSize={}, outputLimitFrames={}",
        this->notifyMills, this->maxDelayMills, this->sampleRate, this->bufferSize,
        this->notifyFrames, this->maxDelayFrames, this->bufferCount, this->haBufferSize, this->outputLimitFrames);

    //----- 2. 通道掩码: 默认 -1(全开), 归一化到有效通道范围 -----
    //(setChannelMask 已做校验, 未设置时 inputMask/outputMask 为 -1 全开)
    unsigned inputValid = (this->num_of_capture >= 32) ? 0xFFFFFFFFu : ((1u << this->num_of_capture) - 1u);
    unsigned outputValid = (this->num_of_render >= 32) ? 0xFFFFFFFFu : ((1u << this->num_of_render) - 1u);
    this->inputMask &= inputValid;
    this->outputMask &= outputValid;

    auto inputs = BitConverter::getBitIndex(this->inputMask);
    auto outputs = BitConverter::getBitIndex(this->outputMask);

    this->_iActiveNum = (int)inputs.size();
    this->_oActiveNum = (int)outputs.size();
    this->totalActiveNum = this->_iActiveNum + this->_oActiveNum;

    if (this->totalActiveNum == 0)
    {
        return exp_ns::unexpected("没有激活任何有效通道");
    }

    this->deviceFrameSize = (int)this->bufferSize;
    this->deviceByteSize = (int)this->bufferSize * byteSize;
    this->perChBufferByteSize = this->haBufferSize * byteSize;

    //----- 3. 软件环形缓冲(按通道连续排布) -----
    this->_iTotalBuffers.resize((size_t)this->_iActiveNum * this->perChBufferByteSize);
    this->_oTotalBuffers.resize((size_t)this->_oActiveNum * this->perChBufferByteSize);

    this->_cbBuffers.clear();
    this->_cbBuffers.reserve(this->totalActiveNum);
    for (int i = 0; i < this->_iActiveNum; i++)
    {
        int channel = inputs[i];
        char* buf = this->_iTotalBuffers.data() + (size_t)i * this->perChBufferByteSize;
        this->_cbBuffers.emplace_back(channel, buf, ASIOTrue);
    }
    for (int i = 0; i < this->_oActiveNum; i++)
    {
        int channel = outputs[i];
        char* buf = this->_oTotalBuffers.data() + (size_t)i * this->perChBufferByteSize;
        this->_cbBuffers.emplace_back(channel, buf, ASIOFalse);
    }

    //----- 4. 硬件双缓冲 -----
    vector<ASIOBufferInfo> bufferInfos(this->totalActiveNum);
    for (int i = 0; i < this->_iActiveNum; i++)
    {
        bufferInfos[i] = { ASIOTrue, (long)inputs[i], { nullptr, nullptr } };
    }
    for (int i = 0; i < this->_oActiveNum; i++)
    {
        bufferInfos[this->_iActiveNum + i] = { ASIOFalse, (long)outputs[i], { nullptr, nullptr } };
    }

    println("开始createbuffer");
    ASIOError error = this->iasio->createBuffers(bufferInfos.data(), this->totalActiveNum,
        this->bufferSize, this->callbacks);
    if (error != ASE_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("createBuffers Fail, code: {}", error));
    }
    println("createbuffer 成功");

    for (int i = 0; i < this->totalActiveNum; i++)
    {
        this->_cbBuffers[i].buffers[0] = bufferInfos[i].buffers[0];
        this->_cbBuffers[i].buffers[1] = bufferInfos[i].buffers[1];
    }

    //计数器复位
    this->_captureCounter.store(0, std::memory_order_release);
    this->_renderReadPos.store(0, std::memory_order_release);
    this->_renderWritePos.store(0, std::memory_order_release);

    this->bufferReady = true;

    return {};
}


/// <summary>
/// 打开驱动, 如驱动已打开且资源已创建就不重复执行, 否则重新构建资源。
/// </summary>
exp_ns::expected<void, std::string> ASIODevice::driverOpen(int _sampleRate)
{
    //需要加载驱动的标志位
    bool flag = false;
    auto result = this->getSampleRate(); //检查驱动存活性
    if (result)
    {
        if (this->bufferReady)
        {
            if (this->sampleRate == _sampleRate) //采样率相同且驱动已创建, 不需要额外处理直接返回即可。
            {
                return {};
            }
            else
            {
                if (this->driverRuning)
                {
                    return exp_ns::unexpected("驱动运行中,不允许以不同的采样率打开");
                }
                else
                {
                    flag = true;
                }
            }
        }
        else
        {
            flag = true;
        }
    }
    (void)flag;

    //重新加载驱动,然后创建缓冲区
    result = this->loadInstance();
    result = this->createBuffer();

    return {};
}


/**
 * 底层驱动线程回调:
 * 输入通道: 硬件采集数据填充到输入环形区(覆盖式), 计数单元为单次回调(bufferSize帧)
 * 输出通道: 从输出环形区取数据填充硬件缓冲(不可覆盖), 数据不足时补零(欠载静音), 读位置不前进
 * 环形区按缓冲区槽组织: 每个槽 = 一次回调(deviceByteSize字节), 单次回调整块拷贝无需回绕
 */
void ASIODevice::bufferProcess(long doubleBufferIndex)
{
    int bytes = this->deviceByteSize;   //单次回调字节数

    //----- 输入: 覆盖式写入环形区 -----
    auto wpos = this->_captureCounter.load(std::memory_order_relaxed);   //回调计数
    auto slot = wpos & (this->bufferCount - 1);                          //缓冲区槽索引(2的幂次掩码)
    auto byteIndex = (size_t)slot * bytes;

    for (int i = 0; i < this->_iActiveNum; i++)
    {
        auto& input = this->_cbBuffers[i];
        const char* src = static_cast<const char*>(input.buffers[doubleBufferIndex]);
        std::memcpy(input._buf + byteIndex, src, bytes);
    }
    this->_captureCounter.fetch_add(1, std::memory_order_release);   //计数单元=单次回调

    //----- 输出: 从环形区取数(精确门限约束下不可覆盖) -----
    auto rpos = this->_renderReadPos.load(std::memory_order_relaxed);    //回调计数
    auto wpos2 = this->_renderWritePos.load(std::memory_order_acquire);
    unsigned readable = wpos2 - rpos;   //引擎已写入且未读出的回调数
    if (readable >= 1)
    {
        for (int i = 0; i < this->_oActiveNum; i++)
        {
            auto& output = this->_cbBuffers[this->_iActiveNum + i];
            char* dest = static_cast<char*>(output.buffers[doubleBufferIndex]);
            auto slot2 = rpos & (this->bufferCount - 1);
            std::memcpy(dest, output._buf + (size_t)slot2 * bytes, bytes);
        }
        this->_renderReadPos.fetch_add(1, std::memory_order_release);
    }
    else
    {
        //欠载: 全部输出通道播放静音, 读位置不前进, 等待引擎补数据(保留原生处理)
        for (int i = 0; i < this->_oActiveNum; i++)
        {
            auto& output = this->_cbBuffers[this->_iActiveNum + i];
            char* dest = static_cast<char*>(output.buffers[doubleBufferIndex]);
            std::fill_n(dest, bytes, 0);
        }
        this->_outputUnderrunFrames.fetch_add((long long)this->deviceFrameSize, std::memory_order_relaxed);
    }
}


/**
 * 复位流计数器, 重启设备时保证输入/输出位置重新对齐
 */
void ASIODevice::resetCounters()
{
    this->_captureCounter.store(0, std::memory_order_release);
    this->_renderReadPos.store(0, std::memory_order_release);
    this->_renderWritePos.store(0, std::memory_order_release);
}


TResult<void> ASIODevice::getSampleRate()
{
    ASIOSampleRate _sampleRate;
    auto error = iasio->getSampleRate(&_sampleRate);
    if (error != ASE_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("getSampleRate Fail, code: {}", error));
    }
    this->sampleRate = _sampleRate;

    return {};
}

TResult<void> ASIODevice::supportSampleRate(long value)
{
    auto error = this->iasio->canSampleRate(value);
    if (error != ASE_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("supportSampleRate Fail, code: {}", error));
    }
    return {};
}


/**
 * 设置采样率, 运行中不允许设置采样率
 */
TResult<void> ASIODevice::setSampleRate(long value)
{
    if (this->driverRuning)
    {
        if (value == this->sampleRate)
        {
            return {};
        }
        else
        {
            return exp_ns::unexpected("驱动运行中，不允许设置采样率");
        }
    }
    else
    {
        auto error = this->iasio->setSampleRate(value);
        if (error != ASE_OK)
        {
            return exp_ns::unexpected(fmt_ns::format("setSampleRate Fail, code: {}", error));
        }

        this->sampleRate = value;
    }

    return {};
}


/**
 * 调用驱动启动录音
 */
TResult<void> ASIODevice::start()
{
    if (this->driverRuning == false)
    {
        auto error = this->iasio->start();
        if (error == ASE_OK)
        {
            this->driverRuning = true;
            return {};
        }
        else
        {
            this->driverRuning = false;
            return exp_ns::unexpected(fmt_ns::format("start Fail, code: {}", error));
        }
    }
    else
    {
        return {};
    }
}

/**
 * 调用驱动停止录音
 */
TResult<void> ASIODevice::stop()
{
    if (this->driverRuning == false)
    {
        return {};
    }

    auto error = this->iasio->stop();
    this->driverRuning = false;
    if (error != ASE_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("stop Fail, code: {}", error));
    }

    return {};
}

TResult<void> ASIODevice::getHaParam()
{
    //获取缓冲区, granularity=-1 的话 缓冲区就是2的n次方
    auto error = this->iasio->getBufferSize(&this->bufferMinSize, &this->bufferMaxSize,
        &this->bufferPreferredSize, &this->bufferGranularity);
    if (error != ASE_OK)
    {
        return exp_ns::unexpected(fmt_ns::format("getBufferSize Fail, code: {}", error));
    }
    this->bufferSize = this->bufferPreferredSize;

    auto result = this->getSampleRate();
    if (!result)
    {
        return result;
    }
    return TResult<void>();
}
