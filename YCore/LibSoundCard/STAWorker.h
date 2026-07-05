#pragma once
#include<condition_variable>
#include<queue>
#include<future>
#include<expected>
#include<mutex>
#include<string>
#include<atomic>
#include<functional>


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

private:
    //消费者函数
    long consumer();

public:
    //提交符合STA要求的函数在STA线程中执行
     STAFuture submit(const STAFunc& func);
     STAFuture submit(STAFunc&& func);

public:
    static void UnitTest();


private:
    std::mutex mtx;
    std::condition_variable cv;
    std::future<long> fu;
    std::queue<STATask> taskLsts;
    bool flag = true;
};