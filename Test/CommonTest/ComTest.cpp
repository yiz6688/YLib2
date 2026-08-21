#include<print>
#include<windows.h>
#include"../../YCore/WinUtils.h"
#include<cstddef>
#include<vector>
#include<iostream>
#include"myType.h"
#include"FileStream.h"
using namespace std;



void fileTest()
{
    



}






int main()
{
    //println("{}", "comm");

    wstring path = L"D:\\12/fdfdfd/34\\drerer";
    auto rxx = WinUtils::makeDirs(path);
    if(!rxx)
    {
        auto error = rxx.error();
        std::println("{}", error);
    }

}