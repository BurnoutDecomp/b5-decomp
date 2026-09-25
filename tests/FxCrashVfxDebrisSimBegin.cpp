// FX-CRASHVFX (crash parity 2026-09-25): THE DEBRIS SIMULATION, dispatch side -- ParticleModule::BeginSimulateDebris
// @0x82289A98: every array's FreeExpiredBuckets, then one DebrisUpdateJobData per array with live buckets (list
// head, preset bounciness and drag, the crash-mode collision bit, the cache, the time, the step, and a Random seeded
// SetSeed(mRandom.RandomUInt())), and the jobs started.
//
// run_fxcrashvfx_debris_sim.py extracts the PRODUCTION body out of ParticleModule.cpp and compiles it onto this
// fixture: a ParticleModule carrying the real BrnDebrisArray[5], CgsNumeric::Random and DebrisUpdateJobData[5], a
// DispatchThreadInputBuffer answering the three getters, and BrnDebrisArray::FreeExpiredBuckets /
// DebrisUpdateJob::Execute defined HERE as recorders (the console's AddJobs @0x82BCB498 is the boundary there; this
// host runs each started job in place, so the recorder sees the job data exactly as the console hands it over).
//
// The expected values are the CONSOLE'S OWN OUTPUTS (FxCrashVfxDebrisSimData.h, gen_debris_sim_data.py, the
// function's real instruction words on emu64): the five FreeExpiredBuckets calls, all five job-data slots (the
// console writes a skipped array's lite data too), the jobs started, the module Random, the wait count.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h"

#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <new>
#include <vector>

#include "FxCrashVfxDebrisSimData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
        ++gFailures;
    std::printf("%s  %s\n", lbPassed ? "pass" : "FAIL", lpcLabel);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnEffects { struct BrnCrashTriangleCache { u32 muTag; }; }

namespace BrnParticle
{
    class ParticleModule;
}
namespace BrnGame
{
    struct DispatchThreadInputBuffer;
}

namespace BrnParticle
{
    class ParticleModule
    {
    public:
        static const u32 KU_NUM_DEBRIS_ARRAYS      = 5;
        static const s32 KI_NUM_DEBRIS_UPDATE_JOBS = 5;

        struct DispatchThreadUpdateData
        {
            f32 mfCurrentTime;
            f32 mfCurrentTimeStep;
        };
        struct ParticleRenderData
        {
            u16 muFlags;
        };

        Native::BrnDebrisArray      maDebris[KU_NUM_DEBRIS_ARRAYS];
        CgsNumeric::Random          mRandom;
        Native::DebrisUpdateJobData maDebrisUpdateJobData[KI_NUM_DEBRIS_UPDATE_JOBS];
        s32                         miNumDebrisUpdateJobsToWaitOn;

        void BeginSimulateDebris(const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInput);
    };
}

namespace BrnGame
{
    struct DispatchThreadInputBuffer
    {
        const BrnParticle::ParticleModule::DispatchThreadUpdateData* mpData;
        const BrnEffects::BrnCrashTriangleCache*                     mpCache;
        const BrnParticle::ParticleModule::ParticleRenderData*       mpRender;

        const BrnParticle::ParticleModule::DispatchThreadUpdateData* GetParticleData() const { return mpData; }
        const BrnEffects::BrnCrashTriangleCache* GetBufferCrashTriangleCache() const { return mpCache; }
        const BrnParticle::ParticleModule::ParticleRenderData* GetParticleRenderData() const { return mpRender; }
    };
}

// ---- the recorders --------------------------------------------------------------------------------------
struct FreeCall { u32 muArray, muTime, muFlag; };
struct ExecCall
{
    const void* mpBucketList;
    u32 maBounce[4];
    u32 muDrag, muCollision, muNumArrays;
    const void* mpCache;
    u32 muTime, muDt;
    u32 maRing[8];
    u64 muSeed;
    u32 muIndex;
};
static std::vector<FreeCall> gFrees;
static std::vector<ExecCall> gExecs;
static BrnParticle::ParticleModule* gpModule = 0;

