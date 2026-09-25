// FX-CRASHVFX (crash parity 2026-09-24, item 2): EffectsModule::ProcessCarDetatchedPartContacts @0x82292FA0 --
// the detached-part DUST arm (meType 91 -> CRASH IMPACT DUST simple particles) and the gates ahead of it.
//
// run_fxcrashvfx_detached_dust.py extracts the PRODUCTION body (and the anonymous-namespace constants and
// helpers it names) out of EffectsModule.cpp and compiles it onto a fixture that carries exactly the members
// the body touches: the real CgsNumeric::Random (CgsRandom.cpp), the accumulator array, a SpawnSimple recorder,
// a HandleSparkContacts recorder and a fake surface list.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxDetachedDustData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_dust_data.py, which runs the drain's real instruction words
// (emu64.py: 64-bit GPRs for the inlined LCG, fused fmadds/fmsubs rounded once, the VMX speed gates) and
// records SpawnSimple @0x82281A10 / HandleSparkContacts @0x822906A8 at the call boundary. Compared bit for
// bit: every spawn's position (4 lanes), velocity (4 lanes), type, size, time and alpha; every spark call's
// contact, velocity, type, five floats and flag; the nine accumulators; the ring, seed and cursor after the
// call; and the assert count.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/BrnEntityTypes.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/visualfxsurface.h"
#if __has_include("GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h")
#include "GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h"   // the dust witnesses' BRN_SIMPLEFX_DIAG gate
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "FxCrashVfxDetachedDustData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gDustLines = 0, gNotReconstructed = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
    else
    {
        std::printf("pass  %s\n", lpcLabel);
    }
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
namespace Log
{
    void WriteToLog(const char* lpcText)
    {
        if (lpcText != nullptr && std::strncmp(lpcText, "[simplefx] dust", 15) == 0)
            ++gDustLines;
    }
}
}

// ---- the fake surface list: 64 surfaces, each layout carrying its visualfxsurface ref at +0x10 ---------
namespace
{
    const u32 KU_NUM_FAKE_SURFACES = 64;
    Attrib::RefSpec              gaSurfaceRefs[KU_NUM_FAKE_SURFACES];
    alignas(16) unsigned char    gaSurfaceLayouts[KU_NUM_FAKE_SURFACES][0x100];
    alignas(16) unsigned char    gaVfxLayouts[KU_NUM_FAKE_SURFACES][0x100];
    alignas(16) unsigned char    gaDefaultArea[0x100];
    std::vector<u32>             gSurfaceLookups;
}

namespace Attrib
{
    Instance::Instance(const RefSpec& lrRefSpec, void* lpOwner)
        : mpCollection(nullptr), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
        const unsigned char* lpRef = reinterpret_cast<const unsigned char*>(&lrRefSpec);
        const unsigned char* lpRefs = reinterpret_cast<const unsigned char*>(gaSurfaceRefs);
        const unsigned char* lpLayouts = &gaSurfaceLayouts[0][0];
        if (lpRef >= lpRefs && lpRef < lpRefs + sizeof(gaSurfaceRefs))
            mpAttributeData = gaSurfaceLayouts[(lpRef - lpRefs) / sizeof(RefSpec)];
        else if (lpRef >= lpLayouts && lpRef < lpLayouts + sizeof(gaSurfaceLayouts))
            mpAttributeData = gaVfxLayouts[(lpRef - lpLayouts) / 0x100];
    }
    Instance::~Instance() {}
    int  Instance::GetClass() const { return 0; }
    u64  Instance::GetCollection() const { return 0; }
    void AssertOnClassCheck(int, int, u64) { ++gAsserts; }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
}

namespace BrnEffects
{
namespace
{
    typedef ::EActiveRaceCarIndex         EActiveRaceCarIndex;
    typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;

    // The active-race-car interface: the eight RaceCarStates the drain's hidden gate reads.
    struct FakeActiveRaceCars
    {
        RaceCarState* mpStates;
        const RaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const { return &mpStates[leIndex]; }
    };
    typedef FakeActiveRaceCars RCEntityActiveRaceCarOutputInterface;

    struct EffectsModuleParams
    {
        f32     mDt;
        f32     mTime;
        u8      mPad08[8];
        Vector3 mCameraPosition;
    };

