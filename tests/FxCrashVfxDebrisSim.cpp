// FX-CRASHVFX (crash parity 2026-09-25): THE DEBRIS SIMULATION, job side -- DebrisUpdateJob::Execute @0x82C08298
// -> BrnDebrisArrayLite::Update @0x82C08B58 -> the per-bucket integrator sub_82C08410 (gravity + drag on the
// velocity, the fused position step, the spin advanced by the distance moved, and in crash mode the step tested
// against the crash triangle cache: back to 95% of the way to the hit, reflected, the array's bounciness, a random
// 60..100% and a random sideways kick, one bounce spent).
//
// run_fxcrashvfx_debris_sim.py compiles the revision's WHOLE BrnDebrisArrayLite.cpp onto this fixture: the real
// CgsNumeric::Random, the real BrnDebrisArray / FXBucket / BrnCrashLineTriangleCacheFormat types, and
// BrnCrashTriangleCache::CollideWithTriangleCache defined HERE as the boundary -- it records every line segment it
// is handed and writes back the scripted hit (unit normal + line parameter) the console run was given.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxDebrisSimData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_debris_sim_data.py, which runs the job's real instruction words on
// emu64 (the integrator is an IDA export hole, read from the raw image; the gravity splat 0x832BAD30 filled by
// RUNNING its CRT thunk 0x82C74718). Compared bit for bit: every particle's four quadwords and bounce count, every
// collide call's input segments, the job Random's ring / seed / cursor, the assert count.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Effects/BrnCrashTriangleCache.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
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

// ---- the collide boundary -------------------------------------------------------------------------------
struct CollideCall
{
    u32              muCount;
    std::vector<u32> maWords;   // 12 per line: start, end, normal + parameter (the INPUT)
};
static std::vector<CollideCall> gCollides;
static const SimScript*         gpScript     = 0;
static u32                      guScriptLeft = 0;

template <class T> static void PushLanes(std::vector<u32>& lrOut, const T& lrv)
{
    lrOut.push_back(Bits(lrv.x)); lrOut.push_back(Bits(lrv.y)); lrOut.push_back(Bits(lrv.z)); lrOut.push_back(Bits(lrv.w));
}

namespace BrnEffects
{
    void BrnCrashTriangleCache::CollideWithTriangleCache(BrnCrashLineTriangleCacheFormat* lpLinesToTest,
                                                         u32 luNumberLines) const
    {
        CollideCall lCall;
        lCall.muCount = luNumberLines;
        for (u32 k = 0; k < luNumberLines; ++k)
        {
            BrnCrashLineTriangleCacheFormat& lrLine = lpLinesToTest[k];
            PushLanes(lCall.maWords, lrLine.mLineStartPosition);
            PushLanes(lCall.maWords, lrLine.mLineEndPos);
            PushLanes(lCall.maWords, lrLine.mLineIntersectNormalPlusLineParms);
            if (guScriptLeft != 0)
            {
                const SimScript& lrStep = *gpScript;
                ++gpScript;
                --guScriptLeft;
                if (lrStep.muHit != 0)
                {
                    lrLine.mLineIntersectNormalPlusLineParms.x = Float(lrStep.maResult[0]);
                    lrLine.mLineIntersectNormalPlusLineParms.y = Float(lrStep.maResult[1]);
                    lrLine.mLineIntersectNormalPlusLineParms.z = Float(lrStep.maResult[2]);
                    lrLine.mLineIntersectNormalPlusLineParms.w = Float(lrStep.maResult[3]);
                }
            }
        }
        gCollides.push_back(lCall);
    }
}

#include "fxcrashvfx_debris_sim.inc"

using BrnParticle::Native::BrnDebris;
typedef BrnParticle::Native::BrnDebrisArray::DebrisBucket DebrisBucket;

template <class T> static void LoadLanes(T& lrv, const u32* lau)
{
    lrv.x = Float(lau[0]); lrv.y = Float(lau[1]); lrv.z = Float(lau[2]); lrv.w = Float(lau[3]);
}

