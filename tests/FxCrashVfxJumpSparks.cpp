// FX-CRASHVFX (crash parity 2026-09-25, C2): THE JUMP-LANDING SPARKS -- BrnEffects::JumpStateMachine::FireWheelSparks
// @0x82299670 and its callee EffectsModule::FireJumpSparks @0x822969E0.
//
// run_fxcrashvfx_jump_sparks.py hands this fixture the PRODUCTION bodies: FireWheelSparks extracted from the
// revision's JumpStateMachine.cpp (a member of the real JumpStateMachine, against the revision's own header), and
// FireJumpSparks extracted from EffectsModule.cpp together with the file-local pieces it is built from (the vector
// idioms Dot3 / Scale4 / GuardedLength3 / RefinedRecip / CrossPermuted / Splat / FctidzLowWord, the layout reads,
// the SparkShowerController type and gSparkShowerControllerJumpSparks, every K* constant they name). It links the
// real CgsNumeric::Random. The fixture supplies what those bodies call outside themselves: an EffectsModule carrying
// exactly the members they use (the ring, a surfacelist answered from the case, a recording DoSparkShower), the
// Attrib instance plumbing the surface lookup goes through (as the emu64 hooks answer it), the asserts (counted) and
// the log (captured).
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxJumpSparksData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_jumpsparks_data.py, which runs 0x82299670 -- and the real
// FireJumpSparks it calls twice -- on emu64, DoSparkShower @0x822920C0 recorded at the call. Per case, 4 checks: the
// shower count and each shower's controller; each shower's lerp and emitter frame; its velocity to inherit, time,
// ground height and spark count; the ring and the asserts. Plus one: the PC's gSparkShowerControllerJumpSparks holds
// the words the console's init thunk 0x82C4A790 leaves at unk_82CDB1E0. A console NaN matches any NaN.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameSource/Effects/ActiveRaceCarData.h"
#include "GameSource/Effects/ParticleEffectHelper.h"
#include "GameSource/Effects/Jump/JumpStateMachine.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/visualfxsurface.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "rw/core/base/ostypes.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "fxcrashvfx_jumpsparks_config.inc"   // FXJS_HAS_FIRE_JUMP_SPARKS / FXJS_HAS_CONTROLLER (generated)
#include "FxCrashVfxJumpSparksData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

// A failure prints its whole label; a pass only its first 96 characters.
static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lrLabel.c_str());
    }
    else
    {
        std::printf("pass  %.96s\n", lrLabel.c_str());
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool SameWord(u32 luGot, u32 luWant) { return luGot == luWant || (IsNanBits(luWant) && IsNanBits(luGot)); }
static bool SameWords(const u32* lpauGot, const u32* lpauWant, u32 luCount)
{
    for (u32 i = 0; i < luCount; ++i)
    {
        if (!SameWord(lpauGot[i], lpauWant[i]))
            return false;
    }
    return true;
}

// ---- the asserts (counted) and the log (captured) ------------------------------------------------------------------
static std::vector<std::string> gLog;

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
    void WriteToLog(const char* lpcText) { gLog.push_back(lpcText ? lpcText : ""); }
}
}

// ---- the surface lookup, answered as the emu64 hooks answer it -----------------------------------------------------
// Surface slot n (the case's n-th surface, by id): a 32-byte ref block, a surface layout (its visualfxsurface ref at
// +0x10) and a visualfxsurface block (the spark scale at +0x54). Anything else resolves to nothing, and the generated
// ctors then take their own DefaultDataArea fallback: the zeroed block.
static const u32 KU_MAX_SURFACES = 8;
alignas(16) static unsigned char gaSurfaceRefs[KU_MAX_SURFACES][0x20];
alignas(16) static unsigned char gaLayouts[KU_MAX_SURFACES][0x100];
alignas(16) static unsigned char gaVfx[KU_MAX_SURFACES][0x100];
alignas(16) static unsigned char gaDefaultArea[0x100];
static const JumpSparksCase* gpCase = nullptr;
static std::vector<u32> gSurfaceIds;