    // The parent revision's dust arm announced itself instead of running; its announcement lands here.
    inline void LogNotReconstructed(bool& lrbLogged, const char*)
    {
        lrbLogged = true;
        ++gNotReconstructed;
    }

#include "fxcrashvfx_dust_consts.inc"
}

struct SpawnRecord
{
    Vector3 mvPosition;
    Vector3 mvVelocity;
    u32     muType;
    f32     mfSize, mfTime, mfAlpha;
};

struct SparkRecord
{
    u32     muEntityA;
    Vector3 mvPointOnA;
    Vector3 mvVelocity;
    u32     muType;
    f32     maf[5];
    bool    mbFlag;
};

struct FakeParticleModule
{
    std::vector<SpawnRecord> mSpawns;
    void SpawnSimple(Vector3 lvPosition, Vector3 lvVelocity, BrnParticle::Native::ENativeParticleType leType,
                     f32 lfSizeScale, f32 lfSpawnTime, f32 lfAlpha)
    {
        SpawnRecord lRecord;
        lRecord.mvPosition = lvPosition;
        lRecord.mvVelocity = lvVelocity;
        lRecord.muType     = static_cast<u32>(leType);
        lRecord.mfSize     = lfSizeScale;
        lRecord.mfTime     = lfSpawnTime;
        lRecord.mfAlpha    = lfAlpha;
        mSpawns.push_back(lRecord);
    }
};

struct FakeSurfaceList
{
    void* Surfaces(u32 luIndex)
    {
        gSurfaceLookups.push_back(luIndex);
        return luIndex < KU_NUM_FAKE_SURFACES ? &gaSurfaceRefs[luIndex] : nullptr;
    }
};

class DetachedDustFixture
{
public:
    static const u32 KU_NUM_ACTIVE_RACE_CARS = 8;

    CgsNumeric::Random       mRandom;
    f32                      mafAccumulatedParticleCountTyres[9];
    FakeParticleModule       mParticleModule;
    FakeSurfaceList          mSurfaceList;
    std::vector<SparkRecord> mSparks;

    void HandleSparkContacts(const BrnPhysics::ContactSpy::BaseContact& lrContact, Vector3 lvVelocity,
                             BrnParticle::Native::ESparkArrayID leSparkType, f32 lf1, f32 lf2, f32 lf3, f32 lf4,
                             f32 lf5, bool lbIsCrashing)
    {
        SparkRecord lRecord;
        lRecord.muEntityA  = lrContact.mEntityIdA.muValue;
        lRecord.mvPointOnA = lrContact.mPointOnA;
        lRecord.mvVelocity = lvVelocity;
        lRecord.muType     = static_cast<u32>(leSparkType);
        lRecord.maf[0] = lf1; lRecord.maf[1] = lf2; lRecord.maf[2] = lf3; lRecord.maf[3] = lf4; lRecord.maf[4] = lf5;
        lRecord.mbFlag     = lbIsCrashing;
        mSparks.push_back(lRecord);
    }

    void ProcessCarDetatchedPartContacts(
             const BrnPhysics::ContactSpy::ContactSpyData::PhysicalCarPartContactQueue* lpQueue,
             const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
             const EffectsModuleParams& lrParams);
};

#include "fxcrashvfx_dust_body.inc"
}

static Vector3 VectorFromBits(const u32 lau[4])
{
    Vector3 lv;
    lv.x = Float(lau[0]); lv.y = Float(lau[1]); lv.z = Float(lau[2]); lv.w = Float(lau[3]);
    return lv;
}

static bool SameBits(const Vector3& lrv, const u32 lau[4])
{
    return Bits(lrv.x) == lau[0] && Bits(lrv.y) == lau[1] && Bits(lrv.z) == lau[2] && Bits(lrv.w) == lau[3];
}

