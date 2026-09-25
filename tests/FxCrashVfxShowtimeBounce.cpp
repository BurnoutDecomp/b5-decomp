// FX-CRASHVFX (crash parity 2026-09-25, item 5): EffectsModule::HandleShowtimeTrafficBounce @0x82292808 -- a Showtime
// bounce on a car bursts the player's wreck where it hit: the car's debris (FireDebrisBurst), a pane of glass debris
// (BurstAreaEmitParticles @0x82292160), a spark shower (DoSparkShower) and the 'ExploShort' LION effect.
//
// run_fxcrashvfx_showtime_bounce.py extracts the PRODUCTION code -- the bounce region of EffectsModule.cpp (its
// literals, witness and the handler), BurstAreaEmitParticles and its KF_BURST_AREA_* literals, the controller
// structs and gSparkShowerControllerShowtimeBounce, the shared idioms (Dot3, Vnmsub, RefinedRsqrt, RefinedRecip,
// GuardedLength3, Scale4, CrossPermuted, AxisX / AxisY, Add4 / Sub4 / Mul4 / MaddSplat4 / MakeVector3,
// RandomUnitVector, Splat, DebrisParamsLayout, IsReplayPlayback / IsReplayRecording), and from BrnEffectsUtils.cpp the
// TrigBaseFunctions5 sin / cos and the Vector3Randomiser bodies -- and compiles it onto this fixture: the real
// CgsNumeric::Random, ParticleDescription::HashString, TimerStatusInterface, JustBouncedAction, LionEffect (with its
// setters) and debrisparams Instance; stand-ins for the input buffer, the camera, the active race car interface, the
// replay serialiser and the particle module (FireDebrisBurst / SpawnDebris / the three LION calls recorded), and a
// recording DoSparkShower.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxShowtimeBounceData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_bounce_data.py, which runs 0x82292808 -- and the real
// BurstAreaEmitParticles, RandomiseXYZ and the inlined draws -- on emu64 with the CRT-initialised .bss filled by the
// image's own thunks. Nine checks per case: the debris burst, the glass pieces (count, the first three in full, all
// of them through a digest), the spark shower (the controller's words too), the LION stop / start calls, the new
// effect's slot, the module's handles / next slot / last time, the replay layout, and mRandom with the assert count
// and the player index asked for. A console NaN matches any NaN (the x86 default NaN carries the other sign).
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameSource/AttribSys/Generated/classes/debrisparams.h"
#include "GameSource/Effects/BrnEffectsUtils.h"
#include "GameSource/Effects/Particles/BrnParticleDescription.h"
#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "rw/core/base/ostypes.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "FxCrashVfxShowtimeBounceData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gBounceLines = 0, gNotReconstructed = 0;

// A failure prints its whole label; a pass only its first 72 characters (the runner keeps the output's tail, and 144
// full labels would push an early failure out of it).
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
        std::printf("pass  %.72s\n", lpcLabel);
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool SameWord(u32 luGot, u32 luWant) { return luGot == luWant || (IsNanBits(luWant) && IsNanBits(luGot)); }

static bool SameWords(const void* lpValue, const u32* lpau, u32 luCount)
{
    for (u32 i = 0; i < luCount; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        if (!SameWord(lu, lpau[i]))
            return false;
    }
    return true;
}

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
        if (lpcText != nullptr && std::strncmp(lpcText, "[bounce-vfx]", 12) == 0)
            ++gBounceLines;
    }
}
}

namespace BrnPhysics
{
namespace Vehicle
{
    // The one member the handler reads (BrnVehicleEvents.h @816), by name.
    struct alignas(16) RaceCarState
    {
        Vector3 mLinearVelocity;
    };
}
}

namespace BrnDirector
{
namespace Camera
{
    // The camera as the handler reads it: its transform's position row (camera + 0x30).
    class Camera
    {
    public:
        Matrix44Affine mTransform;
        const Matrix44Affine& GetTransform() const { return mTransform; }
    };
}
}