static bool SameParticle(const BrnDebris& lrGot, const SimParticle& lrWant)
{
    std::vector<u32> lGot;
    PushLanes(lGot, lrGot.mAxisPlusAngle);
    PushLanes(lGot, lrGot.mPositionPlusRotVel);
    PushLanes(lGot, lrGot.mVelocityPlusScale);
    PushLanes(lGot, lrGot.mDiffuseColour);
    for (u32 k = 0; k < 16; ++k)
        if (lGot[k] != lrWant.maWords[k])
            return false;
    return lrGot.muBounceCount == lrWant.muBounces;
}

int main()
{
    const u32 luNumCases = static_cast<u32>(sizeof(kaSimExecCases) / sizeof(kaSimExecCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const SimExecCase& lrCase = kaSimExecCases[luCase];
        char lacLabel[400];

        // The buckets, 16-aligned as the console's allocator hands them out.
        std::vector<DebrisBucket*> lBuckets;
        for (u32 b = 0; b < lrCase.muNumBuckets; ++b)
        {
            DebrisBucket* lpBucket = static_cast<DebrisBucket*>(_aligned_malloc(sizeof(DebrisBucket), 16));
            std::memset(lpBucket, 0, sizeof(DebrisBucket));
            const SimBucket& lrData = lrCase.mpBuckets[b];
            lpBucket->mu16NumberOfParticlesInBucket = static_cast<u16>(lrData.muNum);
            lpBucket->mu16NextPositionInBucket      = static_cast<u16>(lrData.muNum);
            for (u32 i = 0; i < lrData.muNum; ++i)
            {
                lpBucket->maParticleBirthTimes[i] = Float(lrData.mpBirths[i]);
                BrnDebris& lrDebris = lpBucket->maParticleData[i];
                LoadLanes(lrDebris.mAxisPlusAngle, lrData.mpIn[i].maWords + 0);
                LoadLanes(lrDebris.mPositionPlusRotVel, lrData.mpIn[i].maWords + 4);
                LoadLanes(lrDebris.mVelocityPlusScale, lrData.mpIn[i].maWords + 8);
                LoadLanes(lrDebris.mDiffuseColour, lrData.mpIn[i].maWords + 12);
                lrDebris.muBounceCount = static_cast<u8>(lrData.mpIn[i].muBounces);
            }
            lBuckets.push_back(lpBucket);
        }
        for (u32 b = 0; b < lBuckets.size(); ++b)
            lBuckets[b]->mpNextBucket = (b + 1 < lBuckets.size()) ? lBuckets[b + 1] : 0;

        BrnEffects::BrnCrashTriangleCache* lpCache =
            static_cast<BrnEffects::BrnCrashTriangleCache*>(_aligned_malloc(sizeof(BrnEffects::BrnCrashTriangleCache), 16));
        std::memset(lpCache, 0, sizeof(BrnEffects::BrnCrashTriangleCache));
        lpCache->mnNumberOfPackedTriangles = lrCase.muTris;

        BrnParticle::Native::DebrisUpdateJobData* lpJob = static_cast<BrnParticle::Native::DebrisUpdateJobData*>(
            _aligned_malloc(sizeof(BrnParticle::Native::DebrisUpdateJobData), 16));
        std::memset(lpJob, 0, sizeof(*lpJob));
        BrnParticle::Native::BrnDebrisArrayLite& lrLite = lpJob->maDebrisArrays[0];
        lrLite.mpBucketList = lBuckets.empty() ? 0 : lBuckets[0];
        LoadLanes(lrLite.mBounciness, lrCase.maBounciness);
        lrLite.mfDragResistance   = Float(lrCase.muDrag);
        lrLite.mbCollisionEnabled = lrCase.muCollision != 0;
        lpJob->muNumDebrisArrays  = 1;
        lpJob->mpTriCache         = lpCache;
        lpJob->mfCurrentTime      = Float(lrCase.muTime);
        lpJob->mfTimeStep         = Float(lrCase.muDt);
        for (u32 i = 0; i < 8; ++i)
            lpJob->mRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lpJob->mRandom.muSeed              = lrCase.muSeed;
        lpJob->mRandom.muOldestBufferIndex = lrCase.muIndex;

        gCollides.clear();
        gpScript     = lrCase.mpScript;
        guScriptLeft = lrCase.muScriptLen;
        const unsigned luAssertsBefore = gAsserts;

        BrnParticle::Native::DebrisUpdateJob::Execute(lpJob);

        // 1. every particle
        u32 luSame = 0, luTotal = 0, luShown = 0;
        for (u32 b = 0; b < lBuckets.size(); ++b)
        {
            const SimBucket& lrData = lrCase.mpBuckets[b];
            for (u32 i = 0; i < lrData.muNum; ++i)
            {
                ++luTotal;
                const bool lbSame = SameParticle(lBuckets[b]->maParticleData[i], lrData.mpOut[i]);
                luSame += lbSame ? 1u : 0u;
                if (!lbSame && luShown < 3u)
                {
                    ++luShown;
                    const BrnDebris& lrGot = lBuckets[b]->maParticleData[i];
                    std::printf("      bucket %u particle %u: got pos %08X %08X %08X vel %08X %08X %08X ang %08X bounces %u\n"
                                "                          want pos %08X %08X %08X vel %08X %08X %08X ang %08X bounces %u\n",
                                b, i, Bits(lrGot.mPositionPlusRotVel.x), Bits(lrGot.mPositionPlusRotVel.y),
                                Bits(lrGot.mPositionPlusRotVel.z), Bits(lrGot.mVelocityPlusScale.x),
                                Bits(lrGot.mVelocityPlusScale.y), Bits(lrGot.mVelocityPlusScale.z),
                                Bits(lrGot.mAxisPlusAngle.w), static_cast<unsigned>(lrGot.muBounceCount),
                                lrData.mpOut[i].maWords[4], lrData.mpOut[i].maWords[5], lrData.mpOut[i].maWords[6],
                                lrData.mpOut[i].maWords[8], lrData.mpOut[i].maWords[9], lrData.mpOut[i].maWords[10],
                                lrData.mpOut[i].maWords[3], lrData.mpOut[i].muBounces);
                }
            }
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u (%s): every particle (axis+angle, position+spin, velocity+scale, colour, bounces) is "
                      "the console's: %u/%u", luCase, lrCase.mpcName, luSame, luTotal);
        Check(luSame == luTotal, lacLabel);

        // 2. the collide calls
        bool lbCollides = gCollides.size() == lrCase.muNumCollides;
        for (u32 k = 0; lbCollides && k < lrCase.muNumCollides; ++k)
        {
            const SimCollide& lrWant = lrCase.mpCollides[k];
            lbCollides = gCollides[k].muCount == lrWant.muCount
                      && std::memcmp(gCollides[k].maWords.data(), lrWant.mpLines, 12u * 4u * lrWant.muCount) == 0;
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: CollideWithTriangleCache is called as on the console (%u call(s), every segment and its "
                      "{0,0,0,1} seed bit for bit): %s", luCase, lrCase.muNumCollides, lbCollides ? "yes" : "no");
        Check(lbCollides, lacLabel);

        // 3. the Random and the asserts
        bool lbRing = lpJob->mRandom.muSeed == lrCase.muSeedOut && lpJob->mRandom.muOldestBufferIndex == lrCase.muIndexOut
                   && (gAsserts - luAssertsBefore) == lrCase.muAsserts;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lpJob->mRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the job Random's ring / seed / cursor and the assert count are the console's", luCase);
        Check(lbRing, lacLabel);

        for (u32 b = 0; b < lBuckets.size(); ++b)
            _aligned_free(lBuckets[b]);
        _aligned_free(lpCache);
        _aligned_free(lpJob);
    }

    std::printf("FxCrashVfxDebrisSim: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