int main()
{
    typedef BrnPhysics::ContactSpy::ContactSpyData::PhysicalCarPartContactQueue Queue;
    Queue* lpQueue = new Queue();
    lpQueue->Construct(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART);

    void* lpStateStorage = std::calloc(8, sizeof(BrnPhysics::Vehicle::RaceCarState));
    BrnEffects::FakeActiveRaceCars lCars;
    lCars.mpStates = static_cast<BrnPhysics::Vehicle::RaceCarState*>(lpStateStorage);

    unsigned luTotalSpawns = 0;
    for (u32 luCase = 0; luCase < sizeof(kaDustCases) / sizeof(kaDustCases[0]); ++luCase)
    {
        const DustCaseData& lrCase = kaDustCases[luCase];
        char lacLabel[256];

        BrnEffects::DetachedDustFixture* lpFixture = new BrnEffects::DetachedDustFixture();
        for (u32 i = 0; i < 8; ++i)
            lpFixture->mRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lpFixture->mRandom.muSeed = lrCase.muSeed;
        lpFixture->mRandom.muOldestBufferIndex = lrCase.muIndex;
        for (u32 i = 0; i < 9; ++i)
            lpFixture->mafAccumulatedParticleCountTyres[i] = Float(lrCase.maAccumulated[i]);

        for (u32 i = 0; i < 8; ++i)
            lCars.mpStates[i].mbIsHidden = ((lrCase.muHiddenMask >> i) & 1u) != 0;
        for (u32 i = 0; i < KU_NUM_FAKE_SURFACES; ++i)
        {
            std::memset(gaVfxLayouts[i], 0, sizeof(gaVfxLayouts[i]));
            gaVfxLayouts[i][0x4F] = static_cast<unsigned char>((lrCase.muSparksOnMask >> i) & 1u);
            const f32 lfScale = 0.25f + static_cast<f32>(i);
            std::memcpy(&gaVfxLayouts[i][0x54], &lfScale, 4);
        }
        gSurfaceLookups.clear();

        lpQueue->Construct(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART);
        for (u32 i = 0; i < lrCase.muNumContacts; ++i)
        {
            const DustContactData& lrIn = lrCase.mpContacts[i];
            BrnPhysics::ContactSpy::PhysicalCarPartContact lContact;
            std::memset(&lContact, 0, sizeof(lContact));
            lContact.mEntityIdA.muValue     = lrIn.muEntityA;
            lContact.mEntityIdB.muValue     = lrIn.muEntityB;
            lContact.mCollisionTagB.muValue = lrIn.muTagB;
            lContact.mFrictionStress = VectorFromBits(lrIn.maFriction);
            lContact.mNormalStress   = VectorFromBits(lrIn.maNormalStress);
            lContact.mNormal         = VectorFromBits(lrIn.maNormal);
            lContact.mPointOnA       = VectorFromBits(lrIn.maPointOnA);
            lContact.mPointOnB       = VectorFromBits(lrIn.maPointOnB);
            lContact.mVelocity       = VectorFromBits(lrIn.maVelocity);
            lContact.meType          = static_cast<BrnPhysics::ContactSpy::EBodyParts>(lrIn.miType);
            lContact.mbIsHinged      = false;
            lpQueue->AddEvent(lContact);
        }

        BrnEffects::EffectsModuleParams lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.mDt   = Float(lrCase.muDt);
        lParams.mTime = Float(lrCase.muTime);

        const unsigned luAssertsBefore = gAsserts;
        lpFixture->ProcessCarDetatchedPartContacts(lpQueue, &lCars, lParams);

        // ---- the spawns ----
        const std::vector<BrnEffects::SpawnRecord>& lrSpawns = lpFixture->mParticleModule.mSpawns;
        std::snprintf(lacLabel, sizeof(lacLabel), "case %u (%s): %u dust spawns (console %u)",
                      luCase, lrCase.mpcName, static_cast<unsigned>(lrSpawns.size()), lrCase.muNumSpawns);
        Check(lrSpawns.size() == lrCase.muNumSpawns, lacLabel);
        u32 luSpawnMatches = 0;
        for (u32 i = 0; i < lrCase.muNumSpawns && i < lrSpawns.size(); ++i)
        {
            const DustSpawnData& lrWant = lrCase.mpSpawns[i];
            const BrnEffects::SpawnRecord& lrGot = lrSpawns[i];
            const bool lbSame = lrGot.muType == lrWant.muType
                             && SameBits(lrGot.mvPosition, lrWant.maPosition)
                             && SameBits(lrGot.mvVelocity, lrWant.maVelocity)
                             && Bits(lrGot.mfSize) == lrWant.muSize
                             && Bits(lrGot.mfTime) == lrWant.muTime
                             && Bits(lrGot.mfAlpha) == lrWant.muAlpha;
            if (!lbSame)
            {
                std::printf("      spawn %u: got type %u pos (%g,%g,%g,%g) vel (%.9g,%.9g,%.9g,%g) size %.9g time %g alpha %g\n"
                            "               want type %u vel (%.9g,%.9g,%.9g,%g) size %.9g\n",
                            i, lrGot.muType, lrGot.mvPosition.x, lrGot.mvPosition.y, lrGot.mvPosition.z,
                            lrGot.mvPosition.w, lrGot.mvVelocity.x, lrGot.mvVelocity.y, lrGot.mvVelocity.z,
                            lrGot.mvVelocity.w, lrGot.mfSize, lrGot.mfTime, lrGot.mfAlpha, lrWant.muType,
                            Float(lrWant.maVelocity[0]), Float(lrWant.maVelocity[1]), Float(lrWant.maVelocity[2]),
                            Float(lrWant.maVelocity[3]), Float(lrWant.muSize));
            }
            luSpawnMatches += lbSame ? 1u : 0u;
        }
        luTotalSpawns += lrCase.muNumSpawns;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: every spawn bit-identical to the console's (type 2, position, velocity = normal + "
                      "fused jitter, size = fused Random*0.5+0.2, time, alpha 1.0): %u/%u",
                      luCase, luSpawnMatches, lrCase.muNumSpawns);
        Check(luSpawnMatches == lrCase.muNumSpawns && lrSpawns.size() == lrCase.muNumSpawns, lacLabel);

        // ---- the spark arm (unchanged by this fix, but it shares the loop and the gates) ----
        const std::vector<BrnEffects::SparkRecord>& lrSparks = lpFixture->mSparks;
        u32 luSparkMatches = 0;
        for (u32 i = 0; i < lrCase.muNumSparks && i < lrSparks.size(); ++i)
        {
            const DustSparkData& lrWant = lrCase.mpSparks[i];
            const BrnEffects::SparkRecord& lrGot = lrSparks[i];
            bool lbSame = lrGot.muEntityA == lrWant.muEntityA && SameBits(lrGot.mvPointOnA, lrWant.maPointOnA)
                       && SameBits(lrGot.mvVelocity, lrWant.maVelocity) && lrGot.muType == lrWant.muType
                       && (lrGot.mbFlag ? 1u : 0u) == lrWant.muFlag;
            for (u32 k = 0; k < 5; ++k)
                lbSame = lbSame && Bits(lrGot.maf[k]) == lrWant.maF[k];
            luSparkMatches += lbSame ? 1u : 0u;
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: spark calls identical to the console's: %u/%u (got %u)",
                      luCase, luSparkMatches, lrCase.muNumSparks, static_cast<unsigned>(lrSparks.size()));
        Check(luSparkMatches == lrCase.muNumSparks && lrSparks.size() == lrCase.muNumSparks, lacLabel);

        // ---- the accumulators, the ring, the asserts ----
        u32 luAccMatches = 0;
        for (u32 i = 0; i < 9; ++i)
            luAccMatches += (Bits(lpFixture->mafAccumulatedParticleCountTyres[i]) == lrCase.maAccumulatedOut[i]) ? 1u : 0u;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the nine accumulators after the call are the console's bits (fused mDt*20 + carry, "
                      "stored before the count; NaN / negative / fractional carries kept): %u/9", luCase, luAccMatches);
        Check(luAccMatches == 9, lacLabel);

        bool lbRing = lpFixture->mRandom.muSeed == lrCase.muSeedOut
                   && lpFixture->mRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lpFixture->mRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: mRandom's ring, seed and cursor after the call are the console's (4 draws per particle)",
                      luCase);
        Check(lbRing, lacLabel);

        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: asserts fired %u (console %u)", luCase,
                      gAsserts - luAssertsBefore, lrCase.muNumAsserts);
        Check(gAsserts - luAssertsBefore == lrCase.muNumAsserts, lacLabel);

        delete lpFixture;
    }

    Check(gNotReconstructed == 0, "the dust arm runs: no NOT RECONSTRUCTED announcement from the drain");
    Check(gDustLines == 0, "the [simplefx] dust witness is default-off (BRN_SIMPLEFX_DIAG unset)");
    std::printf("(%u console spawns compared)\n", luTotalSpawns);

    std::free(lpStateStorage);
    delete lpQueue;
    std::printf("FxCrashVfxDetachedDust: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
