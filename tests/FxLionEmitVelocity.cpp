// L4 WORLDVFX (owner's list 2026-09-27) -- the LION particle's inherited velocity.
//
// cParticleEmitter::Emit @0x82914D38 keeps TWO vectors on its stack:
//   var_90  the emitter's raw velocity -- the locator arm copies cParticleLocator::mVel (locator+0x40,
//           `lvx128 v13` 0x82914E28 -> `stvx128` 0x82914E58), the sub-emitter arm mParentVel (this+0x50,
//           `lvx128 v0` 0x82914DA4 -> `stvx128` 0x82914DC0);
//   var_50  the spawn matrix's translation, advanced along that velocity: wa + vel * elapsed, one fmuls and
//           one fadds per lane (0x82914E94..0x82914ED8), w = 1.0 (flt_82001C98).
// InitialiseParticle gets r7 = &var_80 (the matrix) and r8 = &var_90 (the velocity) at 0x82914F28, and
// ParticleInsert r5 = &var_80 / r6 = &var_90 at 0x82914FB4 (it hands r6 on as InitialiseParticle's r8,
// 0x82913434). InitialiseParticle @0x829116A8 scales r8 by mEmitterVelWeight into mLocatorVel (0x82911AF0..
// 0x82911B10). The PC body handed on the ADVANCED SPAWN POINT as the velocity, so a particle that inherits
// emitter velocity flew off at its own world position in m/s.
//
// This fixture compiles the PRODUCTION Emit (extracted by run_fxlion_emit_velocity.py into fxlion_emit_body.inc)
// against minimal stand-ins for the types it touches, and records what reaches InitialiseParticle,
// ParticleInsert and SpawnSubEmitter. The expected spawn points are the console's fmuls / fadds, computed in
// the runner with IEEE single rounding (fxlion_emit_expect.inc).
#include <cstdint>
#include <cstdio>
#include <cstring>

typedef float    f32;
typedef double   f64;
typedef uint8_t  u8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef int64_t  s64;

struct cVector { f32 x, y, z, w; };
struct cMatrix { cVector xa, ya, za, wa; };
struct cTime
{
    s32 mTicks;
    s32 GetTicks() const { return mTicks; }
};
struct cParticleRandomSeed { u32 muSeed; };
struct sParticleNucleus    { u32 muMarker; };

struct cParticleLocator
{
    cMatrix mMat;
    cVector mVel;
    const cMatrix& GetMat(const cTime&) const { return mMat; }
};

struct cLionBindings
{
    cParticleLocator* mpLocator;
    cParticleLocator* GetpLocator() const { return mpLocator; }
};

struct cParticleEmitter;

struct cParticleBucket
{
    bool              mbFull;
    bool              mbAllocates;
    cParticleBucket*  mpEmitterNext;
    cParticleEmitter* mpEmitter;
    cTime             mLatestBirthTime;
    sParticleNucleus  mNucleus;
    cVector           mVector;
    cMatrix           mMatrix;

    bool IsFull() const { return mbFull; }
    bool AllocateParticle(u32& arSlot, sParticleNucleus** appNucleus, cVector** appVector, cMatrix** appMatrix)
    {
        if (!mbAllocates)
            return false;
        arSlot      = 0;
        *appNucleus = &mNucleus;
        *appVector  = &mVector;
        *appMatrix  = &mMatrix;
        return true;
    }
    cParticleBucket* GetEmitterNext() const                  { return mpEmitterNext; }
    void             SetLatestBirthTime(const cTime& arTime) { mLatestBirthTime = arTime; }
    void             SetEmitterNext(cParticleBucket* apNext) { mpEmitterNext = apNext; }
    void             SetEmitter(cParticleEmitter* apEmitter) { mpEmitter = apEmitter; }
};

struct cParticleBucketManager
{
    cParticleBucket* mpFresh;       // what the next AllocateBucket hands back (null == out of buckets)
    u32              muCalls;
    static cParticleBucketManager& Instance()
    {
        static cParticleBucketManager sManager = {};
        return sManager;
    }
    cParticleBucket* AllocateBucket(u32, const cTime&, u32)
    {
        ++muCalls;
        return mpFresh;
    }
};

