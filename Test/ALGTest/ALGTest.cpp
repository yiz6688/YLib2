#include<print>
#include"../../YCore/LibAlgorithm/findDelay.h"
using namespace std;


int main()
{
    //println("algTest");

    std::vector<double> x{1.0, 2.0, 3.0, 4.0};
    std::vector<double> y{1.0, 2.0, 3.0};
    FindDelay fd;

    auto corr = fd.correlate(x, y);

    println("长度:{}", corr.size());
    for(auto x : corr)
    {
        print("{:.3f} ", x);
    }



}