// ---- the recordings ------------------------------------------------------------------------------------------------
struct FireRecord
{
    Vector3 mvSpawn, mvCamera, mvHalfExtents, mvVelocity;
    Vector4 mvColour;
    f32     mfTime, mfScale;
    const void* mpParams;
};

struct SpawnRecord
{
    u32     muType;
    Vector3 mvPosition, mvVelocity, mvAxis;
    Vector4 mvColour;
    f32     mfSize, mfTime;
};

struct ShowerRecord
{
    u32            mauController[27];
    Vector4        mvSize;
    Matrix44Affine mTransform;
    Vector3        mvVelocity;
    f32            mfTime, mfGround;
    u32            muCount;
};

struct StartRecord
{
    u32         muHash, muWorld;
    std::string mName;
};

// The particle module as the handler and BurstAreaEmitParticles use it. The LION slots answer as the real
// GetLionEffect @0x82278380 does (the slot of the handle's low seven bits, if its stored handle still matches);
// StopLionEffect and StartLionEffect are recorded, Start answering the case's handle.
struct FixtureParticles
{
    std::vector<FireRecord>  mFires;
    std::vector<SpawnRecord> mSpawns;
    std::vector<u32>         mStops;
    std::vector<StartRecord> mStarts;
    u32                      muStartHandle;
    BrnParticle::LionEffect  maSlots[128];

    void FireDebrisBurst(Vector3 lvSpawnPosition, Vector3 lvCameraPosition, Vector3 lvEmitterHalfExtents,
                         Vector3 lvVelocityToInherit, f32 lfCurrentTime, f32 lfScaleFactor,
                         const Attrib::Gen::debrisparams& lrDebrisParams, Vector4 lvCarColour)
    {
        FireRecord lRecord;
        lRecord.mvSpawn = lvSpawnPosition;
        lRecord.mvCamera = lvCameraPosition;
        lRecord.mvHalfExtents = lvEmitterHalfExtents;
        lRecord.mvVelocity = lvVelocityToInherit;
        lRecord.mvColour = lvCarColour;
        lRecord.mfTime = lfCurrentTime;
        lRecord.mfScale = lfScaleFactor;
        lRecord.mpParams = &lrDebrisParams;
        mFires.push_back(lRecord);
    }

    void SpawnDebris(BrnParticle::Native::EDebrisArrayID leDebrisType, Vector3 lvPosition, Vector3 lvVelocity,
                     Vector3 lvRotationAxis, Vector4 lvColour, f32 lfSize, f32 lfSpawnTime)
    {
        SpawnRecord lRecord;
        lRecord.muType = static_cast<u32>(leDebrisType);
        lRecord.mvPosition = lvPosition;
        lRecord.mvVelocity = lvVelocity;
        lRecord.mvAxis = lvRotationAxis;
        lRecord.mvColour = lvColour;
        lRecord.mfSize = lfSize;
        lRecord.mfTime = lfSpawnTime;
        mSpawns.push_back(lRecord);
    }

    BrnParticle::LionEffect* GetLionEffect(u32 luHandle)
    {
        BrnParticle::LionEffect* lpEffect = &maSlots[luHandle & BrnParticle::LionEffect::KU_HANDLE_INDEX_MASK];
        return (lpEffect->muHandle == luHandle) ? lpEffect : nullptr;
    }

    void StopLionEffect(BrnParticle::LionEffect* lpEffect)
    {
        mStops.push_back(static_cast<u32>(lpEffect - maSlots));
    }

    u32 StartLionEffect(u32 luNameHash, const char* lpcEffectName, u32 luWorldIndex)
    {
        StartRecord lRecord;
        lRecord.muHash = luNameHash;
        lRecord.muWorld = luWorldIndex;
        lRecord.mName = lpcEffectName ? lpcEffectName : "";
        mStarts.push_back(lRecord);
        return muStartHandle;
    }
};

