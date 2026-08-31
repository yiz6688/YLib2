#pragma once
#include<condition_variable>
#include<queue>
#include<future>
#include<expected>
#include<mutex>
#include<string>
#include<atomic>
#include<functional>
#include<windows.h>

using STAType = std::expected<void, std::string>;
using STAFunc = std::function<STAType()>;
using STATask = std::packaged_task<STAType()>;
using STAFuture = std::future<STAType>;

/**
 * STA类，生产消费都在一个单独的线程内完成。
 */
class STAWorker final
{


public:
    STAWorker();

    ~STAWorker();

protected:
    //消费者函数
    long consumer();

public:
    //提交符合STA要求的函数在STA线程中执行
     STAFuture submit(const STAFunc& func);
     STAFuture submit(STAFunc&& func);

public:
    static void UnitTest();

public:
    static unsigned __stdcall threadProc(void* param)
    {
        STAWorker* worker = reinterpret_cast<STAWorker*>(param);
        auto result = worker->consumer();
        return 0;
    }

private:
    std::mutex mtx;
    HANDLE hEvent;
    std::future<long> fu;
    std::queue<STATask> taskLsts;
    bool flag = true;
};