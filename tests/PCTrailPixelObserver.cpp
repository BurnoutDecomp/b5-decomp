#include <cstdio>
#include <cstring>
#include <limits>
#include "GameSource/Effects/Particles/Native/TrailFramePC.h"

static u32 suChecks, suFailures, suSteps, suKinds[8], suDraws;
static bool sbEnabled, sbPair = true;
static f32 sfObservedTime;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++suFailures; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace renderengine {
bool TrailPixelDiag_EnabledPC() { return sbEnabled; }
bool TrailPixelDiag_BeginPC(f32 now,Matrix44::InParam,const TrailPixelBatchPC*)
{ suKinds[suSteps++] = 0; sfObservedTime = now; return sbPair; }
void TrailPixelDiag_EndPC() { suKinds[suSteps++] = 4; }
}
namespace BrnParticle { namespace Native {
void TrailRenderer::BeginRender(renderengine::Texture*) { suKinds[suSteps++] = 1; }
void TrailRenderer::Render(TrailEmitter**,s32,TrailParams*,s8,f32) { suKinds[suSteps++] = 2; ++suDraws; }
void TrailRenderer::EndRender() { suKinds[suSteps++] = 3; }
} }
static void Check(bool value,const char* name)
{ ++suChecks; if (!value) { ++suFailures; std::printf("FAIL %s\n",name); } }