namespace Attrib
{
    Instance::Instance(const RefSpec& lrRefSpec, void* lpOwner)
        : mpCollection(nullptr), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
        const unsigned char* lpRef = reinterpret_cast<const unsigned char*>(&lrRefSpec);
        for (u32 n = 0; n < KU_MAX_SURFACES; ++n)
        {
            if (lpRef == gaSurfaceRefs[n])
                mpAttributeData = gaLayouts[n];
            else if (lpRef == gaLayouts[n] + 0x10)
                mpAttributeData = gaVfx[n];
        }
    }
    Instance::~Instance() {}
    int Instance::GetClass() const { return 0; }
    u64 Instance::GetCollection() const { return 0; }
    void AssertOnClassCheck(int, int, u64) { ++gAsserts; }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
}

namespace BrnEffects
{
    typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;
    typedef BrnPhysics::Vehicle::WheelLite    WheelLite;

    // CarState as the two bodies read it, by name: the step and the time (EffectsModuleParams +0x10 / +0x14 of the
    // console record) and the race-car state (+0x40).
    struct EffectsModuleParams
    {
        f32     mDt;
        f32     mTime;
        u8      mPad08[8];
        Vector3 mCameraPosition;
    };
    struct CarState
    {
        EffectsModuleParams mEffectsModuleParams;
        const RaceCarState* mpCarState;
        f32 GetDt() const   { return mEffectsModuleParams.mDt; }
        f32 GetTime() const { return mEffectsModuleParams.mTime; }
    };

    // surfacelist::Surfaces(id): the case's surfaces, by id, else nothing.
    struct FakeSurfaceList
    {
        void* Surfaces(u32 luIndex) const
        {
            gSurfaceIds.push_back(luIndex);
            for (u32 n = 0; n < gpCase->muNumSurfaces && n < KU_MAX_SURFACES; ++n)
            {
                if (gpCase->mpSurfaces[n].muSurfaceId == luIndex)
                    return gaSurfaceRefs[n];
            }
            return nullptr;
        }
    };

    // The revision's own jump-diag helpers, for a revision whose FireWheelSparks still announced itself.
    namespace
    {
        bool JumpDiagTakeOnce(bool& lrbAlreadySaid)
        {
            if (lrbAlreadySaid)
                return false;
            lrbAlreadySaid = true;
            return true;
        }
        void JumpDiagText(const char* lpcText) { CgsDev::Log::WriteToLog(lpcText); }
    }

#include "fxcrashvfx_jumpsparks_structs.inc"

    namespace
    {
#include "fxcrashvfx_jumpsparks_consts.inc"
    }

    struct ShowerRecord
    {
        const void* mpController;
        u32 mauLerp[4], mauTransform[16], mauInherit[4];
        u32 muTime, muGround, muCount;
    };

    class EffectsModule
    {
    public:
        CgsNumeric::Random        mRandom;
        FakeSurfaceList           mSurfaceList;
        std::vector<ShowerRecord> mShowers;

        CgsNumeric::Random& RandomNumberGenerator() { return mRandom; }

        void DoSparkShower(const SparkShowerController& lrController,
                           VecFloat lvSize,
                           Matrix44Affine lTransform,
                           Vector3 lvVelocityToInherit,
                           f32 lfCurrentTime,
                           f32 lfGroundPositionY,
                           u32 luNumToSpawn)
        {
            ShowerRecord lRecord;
            lRecord.mpController = &lrController;
            std::memcpy(lRecord.mauLerp, &lvSize, 16);
            std::memcpy(lRecord.mauTransform, &lTransform, 64);
            std::memcpy(lRecord.mauInherit, &lvVelocityToInherit, 16);
            lRecord.muTime   = Bits(lfCurrentTime);
            lRecord.muGround = Bits(lfGroundPositionY);
            lRecord.muCount  = luNumToSpawn;
            mShowers.push_back(lRecord);
        }

        void FireJumpSparks(f32 lfCurrentTimeStep,
                            f32 lfCurrentTime,
                            Vector3 lWSContactPoint,
                            Vector3 lWSContactNormal,
                            const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                            f32 lfGroundPositionY,
                            const CollisionTag& lCollisionTag);
    };

#include "fxcrashvfx_jumpsparks_body.inc"
#include "fxcrashvfx_jumpsparks_wheel.inc"
}

