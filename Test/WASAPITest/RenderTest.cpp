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
    
    //WaveReader reader(R"(d:\wave\Sweep@48k_24bit_mono.wav)");
    auto readerResult = WaveReader::create(R"(d:\wave\Sweep@48k_24bit_mono.wav)");

    auto& reader = readerResult.value();
    // vector<char> buffer;
    // buffer.resize(4800);

    // reader.read(buffer.data(), 4800);



    // return 0;
    auto* render = WASAPIManager::createRender(std::move(RenderEndPoints.front()), reader->getWaveFormat());

    if(render == nullptr)
    {
        std::println("播放器初始化失败");
    }

    
    println("开始播放");
    STAType result = render->playAsync(reader.get());

    result = render->waitPlayDone();
    return 0;
}