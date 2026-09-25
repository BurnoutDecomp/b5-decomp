// FX-CRASHVFX (crash parity 2026-09-24, item 1): EffectsModule::ProcessRaceCarContacts @0x82297C08 and its whole
// callee wall -- DoSparkShower @0x822920C0, HandleVehicleVehicleSparks @0x82296790, HandleRaceCarRaceCarSparks
// @0x82290A48, HandleBurstDebris @0x82290BC8, ParticleModule::SpawnSparkShowerFromPoint @0x8228AFC0 and
// ParticleModule::FireDebrisBurst @0x82289E70 -- the race-car contact drain that turns every scrape and crash into
// what a player sees: world-grinding / vehicle-grinding / crash spark showers, debris bursts and crash impact dust.
//
// run_fxcrashvfx_race_car_contacts.py extracts the PRODUCTION bodies (the drain region of EffectsModule.cpp with
// the anonymous-namespace constants, controllers and vector helpers it names; the two particle-module producers;
// BurstAccumulator::Update; the four randomiser bodies; the replay layout's GetCarContact / UpdateCarContact) and
// compiles them onto this fixture, which carries exactly what those bodies touch: the real CgsNumeric::Random, the
// real RaceCarContact queue, the real ActiveRaceCarData / BurstAccumulator / randomisers / event records, a
// recording inter-thread queue and SpawnSimple, and fake Attrib surfaces / debris / spark parameter blocks.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxRaceCarContactsData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_prc_data.py, which runs the drain's real instruction words and
// every callee down to the queue (emu64.py: 64-bit GPRs for the inlined LCG, fused fmadds rounded once, the VMX
// normalise idioms with vrsqrtefp / vrefp refined twice, the CRT-initialised shower controllers and vector
// constants filled by RUNNING their init thunks) and records AddEventSafe @0x822858D0, SpawnSimple @0x82281A10 and
// GetCarModelId @0x82277A18 at the call boundary. Compared bit for bit per case: every posted record, field by
// field; every SpawnSimple; the model-id calls (which car the takedown coin picked); the eight BurstAccumulators;
// the crash-dust accumulators and both burst timers; mRandom's ring / seed / cursor; the replay car-contact table;
// and the assert count.
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
#include "GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h"
#include "GameSource/Effects/ActiveRaceCarData.h"
#include "GameSource/Effects/BrnEffectsUtils.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/visualfxsurface.h"
#include "GameSource/AttribSys/Generated/classes/sparkeffect.h"
#include "GameSource/AttribSys/Generated/classes/debrisparams.h"
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "rw/core/base/ostypes.h"
#if __has_include("GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h")
#include "GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h"
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "FxCrashVfxRaceCarContactsData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gDrainLines = 0, gNotReconstructed = 0;

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
        if (lpcText != nullptr && std::strncmp(lpcText, "[racecar-contact]", 17) == 0)
            ++gDrainLines;
    }
}
}

// ---- Attrib: the fake surface list (64 surfaces, each layout carrying its visualfxsurface ref at +0x10), the
// spark / debris parameter blocks, and the handful of Instance members the bodies reach ----------------------
namespace
{
    const u32 KU_NUM_FAKE_SURFACES = 64;
    Attrib::RefSpec              gaSurfaceRefs[KU_NUM_FAKE_SURFACES];
    alignas(16) unsigned char    gaSurfaceLayouts[KU_NUM_FAKE_SURFACES][0x100];
    alignas(16) unsigned char    gaVfxLayouts[KU_NUM_FAKE_SURFACES][0x100];
    alignas(16) unsigned char    gaDefaultArea[0x200];
    alignas(16) unsigned char    gaSparkLayout[0x100];
    alignas(16) unsigned char    gaCrashDebrisLayout[0x100];
    alignas(16) unsigned char    gaRoadRageDebrisLayout[0x100];
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
    Instance::Instance(Collection* lpCollection, void* lpOwner)
        : mpCollection(lpCollection), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
    }
    Instance::~Instance() {}
    Instance& Instance::operator=(const Instance& lrOther)
    {
        mpCollection    = lrOther.mpCollection;
        mpAttributeData = lrOther.mpAttributeData;
        mpOwner         = lrOther.mpOwner;
        muFlags         = lrOther.muFlags;
        return *this;
    }
    int  Instance::GetClass() const { return 0; }
    u64  Instance::GetCollection() const { return 0; }
    void AssertOnClassCheck(int, int, u64) { ++gAsserts; }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
    Collection* FindCollection(u64, u64) { return nullptr; }
}

