#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <limits>
#define CGS_ASSERT(ok, message) do { if (!(ok)) std::abort(); } while (0)
static int checks,failures;
static void Check(bool value,const char* message){++checks;if(!value){++failures;std::printf("FAIL %s\n",message);}}
namespace CgsGraphics {
void DispatchObjectContext::ResetShadowing(){std::memset(mapConstantData,0,sizeof(mapConstantData));}
}
// Frame allocation and interpreter execution are observable boundaries. The
// threshold/default setup and mesh admission expression come from production.
struct FrameBoundary {int resets=0;void Reset(){++resets;}};
struct InterpreterBoundary {
    FrameBoundary* frame=nullptr;float time=-1;
    void SetSingleBufferedDispatchFrame(FrameBoundary* f){frame=f;}
    void SetTime(float t){time=t;}
};
struct PreZRenderer {
    FrameBoundary mSingleBufferedDispatchFrame,mDoubleBufferedDispatchFrame;
    InterpreterBoundary* mpInterpreter=nullptr;
    bool mbRenderPreZ=true,mbRenderPreZAlpha=false,mbPreZNearOnly=false;
    float mfPreZDistanceThreshold=0;
    int converted=0,sorted=0;
    void* mpMeshProducerInterpreterPC=nullptr;
    struct Prepared { bool mbReady=false; } maPreparedMeshFramesPC[2];
    unsigned muMeshReadFramePC=0;
    void InitializeDispatchContextPC(CgsGraphics::DispatchObjectContext*) const;
    void Defaults();
    bool BuildDispatchLists(CgsGraphics::DispatchObjectContext*);
    void ConvertObjectsToMeshes(FrameBoundary*,FrameBoundary*,InterpreterBoundary*,CgsGraphics::DispatchObjectContext*){++converted;}
    void SortDispatchLists(FrameBoundary*){++sorted;}
};
#include "pc_prez_range.inc"
int main(){
    using namespace CgsGraphics;
    PreZRenderer renderer;renderer.Defaults();InterpreterBoundary interpreter;renderer.mpInterpreter=&interpreter;
    DispatchObjectContext context;
    Check(renderer.mbPreZNearOnly&&renderer.mfPreZDistanceThreshold==200.0f,
        "original constructor selects near pre-Z with raw float8203BA4C=200");
    std::memset(&context,0xA5,sizeof(context));
    const bool built=renderer.BuildDispatchLists(&context);
    bool lanes=true;for(float value:context.mvPreZDistanceThreshold)lanes=lanes&&value==200.0f;
    Check(built&&lanes,"render copies the linear distance into all four context lanes without squaring");
    const float below=std::nextafter(200.0f,0.0f),above=std::nextafter(200.0f,300.0f);
    Check(Admitted(&context,below)&&Admitted(&context,200.0f)&&!Admitted(&context,above),
        "actual mesh predicate keeps the boundary and excludes the first farther representable depth");
    Check(!Admitted(&context,500.0f)&&!Admitted(&context,30000.0f),
        "distant colour-pass meshes no longer receive the extra depth-only draw");
    renderer.mbPreZNearOnly=false;renderer.BuildDispatchLists(&context);
    Check(context.mvPreZDistanceThreshold[0]==200.0f&&!Admitted(&context,201.0f),
        "the separate occlusion-global switch does not override the object context threshold");
    renderer.mfPreZDistanceThreshold=80.5f;renderer.mbRenderPreZ=false;renderer.mbRenderPreZAlpha=true;
    renderer.BuildDispatchLists(&context);
    Check(context.mvPreZDistanceThreshold[0]==80.5f&&!context.mbPreZEnabled&&context.mbPreZAlphaEnabled
        &&interpreter.frame==&renderer.mSingleBufferedDispatchFrame&&interpreter.time==0
        &&renderer.converted==3&&renderer.sorted==3,
        "nonintegral tuning, pass switches and frame/interpreter sequencing remain intact");
    renderer.mpInterpreter=nullptr;
    Check(!renderer.BuildDispatchLists(&context)&&renderer.converted==3,
        "an unavailable interpreter does not generate or sort mesh work");
    std::printf("PCPreZRange: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