namespace BrnEffects
{
namespace Utils
{
#include "fxcrashvfx_bounce_utils.inc"
}

    // The active race car interface as the handler asks it: the player's index, whether it is active and crashing,
    // its state and paint -- the index each accessor was handed recorded.
    struct RCEntityActiveRaceCarOutputInterface
    {
        bool                                    mbPlayerActive;
        bool                                    mbPlayerCrashing;
        EActiveRaceCarIndex                     mePlayer;
        const BrnPhysics::Vehicle::RaceCarState* mpState;
        RwRGBAReal                              mColour;
        mutable u32                             muStateIndex, muColourIndex;

        bool IsPlayerCarActive() const { return mbPlayerActive; }
        bool IsPlayerCarCrashing() const { return mbPlayerCrashing; }
        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayer; }
        const BrnPhysics::Vehicle::RaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const
        {
            muStateIndex = static_cast<u32>(leIndex);
            return mpState;
        }
        const RwRGBAReal& GetRaceCarColour(EActiveRaceCarIndex leIndex) const
        {
            muColourIndex = static_cast<u32>(leIndex);
            return mColour;
        }
    };

namespace EffectsIO
{
    struct InputBuffer
    {
        const CgsSystem::TimerStatusInterface* mpTimers;
        const BrnDirector::Camera::Camera*     mpCamera;
        const RCEntityActiveRaceCarOutputInterface* mpActiveRaceCars;
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const { return mpTimers; }
        const BrnDirector::Camera::Camera* GetCameraInput() const { return mpCamera; }
        const RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarInterface() const { return mpActiveRaceCars; }
    };
}

    // The effects serialiser as the handler sees it: the mode word and the static layout.
    struct FixtureSerialiser
    {
        BrnReplays::BaseSerialiser::EMode meMode;
        unsigned char*                    mpLayout;
        BrnReplays::BaseSerialiser::EMode GetMode() const { return meMode; }
        BrnReplays::EffectsSerialiserStaticLayout* GetStaticLayout() const
        {
            return reinterpret_cast<BrnReplays::EffectsSerialiserStaticLayout*>(mpLayout);
        }
    };

    namespace
    {
        typedef ::EActiveRaceCarIndex             EActiveRaceCarIndex;
        typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;
        const u32 KU_EFFECTS_DIAG_MAX_LINES = 128u;

        // The parent revision's handler announced itself instead of running; its announcement lands here.
        inline void LogNotReconstructed(bool& lrbLogged, const char*)
        {
            lrbLogged = true;
            ++gNotReconstructed;
        }

#include "fxcrashvfx_bounce_helpers.inc"
    }

    class EffectsModule
    {
    public:
        static const u32 KU_MAX_SHOWTIME_BOUNCE_EFFECTS = 3;

        FixtureParticles             mParticleModule;
        CgsNumeric::Random           mRandom;
        const Attrib::Gen::debrisparams& mCrashingDebrisParams;
        u32                          maShowtimeBounceEffectHandles[KU_MAX_SHOWTIME_BOUNCE_EFFECTS];
        u32                          muNextShowtimeBounceEffect;
        f32                          mfLastShowtimeBounceEffectTime;
        FixtureSerialiser            mEffectsSerialiser;
        std::vector<ShowerRecord>    mShowers;

        explicit EffectsModule(const Attrib::Gen::debrisparams& lrCrashingDebrisParams)
            : mCrashingDebrisParams(lrCrashingDebrisParams) {}

        void HandleShowtimeTrafficBounce(const BrnGameState::GameStateModuleIO::JustBouncedAction* lpJustBouncedAction,
                                         const EffectsIO::InputBuffer* lpEffectsInputBuffer);
        void BurstAreaEmitParticles(Vector3* lpCorners, Vector3 lvNormal, Vector3 lvInheritedVelocity,
                                    BrnParticle::Native::EDebrisArrayID leDebrisType, f32 lfCurrentTime,
                                    f32 lfSizeMin, f32 lfSizeMax, f32 lfParticleDensity);
        void DoSparkShower(const SparkShowerController& lrController, VecFloat lvSize, Matrix44Affine lTransform,
                           Vector3 lvVelocityToInherit, f32 lfCurrentTime, f32 lfGroundPositionY, u32 luNumToSpawn);
    };

    // The shower is recorded at its boundary, as the console's is (DoSparkShower @0x822920C0 is proved by its own
    // test, run_fxcrashvfx_spark_shower.py).
    void EffectsModule::DoSparkShower(const SparkShowerController& lrController, VecFloat lvSize,
                                      Matrix44Affine lTransform, Vector3 lvVelocityToInherit, f32 lfCurrentTime,
                                      f32 lfGroundPositionY, u32 luNumToSpawn)
    {
        ShowerRecord lRecord;
        std::memcpy(lRecord.mauController, &lrController, sizeof(lRecord.mauController));
        lRecord.mvSize = lvSize;
        lRecord.mTransform = lTransform;
        lRecord.mvVelocity = lvVelocityToInherit;
        lRecord.mfTime = lfCurrentTime;
        lRecord.mfGround = lfGroundPositionY;
        lRecord.muCount = luNumToSpawn;
        mShowers.push_back(lRecord);
    }

