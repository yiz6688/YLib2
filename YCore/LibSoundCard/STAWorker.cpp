#include"STAWorker.h"
#include<Windows.h>
#include<print>
using namespace std;


STAWorker::STAWorker()
    : flag{true}
{
    //参数1 安全属性  参数2 复位方式 参数3 初始状态  参数4 名称
    //这个设置是初始无信号，自动重置
    this->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    this->fu = std::async(std::launch::async, &STAWorker::consumer, this);
}

STAWorker::~STAWorker()
{
    {
        lock_guard<mutex> lg(this->mtx);
        this->flag = false;
        SetEvent(this->hEvent); //发出信号
    }
    if(this->fu.valid())
    {
        this->fu.get();
    }
    CloseHandle(this->hEvent);
    this->hEvent = NULL;
    println("STAWorker析构完成");
}


long STAWorker::consumer()
{
    println("进入消费函数");
    
    auto hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    //返回S_OK 成功   S_FALSE 已经被初始化
    if (hr != S_OK && hr != S_FALSE)
    {
        //throw "asio driver initialize failed";
        return hr;
    }


    while(this->flag)
    {
        STATask task;
        {
            unique_lock<mutex> lk(this->mtx);
            if(flag == false)
            {
                break;
            }

            while(this->taskLsts.empty())
            {
                lk.unlock();
                //SetLastError(ERROR_SUCCESS);
                DWORD dwSignalledIndex;
                println("开始等待信号!!!!");
                HRESULT hr = CoWaitForMultipleHandles(
                            COWAIT_ALERTABLE,  // 允许在等待期间处理 APC（异步过程调用）
                            INFINITE,          // 超时时间：无限超时
                            1,                 // 句柄数量
                            &this->hEvent,     // 句柄数组
                            &dwSignalledIndex  // 输出：触发返回的句柄索引
                        );
                println("等待信号成功:{}", dwSignalledIndex);
                lk.lock();
                // 根据返回值处理结果
                //RPC_S_CALLPENDING 表示超时
                if (hr == S_OK) {
                    // 事件被成功触发
                    println("{}", dwSignalledIndex);
                    break;  
                } else {
                    // 等待超时
                    Sleep(10); //异常状态加个延时，防止疯狂来消息
                }
            }
            if(this->taskLsts.empty())
            {
                continue;
            }

            if(this->flag == false)
            {
                break;
            }

            task = std::move(this->taskLsts.front());
            this->taskLsts.pop();
        }
        task(); //执行任务。
        println("任务执行完成!!!");
    }

    CoUninitialize(); //回收com资源
    println("退出消费函数");
    return 0;
}

 STAFuture STAWorker::submit(const STAFunc& func)
{
    auto status = this->fu.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::ready)
    {
        std::promise<STAType> p;
        p.set_value(std::unexpected("111"));
        //线程结束了就返回
        return p.get_future();
    }

    STATask task(func);
    auto future = task.get_future();
    lock_guard<mutex> lg(this->mtx);
    this->taskLsts.push(std::move(task));
    SetEvent(this->hEvent); //发出信号
    return future;
}

STAFuture STAWorker::submit(STAFunc &&func)
{

    auto status = this->fu.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::ready)
    {
        std::promise<STAType> p;
        p.set_value(std::unexpected("111"));
        //线程结束了就返回
        return p.get_future();
    }

    STATask task(std::move(func));
    auto future = task.get_future();
    lock_guard<mutex> lg(this->mtx);
    this->taskLsts.push(std::move(task));
    SetEvent(this->hEvent); //发出信号
    return future;
}

#include<vector>
void STAWorker::UnitTest()
{
        STAWorker worker;

        vector<STAFuture> vec;
        auto fu1 = worker.submit([]{
            println("开始执行func1");
            this_thread::sleep_for(chrono::seconds(2));
            println("func1执行结束");
            return expected<void, string>();
        });

        vec.push_back(std::move(fu1));

        auto fu2 =  worker.submit([]{
            println("执行func2");
            this_thread::sleep_for(chrono::seconds(3));
            println("func2执行结束");
            return expected<void, string>();
        });

        vec.push_back(std::move(fu2));


        auto fu3 =  worker.submit([]->STAType{
            println("执行func3");
            this_thread::sleep_for(chrono::seconds(1));
            println("func3执行结束");
            return unexpected("模拟失败");
        });

        vec.push_back(std::move(fu3));

        for(auto& future : vec)
        {
            auto result = future.get();
            if(result)
            {
                println("执行通过");
            }else
            {
                println("{}", result.error());
            }
        }


        println("开始延时！！");
        this_thread::sleep_for(chrono::seconds(3));
        println("延时结束!!");
    }