int main()
{
    using namespace renderengine;
    using namespace BrnParticle::Native;
    TrailPixelClockPC clock;
    Check(!clock.Take(35.9f,36.f),"absolute simulation start gates observation");
    Check(clock.Take(36.f,36.f),"first sample at requested simulation time");
    for (u32 present = 0; present < 720; ++present) clock.Take(36.f,36.f);
    Check(clock.attempts == 1,"720 extra renders cannot spend or advance simulation samples");
    Check(!clock.Take(36.99f,36.f),"sub-second simulation progress does not take a sample");
    Check(clock.Take(37.f,36.f),"next simulation second takes a sample");
    Check(!clock.Take(36.f,36.f),"regressed clock does not duplicate a sample");
    for (u32 second = 38; second < 60; ++second) clock.Take(float(second),36.f);
    Check(clock.attempts == 12 && clock.last == 47.f,"exact hard twelve-pair cap includes eleven elapsed seconds");
    Check(!clock.Take((std::numeric_limits<f32>::quiet_NaN)(),36.f),"invalid time cannot trigger a readback");

    TrailSegmentCollection segments = {};
    for (s32 index = 0; index < 2; ++index)
    {
        segments.WriteSegmentPosition({-.3f + index*.6f,0,.5f,0},index);
        segments.WriteSegmentTangent({0,1,0,0},index);
        segments.WriteSegmentStrength(.8f,index);
        segments.WriteSegmentTime(30.f+index*.1f,index);
    }
    TrailEmitter emitter = {}; emitter.mpCurrentSegments = &segments;
    emitter.mn8NumSegments = 2; emitter.mrTimeLastSegmentAdded = 30.1f;
    TrailEmitter* entries[] = {&emitter};
    TrailPixelBatchPC batches[4] = {{entries,1},{nullptr,0},{nullptr,0},{nullptr,0}};
    Matrix44 matrix; matrix.SetIdentity();
    TrailPixelTrackPC track;
    track.Observe(batches,matrix,128,72,32.f);
    Check(track.selected && track.type == 0 && track.segment == 1 && track.matches == 1,
        "select a real drawable quad from the published original list");
    const auto saved = track;
    const TrailPixelRectPC rect = track.Project(matrix,128,72);
    Check(rect.valid && rect.left == 42 && rect.right == 86 && rect.top == 29 && rect.bottom == 43,
        "native row-vector projection bounds both original 0.125m edges");
    track.Observe(batches,matrix,128,72,35.f);
    Check(track.matches == 1 && std::memcmp(track.ends,saved.ends,sizeof(track.ends)) == 0,
        "camera/time changes retain original laid coordinates and timestamps");
    emitter.mrTimeLastSegmentAdded = 31.f;
    track.Observe(batches,matrix,128,72,35.f);
    Check(track.lastAdded == 31.f,"matching surviving emitter supplies its actual last-added expiry clock");
    segments.WriteSegmentTime(35.f,0);
    track.Observe(batches,matrix,128,72,36.f);
    Check(track.matches == 0 && track.lastAdded == 31.f,"reused slot with different laid time cannot impersonate selected strip");
    segments.WriteSegmentTime(30.f,0); segments.WriteSegmentPosition({-.2f,0,.5f,0},0);
    track.Observe(batches,matrix,128,72,36.f);
    Check(track.matches == 0,"moved segment cannot impersonate fixed world segment");
    batches[0].count = 0;
    track.Observe(batches,matrix,128,72,42.f);
    Check(track.selected && track.active == 0 && track.matches == 0
        && std::memcmp(track.ends,saved.ends,sizeof(track.ends)) == 0,
        "expired original list retains fixed geometry for post-expiry pixel observation");
    matrix.wAxis.x = 8;
    Check(!track.Project(matrix,128,72).valid,"offscreen geometry cannot qualify zero coverage");
    matrix.wAxis.x = 0; matrix.wAxis.w = -1;
    Check(!track.Project(matrix,128,72).valid,"behind-camera geometry cannot qualify zero coverage");

    TrailPixelDeltaPC delta;
    delta.Add(0xff112233u,0xff102030u,50,35,rect);
    delta.Add(0xff000000u,0x00000000u,50,35,rect);
    delta.Add(0xff000000u,0xff000001u,0,0,rect);
    Check(delta.rgb == 2 && delta.alpha == 1,"native RGB and alpha are counted independently");
    Check(delta.roiRgb == 1 && delta.roiMagnitude == 6,"fixed-quad RGB coverage excludes outside pixels and alpha-only changes");
    const auto magnitude = delta.roiMagnitude;
    delta.Add(0xff112233u,0xff112233u,50,35,rect);
    Check(delta.roiMagnitude == magnitude,"zero native RGB difference contributes zero fade magnitude");

    TrailFramePC frame; frame.mbReady = true; frame.mRenderer.mfCurrentTime = 36.f;
    frame.maActive[0][0] = &emitter; frame.maCounts[0] = 1;
    renderengine::Texture* texture = reinterpret_cast<renderengine::Texture*>(1);
    frame.mTexture.mpResourceMemory = &texture;
    sbEnabled = false; suSteps = suDraws = 0; frame.Render(1.f);
    Check(suSteps == 3 && suKinds[0] == 1 && suKinds[1] == 2 && suKinds[2] == 3 && suDraws == 1,
        "default-off observer leaves exactly the original begin/type draw/end");
    sbEnabled = true; suSteps = suDraws = 0; frame.Render(1.f);
    Check(suSteps == 5 && suKinds[0] == 0 && suKinds[1] == 1 && suKinds[2] == 2
        && suKinds[3] == 3 && suKinds[4] == 4 && suDraws == 1,
        "readbacks bracket exactly the original accepted pass without extra draws");
    Check(sfObservedTime == 36.f && frame.mRenderer.mfCurrentTime == 36.f,
        "observer reads absolute draw clock without advancing trail age");
    frame.maCounts[0] = 0; suSteps = suDraws = 0; frame.Render(1.f);
    Check(suSteps == 4 && suKinds[0] == 0 && suKinds[1] == 1 && suKinds[2] == 3
        && suKinds[3] == 4 && suDraws == 0,"expired empty original pass still gets a valid paired observation");
    sbPair = false; suSteps = suDraws = 0; frame.maCounts[0] = 1; frame.Render(1.f);
    Check(suSteps == 4 && suKinds[3] == 3 && suDraws == 1,"readback failure never skips original drawing");
    frame.mbReady = false; suSteps = 0; frame.Render(1.f);
    Check(suSteps == 0,"original unready gate prevents both readback and draw");
    std::printf("PCTrailPixelObserver: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