#include "fxcrashvfx_bounce_burst.inc"
#include "fxcrashvfx_bounce_body.inc"
}

static void LoadVector(Vector3& lrv, const u32 lau[4])
{
    lrv.x = Float(lau[0]); lrv.y = Float(lau[1]); lrv.z = Float(lau[2]); lrv.w = Float(lau[3]);
}

static u64 SpawnDigest(const std::vector<SpawnRecord>& lrSpawns)
{
    u64 lu = 0xCBF29CE484222325ULL;
    for (const SpawnRecord& lrSpawn : lrSpawns)
    {
        u32 lau[19];
        lau[0] = lrSpawn.muType;
        std::memcpy(lau + 1, &lrSpawn.mvPosition, 16);
        std::memcpy(lau + 5, &lrSpawn.mvVelocity, 16);
        std::memcpy(lau + 9, &lrSpawn.mvAxis, 16);
        std::memcpy(lau + 13, &lrSpawn.mvColour, 16);
        lau[17] = Bits(lrSpawn.mfSize);
        lau[18] = Bits(lrSpawn.mfTime);
        for (u32 w : lau)
        {
            if (IsNanBits(w))
                w = 0x7FC00000u;
            lu = (lu ^ w) * 0x100000001B3ULL;
        }
    }
    return lu;
}