struct cParticleDescriptor
{
    u32 mLodGroup;
    u32 GetRequiredBucketType() const { return 1u; }
};

namespace CgsDev { namespace Log { inline void WriteToLog(const char*) {} } }

// What the emitter's three callees were handed.
struct Record
{
    u32              muInitialiseCalls;
    u32              muInsertCalls;
    u32              muSpawnCalls;
    cMatrix          mMatrix;       // InitialiseParticle's arLocator / ParticleInsert's *apMatrix
    cVector          mVelocity;     // InitialiseParticle's arVelocity / ParticleInsert's arVector
    cParticleBucket* mpInsertBucket;
    cParticleBucket* mpSpawnBucket;
    u32              muSpawnSlot;
    s32              miSpawnTicks;
};
static Record gRecord;

struct cParticleEmitter
{
    static const u32 KU_FLAG_SUB_EMITTER = 0x8;

    cVector              mParentVel;
    cTime                mParentTime;
    f32                  mDt;
    u32                  mFlags;
    u32                  mEmissionCount;
    cParticleDescriptor* mpDescriptor;
    cLionBindings*       mpBindings;
    cParticleBucket*     mpBucket;
    cMatrix              mFixtureParentMatrix;   // what ParentMatrixCurrentBuild produces in this fixture

    void Emit(cParticleRandomSeed& arSeed, const cTime& arSpawnTime, const cTime& arTime);

    void ParentMatrixCurrentBuild(cMatrix& arOutMatrix, const cTime&, f32, const cTime&)
    {
        arOutMatrix = mFixtureParentMatrix;
    }
    void InitialiseParticle(sParticleNucleus&, cVector*, cMatrix*, const cMatrix& arLocator, const cVector& arVelocity,
                            cParticleRandomSeed&, const cTime&, const cTime&)
    {
        ++gRecord.muInitialiseCalls;
        gRecord.mMatrix   = arLocator;
        gRecord.mVelocity = arVelocity;
    }
    bool ParticleInsert(cParticleBucket* apBucket, cMatrix* apMatrix, const cVector& arVector, const cTime&,
                        cParticleRandomSeed&, u32, const cTime&)
    {
        ++gRecord.muInsertCalls;
        gRecord.mMatrix        = *apMatrix;
        gRecord.mVelocity      = arVector;
        gRecord.mpInsertBucket = apBucket;
        return true;
    }
    void SpawnSubEmitter(cParticleBucket* apBucket, u32 auSlot, const cTime& arTime)
    {
        ++gRecord.muSpawnCalls;
        gRecord.mpSpawnBucket = apBucket;
        gRecord.muSpawnSlot   = auSlot;
        gRecord.miSpawnTicks  = arTime.GetTicks();
    }
};

namespace
{
#include "fxlion_emit_constants.inc"   // KF_TICKS_TO_SECONDS, from the production file
}
#include "fxlion_emit_body.inc"        // cParticleEmitter::Emit, from the production file
#include "fxlion_emit_expect.inc"      // the inputs and the console's expected spawn points

static u32 guChecks   = 0;
static u32 guFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s\n", lbPassed ? "PASS" : "FAIL", lpcLabel);
}

static u32 Bits(f32 lf)
{
    u32 lu;
    std::memcpy(&lu, &lf, sizeof(lu));
    return lu;
}

static bool SameBits(const cVector& lrA, const cVector& lrB)
{
    return Bits(lrA.x) == Bits(lrB.x) && Bits(lrA.y) == Bits(lrB.y) && Bits(lrA.z) == Bits(lrB.z)
        && Bits(lrA.w) == Bits(lrB.w);
}

static void CheckVelocity(const cVector& lrExpected, const char* lpcCase)
{
    char lac[200];
    const char* const lapcLane[4] = { "x", "y", "z", "w" };
    const f32 lafGot[4]  = { gRecord.mVelocity.x, gRecord.mVelocity.y, gRecord.mVelocity.z, gRecord.mVelocity.w };
    const f32 lafWant[4] = { lrExpected.x, lrExpected.y, lrExpected.z, lrExpected.w };
    for (u32 luLane = 0; luLane < 4u; ++luLane)
    {
        std::snprintf(lac, sizeof(lac), "%s: the inherited velocity's %s lane is the emitter's own (%.6g), not the "
                      "spawn point (got %.6g)", lpcCase, lapcLane[luLane], static_cast<f64>(lafWant[luLane]),
                      static_cast<f64>(lafGot[luLane]));
        Check(Bits(lafGot[luLane]) == Bits(lafWant[luLane]), lac);
    }
}

