#include<print>
#include"../../YCore/LibSoundCard/WASAPI/WASAPIManager.h"
using namespace std;



int main()
{

    //STAWorker staWorker;

    //Sleep(1000);

    //auto fu = staWorker.submit([]{
    //    std::println("test1");
    //    return STAType();
    //});
    //auto r1 = fu.get();
    //Sleep(1000);
    // auto fu2 = staWorker.submit([]{
    //     std::println("test2");
    //     return STAType();
    // });

    // auto r2 = fu2.get();



    //return 0;
    println("Hello WASAPI Render Test");
    auto RenderEndPoints = WASAPIManager::getEndPoints(eRender);
    for (auto& ep : RenderEndPoints)
    {
        println("Render frindlyName:{}, id{}", ep.frindlyName, ep.id);
    }
    
    auto* render = WASAPIManager::createRender(std::move(RenderEndPoints.front()));

    if(render == nullptr)
    {
        std::println("播放器初始化失败");
    }

    WaveReader reader("D:\\wave\\Spk.wav");
    println("开始播放");
    //STAType result = render->playAsync(&reader);


    Sleep(5000);

    return 0;
}