// ---- the camera the debris bursts read their eye point from ---------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    class Camera
    {
    public:
        Matrix44Affine mTransform;
        const Matrix44Affine& GetTransform() const { return mTransform; }
    };
}
}

// ---- the particle module: a recording inter-thread queue and SpawnSimple; the two producers are the extracted
// production bodies ---------------------------------------------------------------------------------------------
namespace BrnParticle
{
    struct PostedRecord
    {
        s32 miType;
        s32 miSize;
        alignas(16) unsigned char mau8Bytes[0x100];
    };

    struct SimpleSpawnRecord
    {
        Vector3 mvPosition;
        Vector3 mvVelocity;
        u32     muType;
        f32     mfSize, mfTime, mfAlpha;
    };

    struct RecordingQueue
    {
        std::vector<PostedRecord> mRecords;
        template <class T>
        bool AddEventSafe(const T* lpEvent, s32 liType, s32 liSize)
        {
            PostedRecord lRecord;
            std::memset(&lRecord, 0, sizeof(lRecord));
            lRecord.miType = liType;
            lRecord.miSize = liSize;
            if (liSize > 0 && liSize <= static_cast<s32>(sizeof(lRecord.mau8Bytes)))
                std::memcpy(lRecord.mau8Bytes, lpEvent, static_cast<size_t>(liSize));
            mRecords.push_back(lRecord);
            return true;
        }
    };

    class ParticleModule
    {
    public:
        RecordingQueue                 mInterThreadEventQueue;
        std::vector<SimpleSpawnRecord> mSpawns;

        void SpawnSimple(Vector3 lvPosition, Vector3 lvVelocity, Native::ENativeParticleType leParticleType,
                         f32 lfSizeScale, f32 lfSpawnTime, f32 lfAlpha)
        {
            SimpleSpawnRecord lRecord;
            lRecord.mvPosition = lvPosition;
            lRecord.mvVelocity = lvVelocity;
            lRecord.muType     = static_cast<u32>(leParticleType);
            lRecord.mfSize     = lfSizeScale;
            lRecord.mfTime     = lfSpawnTime;
            lRecord.mfAlpha    = lfAlpha;
            mSpawns.push_back(lRecord);
        }

        void SpawnSparkShowerFromPoint(Matrix44Affine lTransform,
                                       Vector4 lvLateralAngleMinMaxForwardAngleMinMax,
                                       Vector4 lvVelocityMinMaxInheritanceMinMax,
                                       Vector3 lvVelocityToInherit,
                                       Vector4 lvSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ,
                                       f32 lfCurrentTime,
                                       f32 lfGroundPositionY,
                                       f32 lfVelocityScaleSpeedThreshold,
                                       f32 lfReflectionAmount,
                                       u32 luNumToSpawn,
                                       Native::ESparkArrayID leSparkType);
        void FireDebrisBurst(Vector3 lvSpawnPosition,
                             Vector3 lvCameraPosition,
                             Vector3 lvEmitterHalfExtents,
                             Vector3 lvVelocityToInherit,
                             f32 lfCurrentTime,
                             f32 lfScaleFactor,
                             const Attrib::Gen::debrisparams& lrDebrisParams,
                             Vector4 lvCarColour);
    };

#include "fxcrashvfx_prc_producers.inc"
}

namespace BrnReplays
{
#include "fxcrashvfx_prc_layout.inc"
}

namespace BrnEffects
{
namespace Utils
{
#include "fxcrashvfx_prc_randomisers.inc"
}
#include "fxcrashvfx_prc_burst.inc"

    struct SparkShowerController;
    typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;
    typedef ::EActiveRaceCarIndex             EActiveRaceCarIndex;