namespace BrnParticle
{
namespace Native
{
    void BrnDebrisArray::FreeExpiredBuckets(f32 lfCurrentTime, bool lbIsRunningAt30Hz)
    {
        FreeCall lCall;
        lCall.muArray = static_cast<u32>(this - gpModule->maDebris);
        lCall.muTime  = Bits(lfCurrentTime);
        lCall.muFlag  = lbIsRunningAt30Hz ? 1u : 0u;
        gFrees.push_back(lCall);
    }

    void DebrisUpdateJob::Execute(DebrisUpdateJobData* lpJobData)
    {
        ExecCall lCall;
        const BrnDebrisArrayLite& lrLite = lpJobData->maDebrisArrays[0];
        lCall.mpBucketList = lrLite.mpBucketList;
        lCall.maBounce[0] = Bits(lrLite.mBounciness.x); lCall.maBounce[1] = Bits(lrLite.mBounciness.y);
        lCall.maBounce[2] = Bits(lrLite.mBounciness.z); lCall.maBounce[3] = Bits(lrLite.mBounciness.w);
        lCall.muDrag      = Bits(lrLite.mfDragResistance);
        lCall.muCollision = lrLite.mbCollisionEnabled ? 1u : 0u;
        lCall.muNumArrays = lpJobData->muNumDebrisArrays;
        lCall.mpCache     = lpJobData->mpTriCache;
        lCall.muTime      = Bits(lpJobData->mfCurrentTime);
        lCall.muDt        = Bits(lpJobData->mfTimeStep);
        for (u32 i = 0; i < 8; ++i)
            lCall.maRing[i] = lpJobData->mRandom.mauIntegerBuffer[i];
        lCall.muSeed  = lpJobData->mRandom.muSeed;
        lCall.muIndex = lpJobData->mRandom.muOldestBufferIndex;
        gExecs.push_back(lCall);
    }
}

#include "fxcrashvfx_debris_sim_begin.inc"
}

