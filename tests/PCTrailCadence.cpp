#include <cmath>
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameSource/Effects/Particles/Native/TrailFramePC.h"
static u32 suChecks,suFailures;
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} }
namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++suFailures;return 0;}
void* EndAssert(){return nullptr;}
} }
namespace BrnParticle { namespace Native {
void TrailRenderer::Construct(CgsMemory::HeapMalloc*,BrnGraphics::Im3dSkidsRenderer* renderer){mpRenderer=renderer;}
void TrailRenderer::Update(f32 now,Matrix44::InParam matrix){mfCurrentTime=now;mViewProjectionMatrix=matrix;}
} }
namespace BrnParticle {
// This fixture seats the original EndOfFrame and actual command publication
// bodies on real trail objects. Both simulation and zero-step command frames
// publish, as they do on the uncapped host.
// Unrelated simple-particle publication is an inert boundary; draw is not run.
struct Publication {
    struct SimplePublication {void Publish(int) {}} mSimpleParticleFramePC;
    int maSimpleParticles=0;
    bool mbStalled=false;
    Native::TrailSystem mTrailSystem;
    Native::TrailFramePC mTrailFramePC;
    struct ParticleRenderData {
        f32 mfCurrentTimeStep=0,mfCurrentTime=0;
        u32 muCurrentFrame=0;
        CgsGraphics::Camera mCgsCamera;
    } mRenderData;
    u32 muTrailSystemUpdateFramePC=0;
    f32 mfTrailSystemTimeStepPC=0;
    void EndOfFrame(bool);
    void PublishRenderCommandsPC(const ParticleRenderData&);
    void PublishFrame() {
        ++mRenderData.muCurrentFrame;
        EndOfFrame(false);
        PublishRenderCommandsPC(mRenderData);
    }
};
}
#include "trail_cadence.inc"
static void Check(bool result,const char* name){++suChecks;if(!result){++suFailures;std::printf("FAIL %s\n",name);}}
int main()
{
    using namespace BrnParticle;
    using namespace Native;
    Publication frame;
    frame.mTrailSystem.Construct(nullptr,nullptr);
    Check(frame.mTrailSystem.Prepare(),"actual trail pool prepares");
    frame.mTrailSystem.SetReady();
    frame.mRenderData.mCgsCamera.mViewProjection.SetIdentity();
    TrailEmitterData wheel; wheel.Prepare();
    frame.mRenderData.mfCurrentTime=30.f;
    frame.mRenderData.mfCurrentTimeStep=1.f/60;
    frame.PublishFrame();
    // Two extra present-only frames between fixed60Hz wheel updates, as the
    // shipped165fps host does. The update frame's own sum is correctly zero.
    for (int step=0;step<12;++step) {
        const f32 now=30.f+(step+1)/60.f;
        frame.mTrailSystem.AddTrailSegment(&wheel,{float(step),0,0,0},{0,1,0,0},0,.7f,now);
        frame.mRenderData.mfCurrentTime=now;
        frame.mRenderData.mfCurrentTimeStep=1.f/60;
        frame.PublishFrame();
        frame.mRenderData.mfCurrentTimeStep=0;
        frame.PublishFrame(); frame.PublishFrame();
    }
    Check(frame.mTrailSystem.maActiveEmitters[0].GetSize()==1,"one continuous emitter survives render-only frames");
    Check(wheel.mpTrailEmitter && wheel.mpTrailEmitter->mn8NumSegments>=2,"real strip has at least two segments for rendering");
    Check(frame.mTrailFramePC.maCounts[0]==1 && frame.mTrailFramePC.maActive[0][0]->mn8NumSegments>=2,
          "published snapshot contains drawable geometry");
    Check(frame.mTrailSystem.mfCurrentTimeStep==1.f/60,"last simulation step remains the timeout interval");
    Check(frame.mRenderData.mfCurrentTimeStep==0,"render-only ParticleRenderData retains its original zero step");
    const auto* published = frame.mTrailFramePC.maCounts[0] > 0 ? frame.mTrailFramePC.maActive[0][0] : nullptr;
    const f32 laid=published ? published->mpCurrentSegments->ReadSegmentTime(0) : -1.f;
    Check(std::abs(laid-(30.f+1.f/60))<1.e-5f,"strip origin retains its original timestamp");
    const f32 lastAdded=wheel.mpTrailEmitter ? wheel.mpTrailEmitter->mrTimeLastSegmentAdded : -1.f;
    frame.mRenderData.mfCurrentTime=lastAdded+9.95f;
    frame.mRenderData.mfCurrentTimeStep=1.f/60;
    frame.PublishFrame();
    frame.mRenderData.mfCurrentTimeStep=0;
    const f32 heldTime=frame.mRenderData.mfCurrentTime;
    // More than ten seconds' worth of presents with no simulation progress:
    // retaining the timeout step must never turn these calls into clock ticks.
    for(int present=0;present<720;++present)
    {
        if (present % 120 == 0)
            frame.PublishFrame();
        else
            frame.EndOfFrame(false); // no command producer: keep the read bank
        frame.mTrailFramePC.Update(heldTime,frame.mRenderData.mCgsCamera.mViewProjection);
    }
    Check(frame.mTrailSystem.mfCurrentTime==heldTime,"render-only publication never adds retained step to live absolute clock");
    Check(frame.mTrailFramePC.mRenderer.mfCurrentTime==heldTime,"draw clock assigns absolute time instead of accumulating presents");
    Check(frame.mTrailSystem.maActiveEmitters[0].GetSize()==1,"original emitter remains alive just before its ten-second boundary");
    Check(frame.mTrailFramePC.maCounts[0]==1,"held render bank does not retire a live strip early");
    for (int step=0;step<610;++step){
        frame.mRenderData.mfCurrentTime+=1.f/60;
        frame.mRenderData.mfCurrentTimeStep=1.f/60;
        frame.PublishFrame();
        frame.mRenderData.mfCurrentTimeStep=0;
        frame.PublishFrame();
    }
    Check(frame.mTrailSystem.maActiveEmitters[0].GetSize()==0,"original ten-second expiry returns all inactive emitters");
    Check(wheel.mpTrailEmitter==nullptr,"original expiry detaches the wheel owner");
    Check(frame.mTrailSystem.mFreeEmitters.GetLength()==KN_TRAIL_EMITTER_POOL_SIZE,"entire original pool is reusable");
    std::printf("PCTrailCadence: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
