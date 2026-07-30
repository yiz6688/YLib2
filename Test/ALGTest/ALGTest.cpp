#include<print>
#include"../../YCore/LibAlgorithm/findDelay.h"
#include"../../YCore/LibAlgorithm/SweepTest.h"
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

    fd.gcc_phat_delay(x, y);
}


void sweepTest()
{
    StepSweep step;

    step.GenerateSweepWave(10000, 20, 10, 10, Octave::OCT12, 1);
}


int main()
{
    //println("algTest");

    //sweepTest();
    corrTest2();

}