int main()
{
    static BrnParticle::Native::BrnDebrisArrayParams saParams[5];
    static BrnParticle::Native::BrnDebrisArray::DebrisBucket* sapBuckets[5];
    static BrnEffects::BrnCrashTriangleCache sCache;
    for (u32 a = 0; a < 5; ++a)
        sapBuckets[a] = reinterpret_cast<BrnParticle::Native::BrnDebrisArray::DebrisBucket*>(0x1000u * (a + 1));

    const u32 luNumCases = static_cast<u32>(sizeof(kaSimBeginCases) / sizeof(kaSimBeginCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const SimBeginCase& lrCase = kaSimBeginCases[luCase];
        char lacLabel[400];

        void* lpStorage = _aligned_malloc(sizeof(BrnParticle::ParticleModule), 16);
        std::memset(lpStorage, 0, sizeof(BrnParticle::ParticleModule));
        BrnParticle::ParticleModule* lpModule = static_cast<BrnParticle::ParticleModule*>(lpStorage);
        gpModule = lpModule;
        for (u32 a = 0; a < 5; ++a)
        {
            saParams[a].mvBounciness.x = Float(lrCase.maBounce[a][0]);
            saParams[a].mvBounciness.y = Float(lrCase.maBounce[a][1]);
            saParams[a].mvBounciness.z = Float(lrCase.maBounce[a][2]);
            saParams[a].mvBounciness.w = Float(lrCase.maBounce[a][3]);
            saParams[a].mfDragResistance = Float(lrCase.maDrag[a]);
            lpModule->maDebris[a].mpParams  = &saParams[a];
            lpModule->maDebris[a].mpBuckets = lrCase.maLive[a] ? sapBuckets[a] : 0;
        }
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;
        lpModule->miNumDebrisUpdateJobsToWaitOn = -1;

        BrnParticle::ParticleModule::DispatchThreadUpdateData lData;
        lData.mfCurrentTime     = Float(lrCase.muTime);
        lData.mfCurrentTimeStep = Float(lrCase.muDt);
        BrnParticle::ParticleModule::ParticleRenderData lRender;
        lRender.muFlags = static_cast<u16>(lrCase.muFlags);
        BrnGame::DispatchThreadInputBuffer lInput;
        lInput.mpData   = &lData;
        lInput.mpCache  = &sCache;
        lInput.mpRender = &lRender;

        gFrees.clear();
        gExecs.clear();
        const unsigned luAssertsBefore = gAsserts;
        lpModule->BeginSimulateDebris(&lInput);

        // 1. the five FreeExpiredBuckets calls
        bool lbFrees = gFrees.size() == lrCase.muNumFrees;
        for (u32 k = 0; lbFrees && k < lrCase.muNumFrees; ++k)
            lbFrees = gFrees[k].muArray == lrCase.maFreeArray[k] && gFrees[k].muTime == lrCase.maFreeTime[k]
                   && gFrees[k].muFlag == lrCase.maFreeFlag[k];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u (%s): FreeExpiredBuckets(time, crash-mode bit) on every array, in order: %s",
                      luCase, lrCase.mpcName, lbFrees ? "yes" : "no");
        Check(lbFrees, lacLabel);

        // 2. all five job-data slots' lite fields (the console writes skipped arrays' too)
        u32 luSlots = 0;
        for (u32 n = 0; n < 5; ++n)
        {
            const u32* lau = lrCase.maJobs[n];
            const BrnParticle::Native::BrnDebrisArrayLite& lrLite = lpModule->maDebrisUpdateJobData[n].maDebrisArrays[0];
            u32 luWantArray = 0xFFFFFFFFu;
            if (lau[0] != 0u)
                luWantArray = (lau[0] - 0x40100000u) / 0x2000u;
            u32 luGotArray = 0xFFFFFFFFu;
            for (u32 a = 0; a < 5; ++a)
                if (lrLite.mpBucketList == sapBuckets[a])
                    luGotArray = a;
            const bool lbSame = luGotArray == luWantArray
                             && Bits(lrLite.mBounciness.x) == lau[4] && Bits(lrLite.mBounciness.y) == lau[5]
                             && Bits(lrLite.mBounciness.z) == lau[6] && Bits(lrLite.mBounciness.w) == lau[7]
                             && Bits(lrLite.mfDragResistance) == lau[8]
                             && (lrLite.mbCollisionEnabled ? 1u : 0u) == (lau[9] >> 24);
            luSlots += lbSame ? 1u : 0u;
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the five DebrisUpdateJobData slots' lite arrays (list head, bounciness, drag, collision) "
                      "are the console's: %u/5", luCase, luSlots);
        Check(luSlots == 5u, lacLabel);

        // 3. the jobs started: count, per-job fields and Random
        bool lbJobs = gExecs.size() == lrCase.muAddCount
                   && static_cast<u32>(lpModule->miNumDebrisUpdateJobsToWaitOn) == lrCase.muAddCount
                   && (lrCase.muAddCount == 0u) == (lrCase.muNumJobs == 0u);
        for (u32 n = 0; lbJobs && n < gExecs.size(); ++n)
        {
            const u32* lau = lrCase.maJobs[n];
            const ExecCall& lrGot = gExecs[n];
            const u64 luSeed = (static_cast<u64>(lau[24]) << 32) | lau[25];
            lbJobs = lrGot.muNumArrays == lau[12] && lrGot.mpCache == &sCache && lau[13] == 0x40200000u
                  && lrGot.muTime == lau[14] && lrGot.muDt == lau[15]
                  && lrGot.muSeed == luSeed && lrGot.muIndex == lau[26];
            for (u32 i = 0; lbJobs && i < 8; ++i)
                lbJobs = lrGot.maRing[i] == lau[16 + i];
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: %u job(s) started, each with one array, the cache, the time, the step and the "
                      "console's seeded Random (SetSeed(RandomUInt())): %s", luCase, lrCase.muAddCount,
                      lbJobs ? "yes" : "no");
        Check(lbJobs, lacLabel);

        // 4. the module Random and the asserts
        bool lbRing = lpModule->mRandom.muSeed == lrCase.muSeedOut
                   && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut
                   && (gAsserts - luAssertsBefore) == lrCase.muAsserts;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the module Random (one step per job) and the assert count are the console's", luCase);
        Check(lbRing, lacLabel);

        _aligned_free(lpStorage);
    }

    std::printf("FxCrashVfxDebrisSimBegin: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