int main()
{
    unsigned luPieces = 0;
    const u32 luNumCases = static_cast<u32>(sizeof(kaBounceCases) / sizeof(kaBounceCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const BounceCase& lrCase = kaBounceCases[luCase];
        char lacLabel[400];

        // The debrisparams in raw storage: never constructed (that reaches the AttribSys runtime); its Instance's
        // layout pointer is all the handler reads (the emitter half extents at +0x00).
        alignas(16) static unsigned char saDebrisLayout[0x100];
        std::memset(saDebrisLayout, 0, sizeof(saDebrisLayout));
        std::memcpy(saDebrisLayout, lrCase.mauHalf, 16);
        alignas(16) static unsigned char saParams[sizeof(Attrib::Gen::debrisparams)];
        std::memset(saParams, 0, sizeof(saParams));
        Attrib::Gen::debrisparams* lpParams = reinterpret_cast<Attrib::Gen::debrisparams*>(saParams);
        ((Attrib::Instance&)*lpParams).mpAttributeData = saDebrisLayout;

        BrnEffects::EffectsModule* lpModule = new BrnEffects::EffectsModule(*lpParams);
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.mauRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;
        for (u32 i = 0; i < 3; ++i)
            lpModule->maShowtimeBounceEffectHandles[i] = lrCase.mauHandles[i];
        lpModule->muNextShowtimeBounceEffect     = lrCase.muNext;
        lpModule->mfLastShowtimeBounceEffectTime = Float(lrCase.muLast);
        // The slots as the emulator's: zero-filled, then the case's stamped handles and flags.
        std::memset(lpModule->mParticleModule.maSlots, 0, sizeof(lpModule->mParticleModule.maSlots));
        for (u32 n = 0; n < lrCase.muNumSlots; ++n)
        {
            lpModule->mParticleModule.maSlots[lrCase.mauSlotIndex[n]].muHandle = lrCase.mauSlotHandle[n];
            lpModule->mParticleModule.maSlots[lrCase.mauSlotIndex[n]].muFlags = static_cast<u16>(lrCase.mauSlotFlags[n]);
        }
        lpModule->mParticleModule.muStartHandle = lrCase.muStartHandle;

        alignas(16) static unsigned char saLayout[0x1300];
        std::memset(saLayout, 0, sizeof(saLayout));
        saLayout[7] = static_cast<unsigned char>(lrCase.muLayoutByte7);
        std::memcpy(saLayout + 0x10, lrCase.mauLayoutContact, 16);
        std::memcpy(saLayout + 0x20, lrCase.mauLayoutAngle, 16);
        lpModule->mEffectsSerialiser.meMode   = static_cast<BrnReplays::BaseSerialiser::EMode>(lrCase.muMode);
        lpModule->mEffectsSerialiser.mpLayout = saLayout;

        CgsSystem::TimerStatusInterface lTimers;
        std::memset(&lTimers, 0, sizeof(lTimers));
        lTimers.mSimTimerStatus.mTime.miSeconds  = lrCase.miSeconds;
        lTimers.mSimTimerStatus.mTime.mfFraction = Float(lrCase.muFraction);

        BrnDirector::Camera::Camera lCamera;
        std::memset(&lCamera, 0, sizeof(lCamera));
        LoadVector(lCamera.mTransform.wAxis, lrCase.mauCamera);

        alignas(16) static BrnPhysics::Vehicle::RaceCarState sState;
        LoadVector(sState.mLinearVelocity, lrCase.mauVel);

        BrnEffects::RCEntityActiveRaceCarOutputInterface lActive;
        lActive.mbPlayerActive    = lrCase.muPlayerActive != 0u;
        lActive.mbPlayerCrashing  = lrCase.muCrashing != 0u;
        lActive.mePlayer          = static_cast<EActiveRaceCarIndex>(lrCase.muPlayer);
        lActive.mpState           = &sState;
        lActive.mColour.red   = Float(lrCase.mauColour[0]);
        lActive.mColour.green = Float(lrCase.mauColour[1]);
        lActive.mColour.blue  = Float(lrCase.mauColour[2]);
        lActive.mColour.alpha = Float(lrCase.mauColour[3]);
        lActive.muStateIndex = lActive.muColourIndex = 0xFFFFFFFFu;

        BrnEffects::EffectsIO::InputBuffer lInput;
        lInput.mpTimers = &lTimers;
        lInput.mpCamera = &lCamera;
        lInput.mpActiveRaceCars = &lActive;

        alignas(16) BrnGameState::GameStateModuleIO::JustBouncedAction lAction;
        std::memset(&lAction, 0, sizeof(lAction));
        LoadVector(lAction.mContactPoint, lrCase.mauContact);
        lAction.mbOnCar        = lrCase.muOnCar != 0u;
        lAction.mu8EventByte7  = static_cast<u8>(lrCase.muGood);

        const unsigned luAssertsBefore = gAsserts;
        lpModule->HandleShowtimeTrafficBounce(lrCase.muHasAction ? &lAction : nullptr, &lInput);
        const FixtureParticles& lrParticles = lpModule->mParticleModule;

        // 1. the car's debris burst.
        bool lbFire = lrParticles.mFires.size() == lrCase.muNumFires;
        if (lbFire && lrCase.muNumFires == 1u)
        {
            const FireRecord& lrFire = lrParticles.mFires[0];
            lbFire = SameWords(&lrFire.mvSpawn, lrCase.mauFireVecs[0], 4) && SameWords(&lrFire.mvCamera, lrCase.mauFireVecs[1], 4)
                  && SameWords(&lrFire.mvHalfExtents, lrCase.mauFireVecs[2], 4)
                  && SameWords(&lrFire.mvVelocity, lrCase.mauFireVecs[3], 4)
                  && SameWords(&lrFire.mvColour, lrCase.mauFireVecs[4], 4)
                  && SameWord(Bits(lrFire.mfTime), lrCase.muFireTime) && SameWord(Bits(lrFire.mfScale), lrCase.muFireScale)
                  && lrFire.mpParams == &lpModule->mCrashingDebrisParams && lrCase.muFireParamsOffset == 0x2D388u;
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] FireDebrisBurst x%u: the point, camera, half extents, velocity, "
                      "paint, time, scale 1.0, the player's crash debrisparams", lrCase.mpcName, lrCase.muNumFires);
        Check(lbFire, lacLabel);

        // 2 / 3. BurstAreaEmitParticles' glass pieces.
        const std::vector<SpawnRecord>& lrSpawns = lrParticles.mSpawns;
        bool lbFirst = lrSpawns.size() == lrCase.muNumSpawns;
        const u32 luFull = (lrCase.muNumSpawns < 3u) ? lrCase.muNumSpawns : 3u;
        for (u32 n = 0; lbFirst && n < luFull; ++n)
        {
            const BounceSpawnOut& lrWant = lrCase.mpSpawns[n];
            const SpawnRecord& lrHave = lrSpawns[n];
            lbFirst = lrHave.muType == lrWant.muType && SameWords(&lrHave.mvPosition, lrWant.mauPos, 4)
                   && SameWords(&lrHave.mvVelocity, lrWant.mauVel, 4) && SameWords(&lrHave.mvAxis, lrWant.mauAxis, 4)
                   && SameWords(&lrHave.mvColour, lrWant.mauColour, 4) && SameWord(Bits(lrHave.mfSize), lrWant.muSize)
                   && SameWord(Bits(lrHave.mfTime), lrWant.muTime);
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] BurstAreaEmitParticles: %u glass pieces (got %u), the first %u "
                      "in full", lrCase.mpcName, lrCase.muNumSpawns, static_cast<unsigned>(lrSpawns.size()), luFull);
        Check(lbFirst, lacLabel);
        const u64 luDigest = SpawnDigest(lrSpawns);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] every glass piece, in order (digest %016llX, the console's "
                      "%016llX)", lrCase.mpcName, static_cast<unsigned long long>(luDigest),
                      static_cast<unsigned long long>(lrCase.muSpawnDigest));
        Check(lrSpawns.size() == lrCase.muNumSpawns && luDigest == lrCase.muSpawnDigest, lacLabel);
        luPieces += static_cast<unsigned>(lrSpawns.size());

        // 4. the spark shower.
        bool lbShower = lpModule->mShowers.size() == lrCase.muNumShowers;
        if (lbShower && lrCase.muNumShowers == 1u)
        {
            const ShowerRecord& lrShower = lpModule->mShowers[0];
            lbShower = SameWords(lrShower.mauController, lrCase.mauShowerController, 27)
                    && SameWords(&lrShower.mvSize, lrCase.mauShowerSize, 4)
                    && SameWords(&lrShower.mTransform, lrCase.mauShowerMatrix, 16)
                    && SameWords(&lrShower.mvVelocity, lrCase.mauShowerVel, 4)
                    && SameWord(Bits(lrShower.mfTime), lrCase.muShowerTime)
                    && SameWord(Bits(lrShower.mfGround), lrCase.muShowerGround) && lrShower.muCount == lrCase.muShowerCount;
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] DoSparkShower x%u: gSparkShowerControllerShowtimeBounce's words, "
                      "the size lerp, the contact frame, velocity, time, ground, %u sparks", lrCase.mpcName,
                      lrCase.muNumShowers, lrCase.muShowerCount);
        Check(lbShower, lacLabel);

        // 5. the LION calls.
        bool lbLion = lrParticles.mStops.size() == lrCase.muNumStops && lrParticles.mStarts.size() == lrCase.muNumStarts;
        for (u32 n = 0; lbLion && n < lrCase.muNumStops; ++n)
            lbLion = lrParticles.mStops[n] == lrCase.mauStopSlot[n];
        if (lbLion && lrCase.muNumStarts == 1u)
            lbLion = lrParticles.mStarts[0].muHash == lrCase.muStartHash && lrParticles.mStarts[0].muWorld == lrCase.muStartWorld
                  && lrParticles.mStarts[0].mName == lrCase.mpcStartName;
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] LION: %u stop(s), %u start(s) of ExploShort (hash, name, world 0)",
                      lrCase.mpcName, lrCase.muNumStops, lrCase.muNumStarts);
        Check(lbLion, lacLabel);

        // 6. the new effect's slot.
        const BrnParticle::LionEffect& lrSlot = lrParticles.maSlots[lrCase.muNewSlot];
        const f32 lafVelocity[3] = { lrSlot.mfVelocityX, lrSlot.mfVelocityY, lrSlot.mfVelocityZ };
        const bool lbSlot = lrSlot.muHandle == lrCase.muSlotHandle && SameWord(Bits(lrSlot.mfStateBlend), lrCase.muSlotBlend)
                         && SameWords(&lrSlot.mTransform, lrCase.mauSlotTransform, 16)
                         && SameWords(lafVelocity, lrCase.mauSlotVel, 3) && lrSlot.muFlags == lrCase.muSlotFlags;
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the effect in slot %u: the contact frame, the car's velocity, "
                      "the state blend and the flags (0x%04X)", lrCase.mpcName, lrCase.muNewSlot, lrCase.muSlotFlags);
        Check(lbSlot, lacLabel);

        // 7. the module's bookkeeping.
        bool lbModule = lpModule->muNextShowtimeBounceEffect == lrCase.muNextOut
                     && SameWord(Bits(lpModule->mfLastShowtimeBounceEffectTime), lrCase.muLastOut);
        for (u32 i = 0; i < 3; ++i)
            lbModule = lbModule && lpModule->maShowtimeBounceEffectHandles[i] == lrCase.mauHandlesOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the three handles, the next slot (%u) and the last time",
                      lrCase.mpcName, lrCase.muNextOut);
        Check(lbModule, lacLabel);

        // 8. the replay layout.
        const bool lbLayout = saLayout[7] == lrCase.muLayoutByte7Out && SameWords(saLayout + 0x10, lrCase.mauLayoutContactOut, 4)
                           && SameWords(saLayout + 0x20, lrCase.mauLayoutAngleOut, 4);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the static layout's +0x07 byte, point and angle", lrCase.mpcName);
        Check(lbLayout, lacLabel);

        // 9. every draw, the asserts, the player index asked.
        bool lbRandom = lpModule->mRandom.muSeed == lrCase.muSeedOut
                     && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut
                     && (gAsserts - luAssertsBefore) == lrCase.muAsserts
                     && lActive.muStateIndex == lrCase.muStateIndex && lActive.muColourIndex == lrCase.muColourIndex;
        for (u32 i = 0; i < 8; ++i)
            lbRandom = lbRandom && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.mauRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mRandom (every draw, in order), %u assert(s), the player index "
                      "asked of the interface", lrCase.mpcName, lrCase.muAsserts);
        Check(lbRandom, lacLabel);

        delete lpModule;
    }

    std::printf("(%u glass pieces compared; %u NOT RECONSTRUCTED announcement(s); %u [bounce-vfx] line(s))\n", luPieces,
                gNotReconstructed, gBounceLines);
    std::printf("FxCrashVfxShowtimeBounce: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
