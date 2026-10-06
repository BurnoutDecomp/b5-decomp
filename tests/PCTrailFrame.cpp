#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"
#include "GameSource/Effects/Particles/Native/TrailFramePC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

static u32 suChecks, suFailures;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++suFailures; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace BrnParticle { namespace Native {
TrailParams TrailSystem::mgaDefaultParams[KI_MAX_NUM_TRAIL_TYPES] = {};
} }
#include "trail_frame.inc"
using namespace BrnParticle::Native;
static void Check(bool value, const char* name)
{
    ++suChecks;
    if (!value) { ++suFailures; std::printf("FAIL %s\n", name); }
}
int main()
{
    TrailSystem live;
    live.mbIsReady = true;
    live.mRenderer = {};
    live.mRenderer.mfCurrentTime = 40.f;
    live.mRenderer.mViewProjectionMatrix.SetIdentity();
    live.mfCurrentTime = 40.f;
    live.mfCurrentTimeStep = 1.f / 60;
    for (s32 type = 0; type < KI_MAX_NUM_TRAIL_TYPES; ++type) {
        live.maActiveEmitters[type].Prepare();
        TrailSystem::mgaDefaultParams[type].mStartColour = {float(type), .3f, .4f, .77f};
        TrailSystem::mgaDefaultParams[type].mEndColour = {float(type), .3f, .4f, 0.f};
    }
    for (s32 index = 0; index < KN_TRAIL_EMITTER_POOL_SIZE; ++index) {
        auto& emitter = live.maEmitterPool[index];
        emitter = {};
        emitter.mpCurrentSegments = &live.maSegments[index];
        emitter.mpOldSegments = &live.maSegments[index + KN_TRAIL_EMITTER_POOL_SIZE];
        emitter.mn8NumSegments = 2;
        emitter.mu8TrailTypeID = s8(index % KI_MAX_NUM_TRAIL_TYPES);
        for (s32 segment = 0; segment < KN_MAX_TRAIL_SIZE; ++segment) {
            emitter.mpCurrentSegments->WriteSegmentPosition({float(index), -.2f, float(segment), 0}, segment);
            emitter.mpCurrentSegments->WriteSegmentTangent({1, 0, 0, 0}, segment);
            emitter.mpCurrentSegments->WriteSegmentTime(39.f, segment);
            emitter.mpCurrentSegments->WriteSegmentStrength(.6f, segment);
        }
        live.maActiveEmitters[emitter.mu8TrailTypeID].AddEntry(&emitter);
    }
#if HAS_TRAIL_FRAME
    TrailFramePC frame;
    frame.Publish(live);
    auto count = [&](s32 type) { return frame.maCounts[type]; };
    auto record = [&](s32 type, s32 index) { return frame.maActive[type][index]; };
    auto params = [&](s32 type) { return &frame.maParams[type]; };
#else
    auto count = [&](s32 type) { return live.maActiveEmitters[type].GetSize(); };
    auto record = [&](s32 type, s32 index) { return live.maActiveEmitters[type][index]; };
    auto params = [&](s32 type) { return &TrailSystem::mgaDefaultParams[type]; };
#endif
    for (s32 type = 0; type < KI_MAX_NUM_TRAIL_TYPES; ++type)
        Check(count(type) == 24, "all active emitters are published by trail type");
    TrailEmitter saved = *record(0, 0);
    TrailSegmentCollection segments = *saved.mpCurrentSegments;
    TrailParams colour = *params(0);
    Check(saved.mpCurrentSegments != live.maEmitterPool[0].mpCurrentSegments,
        "draw bank owns independent segment storage");
    std::thread producer([&] {
        for (u32 update = 0; update < 20000; ++update) {
            live.maEmitterPool[0].mpCurrentSegments->WriteSegmentPosition({float(update), 2, 3, 0}, 0);
            live.maEmitterPool[0].mpCurrentSegments->WriteSegmentTime(float(update), 0);
            live.maEmitterPool[0].mn8NumSegments = s8(2 + update % 14);
        }
        live.maEmitterPool[0].mu8TrailTypeID = 3;
        live.maActiveEmitters[0].RemoveEntry(0);
        TrailSystem::mgaDefaultParams[0].mStartColour.alpha = .1f;
    });
    producer.join();
    Check(count(0) == 24, "next update's active-list removal cannot affect dispatch");
    Check(record(0, 0)->mn8NumSegments == saved.mn8NumSegments,
        "segment count belongs to the published emitter");
    Check(record(0, 0)->mu8TrailTypeID == saved.mu8TrailTypeID,
        "emitter reuse cannot change the published type");
    Check(std::memcmp(record(0, 0)->mpCurrentSegments, &segments, sizeof(segments)) == 0,
        "following wheel updates cannot move or age the drawn segments");
    Check(std::memcmp(params(0), &colour, sizeof(colour)) == 0,
        "surface colour changes cannot alter published fade parameters");
#if HAS_TRAIL_FRAME
    const f32 oldClock = live.mRenderer.mfCurrentTime;
    Matrix44 camera; camera.SetIdentity(); camera.wAxis.x = 9;
    frame.Update(41.f, camera);
    Check(live.mRenderer.mfCurrentTime == oldClock, "dispatch cannot change the producer timeout clock");
    Check(frame.mRenderer.mfCurrentTime == 41 && frame.mRenderer.mViewProjectionMatrix.wAxis.x == 9,
        "dispatch uses the matching published camera and fade clock");
    frame.Publish(live);
    Check(count(0) == 23, "joined publication exposes active-list changes");
    Check(params(0)->mStartColour.alpha == .1f, "joined publication exposes surface changes");
    live.mbIsReady = false;
    frame.Publish(live);
    Check(!frame.mbReady, "unready publication suppresses the draw");
#endif
    std::printf("PCTrailFrame: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