// ---- the rings ------------------------------------------------------------------------------------------------------
static void SetRandom(CgsNumeric::Random& lrRandom, const u32* lpauRing, u64 luSeed, u32 luIndex)
{
    for (u32 i = 0; i < 8; ++i)
        lrRandom.mauIntegerBuffer[i] = lpauRing[i];
    lrRandom.muSeed = luSeed;
    lrRandom.muOldestBufferIndex = luIndex;
}

static bool SameRandom(const CgsNumeric::Random& lrRandom, const u32* lpauRing, u64 luSeed, u32 luIndex)
{
    bool lbSame = lrRandom.muSeed == luSeed && lrRandom.muOldestBufferIndex == luIndex;
    for (u32 i = 0; i < 8; ++i)
        lbSame = lbSame && lrRandom.mauIntegerBuffer[i] == lpauRing[i];
    return lbSame;
}

static void PrintWords(const char* lpcLabel, const u32* lpauGot, const u32* lpauWant, u32 luCount)
{
    std::printf("        %s got: ", lpcLabel);
    for (u32 i = 0; i < luCount; ++i)
        std::printf(" %08X", lpauGot[i]);
    std::printf("\n        %s want:", lpcLabel);
    for (u32 i = 0; i < luCount; ++i)
        std::printf(" %08X", lpauWant[i]);
    std::printf("\n");
}