    // The active-race-car interface: the eight RaceCarStates, the eight colours, and the model-id calls the
    // takedown coin's winner is announced with.
    struct FakeActiveRaceCars
    {
        RaceCarState*    mpStates;
        RwRGBAReal       maColours[8];
        mutable std::vector<u32> mModelIdCalls;
        const RaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const
        {
            CGS_ASSERT(leIndex >= 0 && leIndex < 8, "leActiveRaceCarIndex in range");
            return &mpStates[leIndex];
        }
        RwRGBAReal GetRaceCarColour(EActiveRaceCarIndex leIndex) const { return maColours[leIndex]; }
        u32 GetCarModelId(EActiveRaceCarIndex leIndex) const
        {
            mModelIdCalls.push_back(static_cast<u32>(leIndex));
            return 0u;
        }
    };
    typedef FakeActiveRaceCars RCEntityActiveRaceCarOutputInterface;

    struct EffectsModuleParams
    {
        f32     mDt;
        f32     mTime;
        u8      mPad08[8];
        Vector3 mCameraPosition;
    };

    struct FakeSurfaceList
    {
        void* Surfaces(u32 luIndex) const
        {
            return luIndex < KU_NUM_FAKE_SURFACES ? &gaSurfaceRefs[luIndex] : nullptr;
        }
    };

    struct FakeEffectsSerialiser
    {
        BrnReplays::BaseSerialiser::EMode meMode;
        u8*                               mpLayout;
        BrnReplays::BaseSerialiser::EMode GetMode() const { return meMode; }
        BrnReplays::EffectsSerialiserStaticLayout* GetStaticLayout() const
        {
            return reinterpret_cast<BrnReplays::EffectsSerialiserStaticLayout*>(mpLayout);
        }
    };

    namespace
    {
        // The parent revision's drain announced itself instead of running; its announcement lands here.
        inline void LogNotReconstructed(bool& lrbLogged, const char*)
        {
            lrbLogged = true;
            ++gNotReconstructed;
        }
    }

#include "fxcrashvfx_prc_structs.inc"

    namespace
    {
#include "fxcrashvfx_prc_consts.inc"
    }

    class EffectsModule
    {
    public:
        static const u32 KU_NUM_ACTIVE_RACE_CARS = 8;

        BrnParticle::ParticleModule                     mParticleModule;
        CgsNumeric::Random                              mRandom;
        ActiveRaceCarData*                              maActiveRaceCarData;
        f32                                             mafAccumulatedParticleCountCrash[9];
        f32                                             mafTimeUntilNextDebrisBurst[8];
        f32                                             mafTimeUntilNextSparksBurst[8];
        BrnGameState::GameStateModuleIO::EGameModeType  meCurrentGameMode;
        Attrib::Gen::sparkeffect                        mSparkParams[4];
        Attrib::Gen::debrisparams                       mCrashingDebrisParams;
        Attrib::Gen::debrisparams                       mRoadRageDebrisParams;
        FakeSurfaceList                                 mSurfaceList;
        FakeEffectsSerialiser                           mEffectsSerialiser;

        void DoSparkShower(const SparkShowerController& lrController,
                           VecFloat lvSize,
                           Matrix44Affine lTransform,
                           Vector3 lvVelocityToInherit,
                           f32 lfCurrentTime,
                           f32 lfGroundPositionY,
                           u32 luNumToSpawn);
        void HandleRaceCarRaceCarSparks(f32 lfDt,
                                        f32 lfTime,
                                        Vector3 lvPosition,
                                        Vector3 lvNormal,
                                        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                                        f32 lfGroundPositionY);
        void HandleVehicleVehicleSparks(Vector3 lvPosition,
                                        Vector3 lvOtherVelocity,
                                        ActiveRaceCarData& lrActiveRaceCar,
                                        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                                        f32 lfDt,
                                        f32 lfTime);
        void HandleBurstDebris(const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                               EActiveRaceCarIndex leIndex,
                               f32 lfDt,
                               f32 lfTime,
                               Vector3 lvPosition,
                               Vector3 lvNormal,
                               const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                               const BrnDirector::Camera::Camera* lpCamera,
                               const Attrib::Gen::debrisparams& lrDebrisParams,
                               const RwRGBAReal& lrColour);
        void ProcessRaceCarContacts(const BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue* lpQueue,
                                    const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                    const EffectsModuleParams& lrParams,
                                    const BrnDirector::Camera::Camera* lpCamera);
    };

#include "fxcrashvfx_prc_body.inc"
}