static void CheckSpawnPoint(const u32 lauExpected[3], const char* lpcCase)
{
    char lac[200];
    const char* const lapcLane[3] = { "x", "y", "z" };
    const f32 lafGot[3] = { gRecord.mMatrix.wa.x, gRecord.mMatrix.wa.y, gRecord.mMatrix.wa.z };
    for (u32 luLane = 0; luLane < 3u; ++luLane)
    {
        std::snprintf(lac, sizeof(lac), "%s: the spawn point's %s lane is wa + vel * elapsed, fmuls then fadds "
                      "(0x82914E98.. / 0x82914EA8..): want 0x%08X got 0x%08X", lpcCase, lapcLane[luLane],
                      static_cast<unsigned>(lauExpected[luLane]), static_cast<unsigned>(Bits(lafGot[luLane])));
        Check(Bits(lafGot[luLane]) == lauExpected[luLane], lac);
    }
    std::snprintf(lac, sizeof(lac), "%s: the spawn point's w lane is 1.0 (flt_82001C98, 0x82914ED8)", lpcCase);
    Check(Bits(gRecord.mMatrix.wa.w) == Bits(1.0f), lac);
}

int main()
{
    cParticleRandomSeed lSeed = { 7u };
    cParticleDescriptor lDescriptor = { 3u };
    cParticleLocator lLocator;
    lLocator.mMat = KM_LOCATOR;
    lLocator.mVel = KV_LOCATOR_VEL;
    cLionBindings lBindings = { &lLocator };

    // ---- A. the locator arm, a bucket with a free slot: InitialiseParticle directly ----------------------------
    {
        std::memset(&gRecord, 0, sizeof(gRecord));
        cParticleBucket lBucket = {};
        lBucket.mbAllocates = true;
        cParticleEmitter lEmitter = {};
        lEmitter.mpDescriptor = &lDescriptor;
        lEmitter.mpBindings   = &lBindings;
        lEmitter.mpBucket     = &lBucket;
        const cTime lSpawn = { KI_A_SPAWN_TICKS };
        const cTime lTime  = { KI_A_TIME_TICKS };
        lEmitter.Emit(lSeed, lSpawn, lTime);

        Check(gRecord.muInitialiseCalls == 1u && gRecord.muInsertCalls == 0u,
              "A: one particle initialised straight into the free slot (no ParticleInsert)");
        CheckVelocity(KV_LOCATOR_VEL, "A (locator arm)");
        Check(SameBits(gRecord.mMatrix.xa, KM_LOCATOR.xa) && SameBits(gRecord.mMatrix.ya, KM_LOCATOR.ya)
              && SameBits(gRecord.mMatrix.za, KM_LOCATOR.za),
              "A: the spawn matrix's basis rows are the locator's, untouched");
        CheckSpawnPoint(KAU_A_SPAWN, "A (locator arm)");
        Check(lBucket.mLatestBirthTime.GetTicks() == KI_A_SPAWN_TICKS, "A: the bucket's latest birth is the spawn time");
        Check(gRecord.muSpawnCalls == 1u && gRecord.mpSpawnBucket == &lBucket && gRecord.muSpawnSlot == 0u
              && gRecord.miSpawnTicks == KI_A_TIME_TICKS,
              "A: SpawnSubEmitter(bucket, 0, arTime) follows the particle");
        Check(lEmitter.mEmissionCount == 1u, "A: mEmissionCount counts the emission");
    }

    // ---- B. the locator arm, the only bucket full: a fresh bucket and ParticleInsert -------------------------
    {
        std::memset(&gRecord, 0, sizeof(gRecord));
        cParticleBucket lFull = {};
        lFull.mbFull = true;
        cParticleBucket lFresh = {};
        lFresh.mbAllocates = true;
        cParticleBucketManager::Instance().mpFresh = &lFresh;
        cParticleEmitter lEmitter = {};
        lEmitter.mpDescriptor = &lDescriptor;
        lEmitter.mpBindings   = &lBindings;
        lEmitter.mpBucket     = &lFull;
        const cTime lSpawn = { KI_A_SPAWN_TICKS };
        const cTime lTime  = { KI_A_TIME_TICKS };
        lEmitter.Emit(lSeed, lSpawn, lTime);

        Check(gRecord.muInsertCalls == 1u && gRecord.muInitialiseCalls == 0u && gRecord.mpInsertBucket == &lFresh,
              "B: the particle goes through ParticleInsert into the fresh bucket");
        CheckVelocity(KV_LOCATOR_VEL, "B (ParticleInsert)");
        CheckSpawnPoint(KAU_A_SPAWN, "B (ParticleInsert)");
        Check(lEmitter.mpBucket == &lFresh && lFresh.mpEmitterNext == &lFull && lFresh.mpEmitter == &lEmitter,
              "B: the fresh bucket is linked at the head of the emitter's list");
        Check(gRecord.muSpawnCalls == 1u && gRecord.mpSpawnBucket == &lFresh,
              "B: SpawnSubEmitter follows on the fresh bucket");
        cParticleBucketManager::Instance().mpFresh = 0;
    }

    // ---- C. the sub-emitter arm: the parent's matrix and mParentVel since mParentTime --------------------------
    {
        std::memset(&gRecord, 0, sizeof(gRecord));
        cParticleBucket lBucket = {};
        lBucket.mbAllocates = true;
        cParticleEmitter lEmitter = {};
        lEmitter.mFlags               = cParticleEmitter::KU_FLAG_SUB_EMITTER;
        lEmitter.mParentVel           = KV_PARENT_VEL;
        lEmitter.mParentTime.mTicks   = KI_C_PARENT_TICKS;
        lEmitter.mDt                  = 0.0166666f;
        lEmitter.mFixtureParentMatrix = KM_PARENT;
        lEmitter.mpDescriptor         = &lDescriptor;
        lEmitter.mpBindings           = &lBindings;   // the [lionspawn] witness reads it
        lEmitter.mpBucket             = &lBucket;
        const cTime lSpawn = { KI_C_SPAWN_TICKS };
        const cTime lTime  = { KI_C_TIME_TICKS };
        lEmitter.Emit(lSeed, lSpawn, lTime);

        Check(gRecord.muInitialiseCalls == 1u, "C: one particle initialised");
        CheckVelocity(KV_PARENT_VEL, "C (sub-emitter arm)");
        Check(SameBits(gRecord.mMatrix.xa, KM_PARENT.xa) && SameBits(gRecord.mMatrix.ya, KM_PARENT.ya)
              && SameBits(gRecord.mMatrix.za, KM_PARENT.za),
              "C: the spawn matrix's basis rows are ParentMatrixCurrentBuild's, untouched");
        CheckSpawnPoint(KAU_C_SPAWN, "C (sub-emitter arm)");
    }

    // ---- D. no free slot and no bucket to be had: the particle is not born ------------------------------------
    {
        std::memset(&gRecord, 0, sizeof(gRecord));
        cParticleBucket lRefuses = {};
        cParticleEmitter lEmitter = {};
        lEmitter.mpDescriptor = &lDescriptor;
        lEmitter.mpBindings   = &lBindings;
        lEmitter.mpBucket     = &lRefuses;
        const cTime lSpawn = { KI_A_SPAWN_TICKS };
        const cTime lTime  = { KI_A_TIME_TICKS };
        lEmitter.Emit(lSeed, lSpawn, lTime);
        Check(gRecord.muInitialiseCalls == 0u && gRecord.muInsertCalls == 0u && gRecord.muSpawnCalls == 0u,
              "D: out of buckets -- no particle, no SpawnSubEmitter (0x82914F9C)");
        Check(lEmitter.mEmissionCount == 1u, "D: mEmissionCount still counts the attempt (0x82914D68)");
    }

    std::printf("FxLionEmitVelocity: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