int main()
{
    using namespace BrnEffects;

    alignas(16) static unsigned char saState[sizeof(RaceCarState)];
    alignas(16) static unsigned char saActive[sizeof(ActiveRaceCarData)];
    alignas(16) static unsigned char saMachine[sizeof(JumpStateMachine)];
    alignas(16) static unsigned char saParticles[64];   // the helper's particle module: never read

    const u32 luNumCases = static_cast<u32>(sizeof(kaJumpSparksCases) / sizeof(kaJumpSparksCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const JumpSparksCase& lrCase = kaJumpSparksCases[luCase];
        gpCase = &lrCase;
        gSurfaceIds.clear();

        // The surfaces: slot n holds the case's n-th surface's spark scale.
        std::memset(gaSurfaceRefs, 0, sizeof(gaSurfaceRefs));
        std::memset(gaLayouts, 0, sizeof(gaLayouts));
        std::memset(gaVfx, 0, sizeof(gaVfx));
        std::memset(gaDefaultArea, 0, sizeof(gaDefaultArea));
        for (u32 n = 0; n < lrCase.muNumSurfaces && n < KU_MAX_SURFACES; ++n)
            std::memcpy(gaVfx[n] + 0x54, &lrCase.mpSurfaces[n].muSparkScale, 4);

        // The race-car state: the two rear wheels and the linear velocity, by name.
        std::memset(saState, 0, sizeof(saState));
        RaceCarState* lpState = reinterpret_cast<RaceCarState*>(saState);
        for (u32 w = 0; w < 2; ++w)
        {
            WheelLite& lrWheel = lpState->maWheels[2 + w];
            const JumpSparksWheel& lrData = lrCase.maWheels[w];
            std::memcpy(&lrWheel.mRoadContact.mPosition, lrData.mauPos, 16);
            std::memcpy(&lrWheel.mRoadContact.mNormal, lrData.mauNormal, 16);
            lrWheel.mRoadContact.mCollisionTag.muValue = lrData.muTag;
            lrWheel.mRoadContact.mbIsOnGround = lrData.mu8Ground != 0;
        }
        std::memcpy(&lpState->mLinearVelocity, lrCase.mauVel, 16);

        // The active race car: its ground height, by name.
        std::memset(saActive, 0, sizeof(saActive));
        ActiveRaceCarData* lpActive = reinterpret_cast<ActiveRaceCarData*>(saActive);
        lpActive->mfGroundPositionY = Float(lrCase.muGround);

        EffectsModule lEffects;
        SetRandom(lEffects.mRandom, lrCase.mauEffRing, lrCase.muEffSeed, lrCase.muEffIndex);

        CarState lCarState;
        std::memset(&lCarState, 0, sizeof(lCarState));
        lCarState.mEffectsModuleParams.mDt   = Float(lrCase.muDt);
        lCarState.mEffectsModuleParams.mTime = Float(lrCase.muTime);
        lCarState.mpCarState = lpState;

        RwRGBAReal lColour;
        std::memset(&lColour, 0, sizeof(lColour));
        RaceCarParticleEffectHelper lHelper(lEffects, *lpActive, lpState,
                                            *reinterpret_cast<BrnParticle::ParticleModule*>(saParticles), nullptr, 0u,
                                            lColour, static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(0),
                                            nullptr);

        gAsserts = 0;
        const JumpStateMachine* lpMachine = reinterpret_cast<const JumpStateMachine*>(saMachine);
        lpMachine->FireWheelSparks(lCarState, lHelper);

        const std::string lName = "[" + std::to_string(luCase) + "] " + lrCase.mpcName;
        const bool lbCount = lEffects.mShowers.size() == lrCase.muNumShowers;
        bool lbController = lbCount, lbFrame = lbCount, lbScalars = lbCount;
        for (u32 i = 0; lbCount && i < lrCase.muNumShowers; ++i)
        {
            const ShowerRecord&     lrGot  = lEffects.mShowers[i];
            const JumpSparksShower& lrWant = lrCase.mpShowers[i];
#if FXJS_HAS_CONTROLLER
            lbController = lbController && lrWant.muController == 1u
                        && lrGot.mpController == static_cast<const void*>(&gSparkShowerControllerJumpSparks);
#else
            lbController = false;
#endif
            const bool lbThisFrame = SameWords(lrGot.mauLerp, lrWant.mauLerp, 4)
                                  && SameWords(lrGot.mauTransform, lrWant.mauTransform, 16);
            if (!lbThisFrame)
            {
                std::printf("        shower %u:\n", i);
                PrintWords("lerp     ", lrGot.mauLerp, lrWant.mauLerp, 4);
                PrintWords("transform", lrGot.mauTransform, lrWant.mauTransform, 16);
            }
            lbFrame = lbFrame && lbThisFrame;
            const bool lbThisScalars = SameWords(lrGot.mauInherit, lrWant.mauInherit, 4)
                                    && SameWord(lrGot.muTime, lrWant.muTime) && SameWord(lrGot.muGround, lrWant.muGround)
                                    && lrGot.muCount == lrWant.muCount;
            if (!lbThisScalars)
            {
                PrintWords("inherit  ", lrGot.mauInherit, lrWant.mauInherit, 4);
                std::printf("        time %08X/%08X ground %08X/%08X count %u/%u\n", lrGot.muTime, lrWant.muTime,
                            lrGot.muGround, lrWant.muGround, lrGot.muCount, lrWant.muCount);
            }
            lbScalars = lbScalars && lbThisScalars;
        }
        char lacCounts[96];
        std::snprintf(lacCounts, sizeof(lacCounts), " (showers %u want %u)",
                      static_cast<u32>(lEffects.mShowers.size()), lrCase.muNumShowers);
        Check(lbController, lName + " -- the shower count and each shower's controller" + lacCounts);
        Check(lbFrame, lName + " -- each shower's lerp and emitter frame (normal, along-ground velocity, cross, point)");
        Check(lbScalars, lName + " -- each shower's velocity to inherit, time, ground height, spark count");
        Check(SameRandom(lEffects.mRandom, lrCase.mauEffRingOut, lrCase.muEffSeedOut, lrCase.muEffIndexOut)
              && gAsserts == lrCase.muAsserts,
              lName + " -- the ring (two vector-slot draws, one RandomFloat per shower) and the asserts");
    }

#if FXJS_HAS_CONTROLLER
    u32 lauController[27];
    std::memcpy(lauController, &gSparkShowerControllerJumpSparks, sizeof(lauController));
    const bool lbControllerWords = SameWords(lauController, kauJumpSparksController, 27);
    if (!lbControllerWords)
        PrintWords("controller", lauController, kauJumpSparksController, 27);
#else
    const bool lbControllerWords = false;
#endif
    Check(lbControllerWords, "gSparkShowerControllerJumpSparks holds what the console's thunk 0x82C4A790 leaves at "
                             "unk_82CDB1E0 (both argument sets, reflection 2.0, threshold 44.694443, array 0)");

    std::printf("FxCrashVfxJumpSparks: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures != 0 ? 1 : 0;
}
