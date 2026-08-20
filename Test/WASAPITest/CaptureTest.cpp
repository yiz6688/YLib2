#include<print>
#include"../../YCore/LibSoundCard/WASAPI/WASAPIManager.h"
using namespace std;

int main()
{
    
    println("Hello WASAPI Render Test");
    auto CaptureEndPoints = WASAPIManager::getEndPoints(eCapture);
    for (auto& ep : CaptureEndPoints)
    {
        println("Render frindlyName:{}, id{}", ep.frindlyName, ep.id);
    }

    //WaveWriter writer(WaveFormat(48000, 16, 1), R"(D:\wave\out2\123.wav)");
    auto writerResult = WaveWriter::create(WaveFormat(48000, 16, 1), R"(D:\wave\out2\123.wav)");
    auto& writer = writerResult.value();

    auto* capture = WASAPIManager::createCapture(std::move(CaptureEndPoints.front()), writer->getWaveFormat());

    if(capture == nullptr)
    {
        std::println("播放器初始化失败");
    }

    
    println("开始播放");
    STAType result = capture->captureAsync(writer.get(), 3000);
    if(!result)
    {
        println("{}", result.error());
    }

    result = capture->waitCaptureDone();

    return 0;
}