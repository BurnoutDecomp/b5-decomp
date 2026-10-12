// Complete production graph/callback bodies with report and draw spies.
// This checks original layout/accumulation behavior, not native GPU rendering.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>

namespace CgsDev::Assert
{
    int BeginAssert() { return 0; }
    void* EndAssert() { return nullptr; }
    int FireAssert(const char*, const char*, int) { return 0; }
}
namespace CgsDev::Log { StrStreamBase* gpDebugPrint = nullptr; }
namespace CgsDev
{
    using RGBA = u32;
    struct Box { Vector2 position, size; RGBA colour; };
    struct Text { std::string text; Vector2 position; float size; RGBA colour; };
    struct Debug2DImmediateRender
    {
        std::vector<Box> boxes;
        std::vector<Text> texts;
        unsigned frames = 0;
        float frame[6] = {};
        void DrawBox(const Vector2& position, const Vector2& size, RGBA colour)
        { boxes.push_back({position, size, colour}); }
        void DrawText(const char* text, const Vector2& position, float size, RGBA colour, bool)
        { texts.push_back({text, position, size, colour}); }
        void DrawFrame(float x0, float y0, float x1, float y1, RGBA colour, float border)
        { ++frames; frame[0]=x0; frame[1]=y0; frame[2]=x1; frame[3]=y1; frame[4]=float(colour); frame[5]=border; }
        void Clear() { boxes.clear(); texts.clear(); frames=0; }
    };
    class DebugComponentPerfMonGpu
    {
    public:
        struct RenderCallbackParameter
        {
            DebugComponentPerfMonGpu* mpThis;
            Debug2DImmediateRender* mpRender;
            f32 mfDrawX, mfDrawY, mfTotalGpu;
        };
        f32 mfMaxGpu = 0;
        void RenderPerformanceGraph(Debug2DImmediateRender*);
        static void RenderGraphCallback(const PerfMonGpuMonitorData&, void*);
    };
    static std::vector<PerfMonGpuMonitorData> data;
    static bool reportedTotal = true;
    void PerfMonGpu::ReportMonitors(FPerfMonGpuReportCallback* callback, void* context, bool total)
    { reportedTotal = total; for (const auto& item : data) callback(item, context); }
}
#include "pc_gpu_monitor_graph.inc"
static unsigned checks, failures;
static void Check(bool passed, const char* label)
{ ++checks; if (!passed) { ++failures; std::printf("FAIL %s\n",label); } }
static bool Near(float a, float b) { return std::fabs(a-b) < .0001f; }
int main()
{
    using namespace CgsDev;
    DebugComponentPerfMonGpu overlay;
    Debug2DImmediateRender draw;
    const rw::RGBA white(255,255,255,255);
    data = {{"First",20,white,false},{"Second",30,white,true}};
    overlay.RenderPerformanceGraph(&draw);
    Check(overlay.mfMaxGpu == 50, "the callback total raises the reference-bar maximum");
    Check(!reportedTotal, "graph requests monitor segments without total rows");
    Check(draw.frames == 1 && draw.frame[0] == 32 && draw.frame[1] == 416
        && draw.frame[2] == 608 && draw.frame[3] == 420 && draw.frame[5] == 2,
        "original graph frame origin, extent and border are preserved");
    bool grid = draw.boxes.size() == 8;
    for (unsigned i=0;i<5 && grid;++i) grid &= draw.boxes[i].position.x == 27+144*float(i)
        && draw.boxes[i].position.y == 420 && draw.boxes[i].size.x == 10
        && draw.boxes[i].size.y == 3 && draw.boxes[i].colour == 0xFFFFFFFF;
    Check(grid, "all five original grid bars have the attested size and positions");
    bool labels = draw.texts.size() == 5;
    const char* names[] = {"0%","50%","100%","150%","200%"};
    const float x[] = {32,168,304,448,592};
    for (unsigned i=0;i<5 && labels;++i) labels &= draw.texts[i].text == names[i]
        && draw.texts[i].position.x == x[i] && draw.texts[i].position.y == 424
        && draw.texts[i].size == 15;
    Check(labels, "all original percent labels and their nonuniform positions are preserved");
    Check(draw.boxes.size() == 8 && Near(draw.boxes[6].size.x,57.600002f)
        && Near(draw.boxes[7].position.x,89.600006f) && Near(draw.boxes[7].size.x,86.400009f)
        && draw.boxes[6].size.y == 4 && draw.boxes[7].size.y == 4,
        "actual graph callbacks accumulate adjacent percentage-scaled segments");
    Check(draw.boxes.size() == 8 && draw.boxes[7].colour == 0xFFFFFFFF,
        "graph retains original opaque monitor colour even above budget");
    data = {{"Smaller",15,white,false}}; draw.Clear(); overlay.RenderPerformanceGraph(&draw);
    Check(overlay.mfMaxGpu == 50, "a smaller frame does not shrink the recorded maximum");
    Check(draw.boxes.size() == 7 && Near(draw.boxes[5].size.x,
        std::fma(50.0f,2.8799999f,32.0f)-32.0f),
        "the next frame draws the prior maximum with the distinct reference scale");
    data = {{"Large",250,white,false}}; draw.Clear(); overlay.RenderPerformanceGraph(&draw);
    Check(overlay.mfMaxGpu == 200, "a raised maximum is capped at the original 200 percent");
    data.clear(); draw.Clear(); overlay.RenderPerformanceGraph(&draw);
    Check(overlay.mfMaxGpu == 200 && draw.boxes.size() == 6,
        "an empty report preserves the high-water reference without inventing segments");
    overlay.mfMaxGpu = -1; draw.Clear(); overlay.RenderPerformanceGraph(&draw);
    Check(overlay.mfMaxGpu == 0, "the zero callback total raises a negative initial maximum");
    std::printf("PCGpuMonitorGraph: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
