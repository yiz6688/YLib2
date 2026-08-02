#include<print>
#include"../../YCore/LibAlgorithm/findDelay.h"
#include"../../YCore/LibAlgorithm/SweepTest.h"
#include"../../YCore/WaveReader.h"
#include"../../YCore/WinUtils.h"
#include"../../YCore/FIleUtils.h"
using namespace std;



void corrTest()
{
    std::vector<double> x{1.0, 2.0, 3.0, 4.0};
    std::vector<double> y{1.0, 2.0, 3.0};
    FindDelay fd;

    auto corr = fd.correlate(x, y);

    for(auto x : corr)
    {
        print("{:.3f} ", x);
    }
    println();
    corr = fd.correlate2(x, y);

    for(auto x : corr)
    {
        print("{:.3f} ", x);
    }

}


void corrTest2()
{
    std::vector<double> x{1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y{3.0, 4.0, 5.0};
    FindDelay fd;

    int delay = fd.gcc_phat_delay(x, y);
    std::println("{}", delay);
}


void sweepTest()
{
    StepSweep step;

    step.GenerateSweepWave(10000, 20, 10, 10, Octave::OCT12, 1);
}

void sweepTest2()
{
    StepSweep step;

    WaveReader reader("");
    int num = reader.getFrameCount();
    std::vector<double> data(num);
    auto res = reader.readSamples64(data.data(), num);
    if(!res)
    {
        std::println("读取文件失败:{}", res.error());
        return;
    }

    step.sweepTest(data, 10000, 20, 10, 10, Octave::OCT12, 0);

}





int main()
{
    //println("algTest");

    //sweepTest();
    //corrTest2();
    StepSweep step;
    std::vector<double> vec;
    step.sweepTest(vec, 10000, 20, 10, 10, Octave::OCT12, 0);
}