// ---- comparison helpers ---------------------------------------------------------------------------------------
static Vector3 VectorFromBits(const u32 lau[4])
{
    Vector3 lv;
    lv.x = Float(lau[0]); lv.y = Float(lau[1]); lv.z = Float(lau[2]); lv.w = Float(lau[3]);
    return lv;
}

static bool SameBits(const void* lpValue, const u32* lpau, u32 luWords)
{
    for (u32 i = 0; i < luWords; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        if (lu != lpau[i])
            return false;
    }
    return true;
}

static void PrintVector(const char* lpcLabel, const void* lpValue, const u32* lpauWant, u32 luWords)
{
    std::printf("        %s got:", lpcLabel);
    for (u32 i = 0; i < luWords; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        std::printf(" %08X", lu);
    }
    std::printf("\n        %s want:", lpcLabel);
    for (u32 i = 0; i < luWords; ++i)
        std::printf(" %08X", lpauWant[i]);
    std::printf("\n");
}

// One posted record against the console's, field by field (the host records are the same field lists; the
// debris record's debrisparams is compared by WHICH parameter block it carries).
static bool SameRecord(const BrnParticle::PostedRecord& lrGot, const PrcEvent& lrWant, bool lbVerbose)
{
    if (static_cast<u32>(lrGot.miType) != lrWant.muType)
    {
        if (lbVerbose)
            std::printf("        type got %d want %u\n", lrGot.miType, lrWant.muType);
        return false;
    }
    bool lbSame = true;
    if (lrWant.muType == 1u)
    {
        const BrnParticle::SpawnSparksFromPointEvent& lr =
            *reinterpret_cast<const BrnParticle::SpawnSparksFromPointEvent*>(lrGot.mau8Bytes);
        const f32 laf[5] = { lr.mfCurrentTime, lr.mfNumSparks, lr.mfHeightAboveGround, lr.mfVelocityInheritMin,
                             lr.mfVelocityInheritMax };
        lbSame = lrGot.miSize == 0x50 && SameBits(&lr.mvWorldSpacePoint, lrWant.maA, 4)
              && SameBits(&lr.mvVelocity, lrWant.maB, 4) && SameBits(&lr.mvNormal, lrWant.maC, 4)
              && static_cast<u32>(lr.meSparkType) == lrWant.muSpark && SameBits(laf, lrWant.maF, 5)
              && (lr.mbIsCrashRelated ? 1u : 0u) == lrWant.muFlag;
    }
    else if (lrWant.muType == 2u)
    {
        const BrnParticle::SpawnSparkShowerFromPointEvent& lr =
            *reinterpret_cast<const BrnParticle::SpawnSparkShowerFromPointEvent*>(lrGot.mau8Bytes);
        const f32 laf[4] = { lr.mfCurrentTime, lr.mfGroundPositionY, lr.mfVelocityScaleSpeedThreshold,
                             lr.mfReflectionAmount };
        lbSame = lrGot.miSize == 0xA0 && SameBits(&lr.mTransform, lrWant.maA, 16)
              && SameBits(&lr.mLateralAngleMinMaxForwardAngleMinMax, lrWant.maB, 4)
              && SameBits(&lr.mVelocityMinMaxInheritanceMinMax, lrWant.maC, 4)
              && SameBits(&lr.mVelocityToInherit, lrWant.maD, 4)
              && SameBits(&lr.mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ, lrWant.maE, 4)
              && SameBits(laf, lrWant.maF, 4) && lr.muNumToSpawn == lrWant.muCount
              && static_cast<u32>(lr.meSparkType) == lrWant.muSpark;
        if (!lbSame && lbVerbose)
        {
            PrintVector("transform", &lr.mTransform, lrWant.maA, 16);
            PrintVector("lateral  ", &lr.mLateralAngleMinMaxForwardAngleMinMax, lrWant.maB, 4);
            PrintVector("velocity ", &lr.mVelocityMinMaxInheritanceMinMax, lrWant.maC, 4);
            PrintVector("inherit  ", &lr.mVelocityToInherit, lrWant.maD, 4);
            PrintVector("size     ", &lr.mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ, lrWant.maE, 4);
            PrintVector("floats   ", laf, lrWant.maF, 4);
            std::printf("        count got %u want %u, spark got %u want %u, size %d\n", lr.muNumToSpawn,
                        lrWant.muCount, static_cast<u32>(lr.meSparkType), lrWant.muSpark, lrGot.miSize);
        }
    }
    else if (lrWant.muType == 5u)
    {
        const BrnParticle::FireDebrisBurstEvent& lr =
            *reinterpret_cast<const BrnParticle::FireDebrisBurstEvent*>(lrGot.mau8Bytes);
        const void* const lpLayout = ((const Attrib::Instance&)lr.mDebrisParams).GetLayoutPointer();
        const u32 luParams = lpLayout == gaCrashDebrisLayout ? 1u : lpLayout == gaRoadRageDebrisLayout ? 2u : 0u;
        const f32 laf[2] = { lr.mfCurrentTime, lr.mfScaleFactor };
        lbSame = lrGot.miSize == static_cast<s32>(sizeof(BrnParticle::FireDebrisBurstEvent))
              && SameBits(&lr.mSpawnPosition, lrWant.maA, 4) && SameBits(&lr.mvEmitterHalfExtents, lrWant.maB, 4)
              && SameBits(&lr.mvVelocityToInherit, lrWant.maC, 4) && SameBits(&lr.mCameraPosition, lrWant.maD, 4)
              && SameBits(&lr.mvCarColour, lrWant.maE, 4) && SameBits(laf, lrWant.maF, 2)
              && luParams == lrWant.muFlag;
        if (!lbSame && lbVerbose)
        {
            PrintVector("spawn    ", &lr.mSpawnPosition, lrWant.maA, 4);
            PrintVector("half     ", &lr.mvEmitterHalfExtents, lrWant.maB, 4);
            PrintVector("velocity ", &lr.mvVelocityToInherit, lrWant.maC, 4);
            PrintVector("camera   ", &lr.mCameraPosition, lrWant.maD, 4);
            PrintVector("colour   ", &lr.mvCarColour, lrWant.maE, 4);
            PrintVector("floats   ", laf, lrWant.maF, 2);
            std::printf("        params got %u want %u\n", luParams, lrWant.muFlag);
        }
    }
    else
    {
        lbSame = false;
    }
    return lbSame;
}

int main()
{
    typedef BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue Queue;
    Queue* lpQueue = new Queue();

    void* lpStateStorage = std::calloc(8, sizeof(BrnPhysics::Vehicle::RaceCarState));
    alignas(16) static unsigned char saCarDataStorage[8 * sizeof(BrnEffects::ActiveRaceCarData)];
    alignas(16) static unsigned char saLayout[0x1300];

    for (u32 i = 0; i < KU_NUM_FAKE_SURFACES; ++i)
        std::memset(&gaSurfaceRefs[i], 0, sizeof(gaSurfaceRefs[i]));

    unsigned luTotalEvents = 0, luTotalSpawns = 0;
    const u32 luNumCases = static_cast<u32>(sizeof(kaPrcCases) / sizeof(kaPrcCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const PrcCase& lrCase = kaPrcCases[luCase];
        char lacLabel[320];

        // ---- the module ----
        BrnEffects::EffectsModule* lpModule = new BrnEffects::EffectsModule();
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;

        std::memset(saCarDataStorage, 0, sizeof(saCarDataStorage));
        lpModule->maActiveRaceCarData = reinterpret_cast<BrnEffects::ActiveRaceCarData*>(saCarDataStorage);
        static_assert(sizeof(BrnEffects::BurstAccumulator) == 0x18, "six f32");
        for (u32 i = 0; i < 8; ++i)
        {
            lpModule->maActiveRaceCarData[i].mfGroundPositionY = Float(lrCase.maGround[i]);
            std::memcpy(&lpModule->maActiveRaceCarData[i].GetBurstAccumulatorWorldGrinding(), lrCase.maBurst[i],
                        sizeof(BrnEffects::BurstAccumulator));
        }
        for (u32 i = 0; i < 9; ++i)
            lpModule->mafAccumulatedParticleCountCrash[i] = Float(lrCase.maCrashAcc[i]);
        for (u32 i = 0; i < 8; ++i)
        {
            lpModule->mafTimeUntilNextDebrisBurst[i] = Float(lrCase.maDebrisT[i]);
            lpModule->mafTimeUntilNextSparksBurst[i] = Float(lrCase.maSparksT[i]);
        }
        lpModule->meCurrentGameMode = static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(lrCase.miGameMode);

        std::memset(gaSparkLayout, 0, sizeof(gaSparkLayout));
        std::memcpy(&gaSparkLayout[0x54], &lrCase.maInherit[0], 4);
        std::memcpy(&gaSparkLayout[0x58], &lrCase.maInherit[1], 4);
        lpModule->mSparkParams[1].mpAttributeData = gaSparkLayout;

        const PrcDebris* const lapDebris[2] = { &lrCase.mCrashDebris, &lrCase.mRoadRageDebris };
        unsigned char* const lapDebrisLayout[2] = { gaCrashDebrisLayout, gaRoadRageDebrisLayout };
        for (u32 k = 0; k < 2; ++k)
        {
            unsigned char* lp = lapDebrisLayout[k];
            const PrcDebris& lr = *lapDebris[k];
            std::memset(lp, 0, 0x100);
            std::memcpy(lp + 0x00, lr.maHalf, 16);
            std::memcpy(lp + 0x84, &lr.muInterval, 4);
            std::memcpy(lp + 0xB8, &lr.muScaleMin, 4);
            std::memcpy(lp + 0xBC, &lr.muSpeedMin, 4);
            std::memcpy(lp + 0xC0, &lr.muScaleMid, 4);
            std::memcpy(lp + 0xC4, &lr.muSpeedMid, 4);
            std::memcpy(lp + 0xC8, &lr.muScaleMax, 4);
            std::memcpy(lp + 0xCC, &lr.muSpeedMax, 4);
        }
        lpModule->mCrashingDebrisParams.mpAttributeData = gaCrashDebrisLayout;
        lpModule->mRoadRageDebrisParams.mpAttributeData = gaRoadRageDebrisLayout;

        for (u32 i = 0; i < KU_NUM_FAKE_SURFACES; ++i)
        {
            std::memset(gaSurfaceLayouts[i], 0, sizeof(gaSurfaceLayouts[i]));
            std::memset(gaVfxLayouts[i], 0, sizeof(gaVfxLayouts[i]));
        }
        for (u32 k = 0; k < lrCase.muNumSurfaces; ++k)
        {
            const PrcSurface& lr = lrCase.mpSurfaces[k];
            gaVfxLayouts[lr.muId][0x4F] = lr.mu8Sparks;
            std::memcpy(&gaVfxLayouts[lr.muId][0x50], &lr.muGrindScale, 4);
            std::memcpy(&gaVfxLayouts[lr.muId][0x54], &lr.muSparkScale, 4);
        }

        std::memset(saLayout, 0, sizeof(saLayout));
        *reinterpret_cast<s32*>(saLayout + 0x34) = static_cast<s32>(lrCase.muNumTable);
        for (u32 n = 0; n < lrCase.muNumTable; ++n)
        {
            const PrcTable& lr = lrCase.mpTable[n];
            std::memcpy(saLayout + 0xD30 + 4 * n, &lr.muA, 4);
            std::memcpy(saLayout + 0xDB0 + 4 * n, &lr.muB, 4);
            std::memcpy(saLayout + 0xE30 + 4 * n, &lr.muTag, 4);
            std::memcpy(saLayout + 0xEB0 + 16 * n, lr.maNormal, 16);
            std::memcpy(saLayout + 0x10B0 + 16 * n, lr.maPoint, 16);
        }
        lpModule->mEffectsSerialiser.meMode   = static_cast<BrnReplays::BaseSerialiser::EMode>(lrCase.miReplayMode);
        lpModule->mEffectsSerialiser.mpLayout = saLayout;

        // ---- the cars, the camera, the contacts ----
        BrnEffects::FakeActiveRaceCars lCars;
        std::memset(lpStateStorage, 0, 8 * sizeof(BrnPhysics::Vehicle::RaceCarState));
        lCars.mpStates = static_cast<BrnPhysics::Vehicle::RaceCarState*>(lpStateStorage);
        for (u32 i = 0; i < 8; ++i)
        {
            const PrcCar& lr = lrCase.maCars[i];
            lCars.mpStates[i].mLinearVelocity = VectorFromBits(lr.maVelocity);
            lCars.mpStates[i].mbCrashing      = lr.mu8Crashing != 0;
            lCars.mpStates[i].mbIsHidden      = lr.mu8Hidden != 0;
            lCars.maColours[i].red   = Float(lr.maColour[0]);
            lCars.maColours[i].green = Float(lr.maColour[1]);
            lCars.maColours[i].blue  = Float(lr.maColour[2]);
            lCars.maColours[i].alpha = Float(lr.maColour[3]);
        }
        BrnDirector::Camera::Camera lCamera;
        std::memset(&lCamera, 0, sizeof(lCamera));
        lCamera.mTransform.wAxis = VectorFromBits(lrCase.maCamera);

        lpQueue->Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
        for (u32 n = 0; n < lrCase.muNumContacts; ++n)
        {
            const PrcContact& lr = lrCase.mpContacts[n];
            BrnPhysics::ContactSpy::RaceCarContact lContact;
            std::memset(&lContact, 0, sizeof(lContact));
            lContact.mEntityIdA.muValue     = lr.muA;
            lContact.mEntityIdB.muValue     = lr.muB;
            lContact.mCollisionTagB.muValue = lr.muTag;
            lContact.mFrictionStress = VectorFromBits(lr.maFriction);
            lContact.mNormalStress   = VectorFromBits(lr.maNStress);
            lContact.mNormal         = VectorFromBits(lr.maNormal);
            lContact.mPointOnA       = VectorFromBits(lr.maPointA);
            lContact.mPointOnB       = VectorFromBits(lr.maPointB);
            lpQueue->AddEvent(lContact);
        }

        BrnEffects::EffectsModuleParams lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.mDt   = Float(lrCase.muDt);
        lParams.mTime = Float(lrCase.muTime);

        const unsigned luAssertsBefore = gAsserts;
        lpModule->ProcessRaceCarContacts(lpQueue, &lCars, lParams, &lCamera);

        // ---- 1. the posted records ----
        const std::vector<BrnParticle::PostedRecord>& lrPosted = lpModule->mParticleModule.mInterThreadEventQueue.mRecords;
        u32 luSameRecords = 0;
        for (u32 n = 0; n < lrCase.muNumEvents && n < lrPosted.size(); ++n)
        {
            const bool lbSame = SameRecord(lrPosted[n], lrCase.mpEvents[n], false);
            if (!lbSame)
            {
                std::printf("      record %u differs:\n", n);
                SameRecord(lrPosted[n], lrCase.mpEvents[n], true);
            }
            luSameRecords += lbSame ? 1u : 0u;
        }
        luTotalEvents += lrCase.muNumEvents;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u (%s): every posted record is the console's, field by field: %u/%u (posted %u)",
                      luCase, lrCase.mpcName, luSameRecords, lrCase.muNumEvents, static_cast<unsigned>(lrPosted.size()));
        Check(luSameRecords == lrCase.muNumEvents && lrPosted.size() == lrCase.muNumEvents, lacLabel);

        // ---- 2. the crash impact dust ----
        const std::vector<BrnParticle::SimpleSpawnRecord>& lrSpawns = lpModule->mParticleModule.mSpawns;
        u32 luSameSpawns = 0;
        for (u32 n = 0; n < lrCase.muNumSpawns && n < lrSpawns.size(); ++n)
        {
            const PrcSpawn& lrWant = lrCase.mpSpawns[n];
            const BrnParticle::SimpleSpawnRecord& lrGot = lrSpawns[n];
            const bool lbSame = lrGot.muType == lrWant.muType && SameBits(&lrGot.mvPosition, lrWant.maPos, 4)
                             && SameBits(&lrGot.mvVelocity, lrWant.maVel, 4) && Bits(lrGot.mfSize) == lrWant.muSize
                             && Bits(lrGot.mfTime) == lrWant.muTime && Bits(lrGot.mfAlpha) == lrWant.muAlpha;
            if (!lbSame)
            {
                std::printf("      spawn %u differs (type %u size %08X want %08X):\n", n, lrGot.muType,
                            Bits(lrGot.mfSize), lrWant.muSize);
                PrintVector("position", &lrGot.mvPosition, lrWant.maPos, 4);
                PrintVector("velocity", &lrGot.mvVelocity, lrWant.maVel, 4);
            }
            luSameSpawns += lbSame ? 1u : 0u;
        }
        luTotalSpawns += lrCase.muNumSpawns;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: every crash-impact-dust SpawnSimple is the console's (type 2, position, fused "
                      "velocity, size, time, alpha): %u/%u (spawned %u)",
                      luCase, luSameSpawns, lrCase.muNumSpawns, static_cast<unsigned>(lrSpawns.size()));
        Check(luSameSpawns == lrCase.muNumSpawns && lrSpawns.size() == lrCase.muNumSpawns, lacLabel);

        // ---- 3. which car the takedown coin picked ----
        bool lbModelIds = lCars.mModelIdCalls.size() == lrCase.muNumModelIds;
        for (u32 n = 0; lbModelIds && n < lrCase.muNumModelIds; ++n)
            lbModelIds = lCars.mModelIdCalls[n] == lrCase.mpModelIds[n];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the takedown-debris car sequence (GetCarModelId calls) is the console's (%u calls)",
                      luCase, lrCase.muNumModelIds);
        Check(lbModelIds, lacLabel);

        // ---- 4. the eight burst accumulators ----
        u32 luSameBursts = 0;
        for (u32 i = 0; i < 8; ++i)
            luSameBursts += SameBits(&lpModule->maActiveRaceCarData[i].GetBurstAccumulatorWorldGrinding(),
                                     lrCase.maBurstOut[i], 6) ? 1u : 0u;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the eight grinding BurstAccumulators after the call are the console's: %u/8",
                      luCase, luSameBursts);
        Check(luSameBursts == 8, lacLabel);

        // ---- 5. the dust accumulators and both burst timers ----
        const bool lbTimers = SameBits(lpModule->mafAccumulatedParticleCountCrash, lrCase.maCrashAccOut, 9)
                           && SameBits(lpModule->mafTimeUntilNextDebrisBurst, lrCase.maDebrisTOut, 8)
                           && SameBits(lpModule->mafTimeUntilNextSparksBurst, lrCase.maSparksTOut, 8);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the nine crash-dust accumulators and the debris / sparks burst timers are the "
                      "console's bits", luCase);
        Check(lbTimers, lacLabel);

        // ---- 6. the random stream ----
        bool lbRing = lpModule->mRandom.muSeed == lrCase.muSeedOut
                   && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: mRandom's ring, seed and cursor after the call are the console's (every draw, in "
                      "order)", luCase);
        Check(lbRing, lacLabel);

        // ---- 7. the replay car-contact table ----
        bool lbTable = *reinterpret_cast<const s32*>(saLayout + 0x34) == static_cast<s32>(lrCase.muTableCountOut);
        for (u32 n = 0; lbTable && n < lrCase.muTableCountOut; ++n)
        {
            const PrcTable& lr = lrCase.mpTableOut[n];
            lbTable = SameBits(saLayout + 0xD30 + 4 * n, &lr.muA, 1) && SameBits(saLayout + 0xDB0 + 4 * n, &lr.muB, 1)
                   && SameBits(saLayout + 0xE30 + 4 * n, &lr.muTag, 1)
                   && SameBits(saLayout + 0xEB0 + 16 * n, lr.maNormal, 4)
                   && SameBits(saLayout + 0x10B0 + 16 * n, lr.maPoint, 4);
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: the replay car-contact table after the call is the console's (count %u)", luCase,
                      lrCase.muTableCountOut);
        Check(lbTable, lacLabel);

        // ---- 8. the asserts ----
        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: asserts fired %u (console %u)", luCase,
                      gAsserts - luAssertsBefore, lrCase.muAsserts);
        Check(gAsserts - luAssertsBefore == lrCase.muAsserts, lacLabel);

        delete lpModule;
    }

    Check(gNotReconstructed == 0, "the drain runs: no NOT RECONSTRUCTED announcement from ProcessRaceCarContacts");
    Check(gDrainLines == 0, "the [racecar-contact] witness is default-off (BRN_SIMPLEFX_DIAG unset)");
    std::printf("(%u console records and %u console dust spawns compared)\n", luTotalEvents, luTotalSpawns);

    std::free(lpStateStorage);
    delete lpQueue;
    std::printf("FxCrashVfxRaceCarContacts: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
