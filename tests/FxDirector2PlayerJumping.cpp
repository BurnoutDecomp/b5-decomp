// FX-DIRECTOR2 (crash parity 2026-09-25): MomentPlayerJumping, the jump moment (type 7) -- its retail path re-derived
// from the X360 and the camera side behind the mbAllowJumpMoment gate LOUD-trapped.
//
// The runner (run_fxdirector2_player_jumping.py) writes the revision's PRODUCTION BrnMomentPlayerJumping.cpp into
// fxd2_player_jumping.inc, which this file includes. The stand-ins below are the moment's leaves:
//   - the behaviour manager's UnSetBehaviourUsedByHandle, recording each (manager, key);
//   - the shot group's Num_ShotList / GetAttributePointer / DefaultDataArea, recording what is asked of them;
//   - Camera::Construct, SetRequestedBorderPostFX (its one real store), CameraState::SetFlag (its real bit set);
//   - the assert and the game log, both recording.
// The BehaviourManager and DirectorResourceManager are raw storage reached BY NAME, so the bank blocks and the jump
// shot group are the members the moment reads.
//
// Against the ARTIST bodies:
//   Construct       @0x8225F1F0  the inlined Moment::Construct; the four collections (owners, latches, index,
//                                count); the interpolate handle and parameters {8, 0, method 1, mapping 3}; the
//                                cooldown seed 0.5 (flt_82001DA0); meType -1; mbPrepared 0; mpParameters 0
//   Prepare         @0x82251048  SEARCHING; once only: rig slots 1 2 7 10 9 5, bystander 0 2, dropped 11 12 13, and
//                                the ice ShotList clamped to five with the 24-byte DefaultDataArea for a null element
//   Update          @0x82275988  SEARCHING -- the retail path: the landing cooldown integrates mfSimTimestep while
//                                |time in air| is not > FLT_EPSILON (a NaN integrates); the three gates (action flag
//                                bit 3, mbCanUseSlomo, mbAllowJumpMoment) -> not met / can't switch / head bit 18;
//                                inhibited -> head bit 23; all three -> ATTACHED_RIG_SHOT, PrepareCameras,
//                                FOUND_PREPARING, head bit 19. FOUND_PREPARING / VALID and the default arm.
//   Release         @0x8223AED0  cooldown 0; each collection releases its allocated handles only when
//                                mbAllocatedShots; the interpolate handle; not met, head bit 18, SEARCHING
//   AddShot         @0x8224B1B8 / 0x8224B0A8 / 0x8224B130  the non-gating "can't add shots" assert, the Append
//   [PC TRAP, NOT X360]  the collections' Prepare / HasFailed / CanSwitchToMeNow and UpdateCamera @0x8223AB78:
//                        announced once in the game log, asserted every time, never silent.
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;
static std::vector<std::string> gAssertLog;
static unsigned gLogWrites = 0;
static std::string gLastLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int)
    {
        ++gAsserts;
        gLastAssert = lpcText ? lpcText : "";
        gAssertLog.push_back(gLastAssert);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    void WriteToLog(const char* lpcText)
    {
        ++gLogWrites;
        gLastLog = lpcText ? lpcText : "";
    }
}
}

using namespace BrnDirector;

// ---- recording stand-ins ------------------------------------------------------------------------------------
struct UnSetCall { const void* mpManager; u32 muKey; };
static std::vector<UnSetCall> gUnSets;
static int giCameraConstructs = 0;
static const void* gpNumShotListThis = nullptr;
static u32 guShotListLength = 7;
struct PointerCall { const void* mpThis; u64 muKey; u32 muIndex; };
static std::vector<PointerCall> gPointerCalls;
static u32 guNullElement = 2;                   // this element index resolves to null
static u32 guDefaultDataBytes = 0;
static int giDefaultDataCalls = 0;
alignas(16) static u8 gaShotElements[8][32];
alignas(16) static u8 gaDefaultData[32];

namespace BrnDirector
{
namespace Camera
{
    void Camera::Construct() { ++giCameraConstructs; }
    void Camera::SetRequestedBorderPostFX(f32 lfAmount) { mEffects.mfBlackBarAmount = lfAmount; }   // Camera.cpp's one store
    void CameraState::SetFlag(u32 luIndex, bool lbValue)                                          // BrnCameraState.cpp's body
    {
        if (lbValue)
            mCurrentFlags.SetBit(luIndex);
        else
            mCurrentFlags.UnSetBit(luIndex);
    }
    void ValidityAccount::MaskToFailFlags() {}   // Moment::PreUpdate's account mask (tested by run_fxdirector_moments.py)
    void BehaviourManager::UnSetBehaviourUsedByHandle(u32 luAllocationKey)
    {
        gUnSets.push_back(UnSetCall{ this, luAllocationKey });
    }
}
    void Moment::SetParameters(const Parameters*) {}
    void Moment::Destruct() {}
}

namespace Attrib
{
    void* DefaultDataArea(u32 luBytes)
    {
        ++giDefaultDataCalls;
        guDefaultDataBytes = luBytes;
        return gaDefaultData;
    }
    void* Instance::GetAttributePointer(u64 luAttributeKey, u32 luIndex) const
    {
        gPointerCalls.push_back(PointerCall{ this, luAttributeKey, luIndex });
        return luIndex == guNullElement ? nullptr : gaShotElements[luIndex & 7u];
    }
namespace Gen
{
    u32 shotgroup::Num_ShotList() const
    {
        gpNumShotListThis = this;
        return guShotListLength;
    }
}
}

#include "fxd2_player_jumping.inc"

// ---- helpers -----------------------------------------------------------------------------------------------
static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}
static u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }

alignas(16) static u8 gaMomentSlot[sizeof(MomentPlayerJumping) + 16];
alignas(16) static u8 gaManager[sizeof(Camera::BehaviourManager)];
alignas(16) static u8 gaResourceManager[sizeof(DirectorResourceManager)];
static Camera::VehicleInfo gPlayerCar;
static GameState gGameState;
static MomentSharedInfo gInfo;
static MomentPlayerJumping::Parameters gMomentParameters;

static Camera::BehaviourManager& Manager() { return *reinterpret_cast<Camera::BehaviourManager*>(gaManager); }
static DirectorResourceManager& ResourceManager()
{
    return *reinterpret_cast<DirectorResourceManager*>(gaResourceManager);
}
static const Camera::BehaviourParameterBank& Bank() { return Manager().mBehaviourParameterBank; }

static MomentPlayerJumping* Placed()
{
    std::memset(gaMomentSlot, 0xCD, sizeof(gaMomentSlot));
    return new (gaMomentSlot) MomentPlayerJumping;
}
static MomentPlayerJumping* FreshMoment()
{
    MomentPlayerJumping* lpMoment = Placed();
    Moment* lpBase = lpMoment;
    lpBase->Construct();
    lpBase->SetParameters(&gMomentParameters);
    lpBase->Prepare(gaManager);
    return lpMoment;
}
static bool HeadFlag(const MomentPlayerJumping* lpMoment, u32 luBit)
{
    return lpMoment->mCamera.mState.mHeadFlags.IsBitSet(luBit);
}
static void Tick(MomentPlayerJumping* lpMoment, f32 lfTimeStep = 1.0f / 30.0f)
{
    lpMoment->PreUpdate();
    static_cast<Moment*>(lpMoment)->Update(lfTimeStep, gaManager, &gInfo);
}
template <typename TCollection>
static bool Constructed(const TCollection& lrCollection, const Moment* lpOwner)
{
    return lrCollection.mpArbStateOwner == nullptr && lrCollection.mpMomentOwner == lpOwner
        && !lrCollection.mbAllocatedShots && !lrCollection.mbHasCurrentCamera && lrCollection.miCurrentCamera == 0
        && lrCollection.maShots.miCount == 0;
}
template <typename TCollection, typename TParams>
static bool ShotIs(const TCollection& lrCollection, u32 luShot, const TParams* lpParams)
{
    const auto& lrShot = lrCollection.maShots.maElements[luShot];
    return lrShot.mpParams == lpParams && !lrShot.mHandle.mbAllocated && lrShot.mHandle.muAllocationKey == 0
        && lrShot.mHandle.mpHelperPool == nullptr && lrShot.mHandle.mpManager == nullptr
        && lrShot.mHandle.mpBehaviour == nullptr;
}
static bool Announced(const char* lpcFragment)
{
    return gLastLog.find("[pjump] TRAP (PC, NOT X360)") == 0 && gLastLog.find(lpcFragment) != std::string::npos;
}

int main()
{
    _putenv_s("BRN_CRASHCAM_DIAG", "");         // the tick witness ([DIAG], NOT X360) stays quiet: the log counts are the traps'
    Manager().mpDirectorResourceManager = &ResourceManager();
    gInfo.mpGameState = &gGameState;
    gInfo.mpPlayerCar = &gPlayerCar;
    gInfo.mfTimestep = 0.25f;                   // NOT the addend: the console reads mfSimTimestep (+0x520)
    gInfo.mfSimTimestep = 1.0f / 60.0f;
    gInfo.mbAllowJumpMoment = false;            // the retail value (MainDirector::Construct @0x8225B97C)
    gGameState.miThisFramesActionFlags = 0x8;   // the player is jumping
    gGameState.mbCanUseSlomo = true;
    gPlayerCar.mRaceCarState.mfTimeInAir = 0.0f;

    // ================= Construct @0x8225F1F0 over pool garbage =================
    MomentPlayerJumping* lpMoment = Placed();
    lpMoment->mInterpolater.mbAllocated = true;
    lpMoment->mInterpolater.muAllocationKey = 77;
    lpMoment->mRigCollection.mbAllocatedShots = true;
    Moment* lpBase = lpMoment;
    lpBase->Construct();
    Check(lpMoment->GetState() == Moment::E_STATE_INVALID_INACTIVE && lpMoment->GetType() == Moment::E_MOMENT_PLAYER_JUMPING
              && !lpMoment->IsInhibited() && giCameraConstructs == 1,
          "K1 Construct: the inlined Moment::Construct -- INACTIVE, type 7 (vtable slot 7), not inhibited, "
          "Camera::Construct");
    Check(Constructed(lpMoment->mRigCollection, lpMoment) && Constructed(lpMoment->mBystanderCollection, lpMoment)
              && Constructed(lpMoment->mDroppedRigCollection, lpMoment) && Constructed(lpMoment->mIceCollection, lpMoment),
          "K2 Construct: the four collections -- no arbitrator-state owner, this moment as owner, both latches 0, "
          "current 0, the shot array's count 0 (0x8225F238..0x8225F2A8)");
    Check(!lpMoment->mInterpolater.mbAllocated && lpMoment->mInterpolater.muAllocationKey == 0
              && lpMoment->mInterpolater.mpHelperPool == nullptr && lpMoment->mInterpolater.mpManager == nullptr
              && lpMoment->mInterpolater.mpBehaviour == nullptr
              && lpMoment->mInterpolaterParams.mType == 8 && lpMoment->mInterpolaterParams.mpcDebugName == nullptr
              && lpMoment->mInterpolaterParams.meInterpolationMethod == Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR
              && lpMoment->mInterpolaterParams.meInterpolationMapping == Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED,
          "K3 Construct: the interpolate handle's five words zeroed; its parameters {8, no name, method 1 (`stw 1, "
          "0x374`), mapping 3 (`stw 3, 0x378`, stored last)}");
    Check(Bits(lpMoment->mfCooldownTimer) == 0x3F000000u && lpMoment->meType == MomentPlayerJumping::E_INVALID_TYPE
              && !lpMoment->mbPrepared && lpMoment->mpParameters == nullptr,
          "K4 Construct: mfCooldownTimer = kfCooldownTime (flt_82001DA0 == 0x3F000000), meType -1, mbPrepared 0, "
          "mpParameters 0");

    // ================= SetParameters / Prepare @0x82251048 =================
    lpBase->SetParameters(&gMomentParameters);
    const bool lbPrepared = lpBase->Prepare(gaManager);
    Check(lpMoment->mpParameters == &gMomentParameters && lbPrepared && lpMoment->mbPrepared
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING,
          "K5 SetParameters stores the block (`stw r4, 0x180(r3)`); Prepare -> SEARCHING, mbPrepared, true");
    const Camera::BehaviourParameterBank& lrBank = Bank();
    const auto& lrRig = lpMoment->mRigCollection;
    Check(lrRig.maShots.miCount == 6 && ShotIs(lrRig, 0, &lrBank.mRigRearQFwd) && ShotIs(lrRig, 1, &lrBank.mRigFrontQCuFwd)
              && ShotIs(lrRig, 2, &lrBank.mRigRoofFwd) && ShotIs(lrRig, 3, &lrBank.mRigUnderbelly)
              && ShotIs(lrRig, 4, &lrBank.mRigFrontQCuFwd2) && ShotIs(lrRig, 5, &lrBank.mRigBootViewFwd),
          "K6 Prepare: the attached-rig collection holds bank rig slots 1 2 7 10 9 5, in that order, handles clear");
    const auto& lrBystander = lpMoment->mBystanderCollection;
    Check(lrBystander.maShots.miCount == 2 && ShotIs(lrBystander, 0, &lrBank.mBystanderJumpLeftParameters)
              && ShotIs(lrBystander, 1, &lrBank.mBystanderJumpFromBehindParameters),
          "K7 Prepare: the bystander collection holds bystander slots 0 and 2");
    const auto& lrDropped = lpMoment->mDroppedRigCollection;
    Check(lrDropped.maShots.miCount == 3 && ShotIs(lrDropped, 0, &lrBank.mRigDropUnderbelly)
              && ShotIs(lrDropped, 1, &lrBank.mRigDropFrontQCuFwd) && ShotIs(lrDropped, 2, &lrBank.mRigDropBootViewFwd),
          "K8 Prepare: the dropped-rig collection holds rig slots 11 12 13");
    const auto& lrIce = lpMoment->mIceCollection;
    const void* lpJumpRig = &ResourceManager().mJumpRig;
    bool lbIceCalls = gPointerCalls.size() == 5;
    for (u32 i = 0; lbIceCalls && i < 5; ++i)
        lbIceCalls = gPointerCalls[i].mpThis == lpJumpRig && gPointerCalls[i].muKey == 0x7533C0E215246B49ULL
                  && gPointerCalls[i].muIndex == i;
    bool lbIceShots = lrIce.maShots.miCount == 5;
    for (u32 i = 0; lbIceShots && i < 5; ++i)
        lbIceShots = ShotIs(lrIce, i, reinterpret_cast<const Attrib::RefSpec*>(
                                          i == guNullElement ? static_cast<void*>(gaDefaultData) : gaShotElements[i]));
    Check(gpNumShotListThis == lpJumpRig && lbIceCalls && lbIceShots && giDefaultDataCalls == 1 && guDefaultDataBytes == 0x18,
          "K9 Prepare: the jump-rig shot group's ShotList (7 long) clamped to FIVE; each element through "
          "GetAttributePointer(0x7533C0E2_15246B49, i) on the resource manager's mJumpRig, the null one replaced by "
          "DefaultDataArea(0x18)");
    const bool lbAgain = lpBase->Prepare(gaManager);
    Check(lbAgain && lrRig.maShots.miCount == 6 && lrBystander.maShots.miCount == 2 && lrDropped.maShots.miCount == 3
              && lrIce.maShots.miCount == 5 && gPointerCalls.size() == 5,
          "K10 a second Prepare adds nothing (the mbPrepared guard) and reports true");
    guShotListLength = 3;
    gPointerCalls.clear();
    MomentPlayerJumping* lpShort = FreshMoment();
    Check(lpShort->mIceCollection.maShots.miCount == 3 && gPointerCalls.size() == 3,
          "K11 a ShotList shorter than five is taken whole (`cmplwi 5 ; bgt`)");
    guShotListLength = 7;

    // ================= SEARCHING -- the retail path =================
    lpMoment = FreshMoment();
    const unsigned luAssertsRetail = gAsserts, luLogsRetail = gLogWrites;
    Tick(lpMoment);
    Check(!lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 18)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING
              && lpMoment->meType == MomentPlayerJumping::E_INVALID_TYPE
              && gAsserts == luAssertsRetail && gLogWrites == luLogsRetail,
          "K12 RETAIL (jumping, slow-mo allowed, mbAllowJumpMoment false): not met, can't switch, head bit 18, still "
          "SEARCHING -- no shot, no trap, no assert");

    // the landing cooldown
    struct AirCase { f32 mfTimeInAir; bool mbIntegrates; const char* mpcLabel; };
    const f32 kfEpsilon = 1.1920928955078125e-7f;
    const AirCase kaAir[] = {
        { 0.0f, true, "grounded" },
        { -0.0f, true, "-0" },
        { kfEpsilon, true, "exactly FLT_EPSILON (not >)" },
        { -kfEpsilon, true, "-FLT_EPSILON (the sign is cleared)" },
        { std::nextafter(kfEpsilon, 1.0f), false, "just above FLT_EPSILON" },
        { 0.5f, false, "airborne" },
        { -0.3f, false, "negative, |x| > FLT_EPSILON" },
        { std::numeric_limits<f32>::quiet_NaN(), true, "NaN (fails the ordered vcmpgtfp)" },
        { std::numeric_limits<f32>::infinity(), false, "+inf" },
    };
    bool lbAir = true;
    for (const AirCase& lrCase : kaAir)
    {
        lpMoment = FreshMoment();
        gPlayerCar.mRaceCarState.mfTimeInAir = lrCase.mfTimeInAir;
        Tick(lpMoment);
        const f32 lfExpected = lrCase.mbIntegrates ? (gInfo.mfSimTimestep + 0.5f) : 0.5f;
        if (Bits(lpMoment->mfCooldownTimer) != Bits(lfExpected))
        {
            lbAir = false;
            std::printf("      cooldown wrong for %s\n", lrCase.mpcLabel);
        }
    }
    gPlayerCar.mRaceCarState.mfTimeInAir = 0.0f;
    Check(lbAir, "K13 the cooldown integrates mfSimTimestep (+0x520, not mfTimestep) exactly when |time in air| is NOT > "
                 "FLT_EPSILON (flt_82001770): 0, -0, +-epsilon and NaN integrate; above epsilon, negative, +inf do not");

    // the three gates
    struct GateCase { s32 miFlags; bool mbSlomo; bool mbAllow; const char* mpcLabel; };
    const GateCase kaGates[] = {
        { 0x17, true, true, "no action-flag bit 3 (0x17: the stunt / smash / billboard bits only)" },
        { 0x8, false, true, "slow-mo not allowed (GameState +0x100)" },
        { 0x8, true, false, "mbAllowJumpMoment false (+0x524) -- the retail case" },
        { 0x0, false, false, "none" },
    };
    bool lbGates = true;
    for (const GateCase& lrCase : kaGates)
    {
        lpMoment = FreshMoment();
        gGameState.miThisFramesActionFlags = lrCase.miFlags;
        gGameState.mbCanUseSlomo = lrCase.mbSlomo;
        gInfo.mbAllowJumpMoment = lrCase.mbAllow;
        Tick(lpMoment);
        if (lpMoment->ConditionsAreMet() || !HeadFlag(lpMoment, 18) || lpMoment->GetState() != Moment::E_STATE_INVALID_SEARCHING)
        {
            lbGates = false;
            std::printf("      gate case wrong: %s\n", lrCase.mpcLabel);
        }
    }
    gGameState.miThisFramesActionFlags = 0x8;
    gGameState.mbCanUseSlomo = true;
    Check(lbGates, "K14 any gate shut -> not met + head bit 18 (0x82275CEC)");

    // ================= behind the gate (never on retail) =================
    gInfo.mbAllowJumpMoment = true;
    lpMoment = FreshMoment();
    lpMoment->SetInhibited(true);
    lpMoment->mCamera.mState.mHeadFlags.UnSetAll();
    const unsigned luAssertsInhibited = gAsserts;
    Tick(lpMoment);
    Check(!lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 23) && !HeadFlag(lpMoment, 18) && lpMoment->ConditionsAreMet()
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING
              && lpMoment->meType == MomentPlayerJumping::E_INVALID_TYPE && gAsserts == luAssertsInhibited,
          "K15 all three gates, inhibited (+0x17B): can't switch, head bit 23 (`oris 0x80`), conditions stay met, "
          "nothing prepared");

    lpMoment = FreshMoment();
    lpMoment->mfJumpTimer = 9.0f;
    const unsigned luLogsBefore = gLogWrites;
    gAssertLog.clear();
    Tick(lpMoment);
    Check(lpMoment->meType == MomentPlayerJumping::E_TYPE_ATTACHED_RIG_SHOT && Bits(lpMoment->mfJumpTimer) == 0u
              && !lpMoment->CanSwitchToMeNow() && lpMoment->ConditionsAreMet() && HeadFlag(lpMoment, 19)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_FOUND_PREPARING,
          "K16 all three gates: meType ATTACHED_RIG_SHOT (5), PrepareCameras, mfJumpTimer 0, can't switch, "
          "FOUND_PREPARING, head bit 19");
    Check(gLogWrites == luLogsBefore + 1 && Announced("BehaviourCollection<>::Prepare")
              && gAssertLog.size() == 2 && gAssertLog[0].find("BehaviourCollection::Prepare has no host body") == 0
              && gAssertLog[1] == "mRigCollection.Prepare(lBehaviourManager)",
          "K17 [PC TRAP] the rig collection's Prepare announces itself in the game log and asserts; PrepareCameras' "
          "own assert (:448) reports the failure");

    const unsigned luLogsHasFailed = gLogWrites;
    gAssertLog.clear();
    Tick(lpMoment);
    Check(gLogWrites == luLogsHasFailed + 1 && Announced("BehaviourCollection<>::HasFailed") && gAssertLog.size() == 1
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING && HeadFlag(lpMoment, 18)
              && !lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow() && Bits(lpMoment->mfCooldownTimer) == 0u,
          "K18 FOUND_PREPARING: [PC TRAP] HasFailed announces, asserts and reports FAILED -> Release (cooldown 0, not "
          "met, head bit 18) and SEARCHING");

    const unsigned luLogsSecond = gLogWrites;
    gAssertLog.clear();
    Tick(lpMoment);
    Check(gLogWrites == luLogsSecond && gAssertLog.size() == 2
              && gAssertLog[0].find("BehaviourCollection::Prepare has no host body") == 0
              && lpMoment->GetState() == Moment::E_STATE_INVALID_FOUND_PREPARING,
          "K19 a trap announces ONCE: the second pass asserts again but writes nothing to the log");

    // VALID (only reachable with the gate open and a READY status -- entered directly here)
    lpMoment = FreshMoment();
    lpMoment->SetState(Moment::E_STATE_VALID);
    lpMoment->mfJumpTimer = 0.0f;
    std::memset(&lpMoment->mCamera.mEffects, 0, sizeof(lpMoment->mCamera.mEffects));
    const unsigned luLogsValid = gLogWrites;
    gAssertLog.clear();
    Tick(lpMoment, 0.05f);
    const Camera::CameraEffects& lrEffects = lpMoment->mCamera.mEffects;
    Check(gLogWrites == luLogsValid + 1 && Announced("MomentPlayerJumping::UpdateCamera @0x8223AB78")
              && gAssertLog.size() == 1 && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING
              && Bits(lpMoment->mfCooldownTimer) == 0u && !lpMoment->ConditionsAreMet()
              && Bits(lpMoment->mfJumpTimer) == Bits(0.05f),
          "K20 VALID: mfJumpTimer += lfTimeStep; [PC TRAP] UpdateCamera announces, asserts and reports a camera failure "
          "-> Release (cooldown 0, not met), SEARCHING");
    Check(Bits(lrEffects.mfBlackBarAmount) == 0x3E19999Au && lpMoment->mCamera.mState.mCurrentFlags.IsBitSet(7)
              && lpMoment->mCamera.mState.mCurrentFlags.IsBitSet(1)
              && std::strcmp(lrEffects.mStartHookNameString.mHookNameString, "Jump_Effect") == 0
              && lrEffects.mbHasStartHookNameString && Bits(lrEffects.mfStartHookNameBlendAmount) == 0x3F800000u,
          "K21 the camera tail runs on the release frame too: border post-FX 0.15 (flt_82CDA5E0 == 0x3E19999A), state "
          "bits 7 and 1, and under kfMinTimeInAir the \"Jump_Effect\" start hook, blend 1 -- then the moment adopts it");
    gInfo.mbAllowJumpMoment = false;

    // ================= Release @0x8223AED0 =================
    lpMoment = FreshMoment();
    u8 laOtherManager[16];
    lpMoment->mInterpolater.mbAllocated = true;
    lpMoment->mInterpolater.muAllocationKey = 41;
    lpMoment->mInterpolater.mpManager = &Manager();
    lpMoment->mInterpolater.mpHelperPool = reinterpret_cast<Camera::BehaviourManager::HelperPool*>(laOtherManager);
    auto& lrRigShots = lpMoment->mRigCollection;
    lrRigShots.mbAllocatedShots = true;
    lrRigShots.mbHasCurrentCamera = true;
    lrRigShots.maShots.maElements[0].mHandle.mbAllocated = true;
    lrRigShots.maShots.maElements[0].mHandle.muAllocationKey = 12;
    lrRigShots.maShots.maElements[0].mHandle.mpManager = &Manager();
    lrRigShots.maShots.maElements[3].mHandle.mbAllocated = true;
    lrRigShots.maShots.maElements[3].mHandle.muAllocationKey = 15;
    lrRigShots.maShots.maElements[3].mHandle.mpManager = &Manager();
    lpMoment->mBystanderCollection.maShots.maElements[0].mHandle.mbAllocated = true;   // latch down: not released
    lpMoment->mBystanderCollection.maShots.maElements[0].mHandle.mpManager = &Manager();
    lpMoment->PreUpdate();
    gUnSets.clear();
    const bool lbReleased = static_cast<Moment*>(lpMoment)->Release();
    Check(lbReleased && gUnSets.size() == 3 && gUnSets[0].muKey == 12 && gUnSets[1].muKey == 15 && gUnSets[2].muKey == 41
              && gUnSets[0].mpManager == &Manager(),
          "K22 Release: the rig collection's two allocated handles (keys 12, 15) then the interpolate handle (41) are "
          "released -- the bystander collection's handle is not, its mbAllocatedShots latch is down");
    Check(!lrRigShots.mbAllocatedShots && !lrRigShots.mbHasCurrentCamera && !lrRigShots.maShots.maElements[0].mHandle.mbAllocated
              && lrRigShots.maShots.maElements[0].mHandle.mpManager == nullptr
              && lrRigShots.maShots.maElements[0].mHandle.muAllocationKey == 12
              && lpMoment->mBystanderCollection.maShots.maElements[0].mHandle.mbAllocated
              && !lpMoment->mInterpolater.mbAllocated && lpMoment->mInterpolater.mpManager == nullptr
              && lpMoment->mInterpolater.mpHelperPool == nullptr && lpMoment->mInterpolater.muAllocationKey == 41,
          "K23 Release: each released handle's +0x00 / +0x08 / +0x0C / +0x10 cleared and its key (+0x04) kept, as "
          "the inlined BehaviourHandle::Release does; the collection's two latches dropped");
    Check(Bits(lpMoment->mfCooldownTimer) == 0u && !lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow()
              && HeadFlag(lpMoment, 18) && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING,
          "K24 Release: cooldown 0, not met, can't switch, head bit 18, SEARCHING (`stw 1, 0x174` -- not INACTIVE)");

    // ================= AddShot's assert =================
    // (The dropped-rig collection has one free slot of four; a full collection's Append writes past its array on the
    // console as well -- its out-of-space assert is non-gating -- so that is not exercised here.)
    lpMoment = FreshMoment();
    gAssertLog.clear();
    lpMoment->mDroppedRigCollection.mbAllocatedShots = true;
    lpMoment->mDroppedRigCollection.AddShot(&lrBank.mRigRearQFwd);
    Check(gAssertLog.size() == 1
              && gAssertLog[0] == "can't add shots when the behaviours have been allocated (although this could be changed)"
              && lpMoment->mDroppedRigCollection.maShots.miCount == 4
              && ShotIs(lpMoment->mDroppedRigCollection, 3, &lrBank.mRigRearQFwd),
          "K25 AddShot on an allocated collection trips its non-gating \"can't add shots\" assert (:159) and still "
          "appends the shot");

    // ================= the rest =================
    Check(std::strcmp(static_cast<Moment*>(lpMoment)->GetName(), "MomentPlayerJumping") == 0
              && lpMoment->GetInstanceType() == Moment::E_MOMENT_PLAYER_JUMPING,
          "K26 GetName \"MomentPlayerJumping\" (0x821F7630), GetInstanceType 7 (0x821F7628)");
    lpMoment = FreshMoment();
    lpMoment->SetState(Moment::E_STATE_INVALID_INACTIVE);
    gAssertLog.clear();
    Tick(lpMoment);
    Check(gAssertLog.size() == 1 && gAssertLog[0] == "unhandled case in switch"
              && lpMoment->GetState() == Moment::E_STATE_INVALID_INACTIVE,
          "K27 Update in any other state trips \"unhandled case in switch\" (:400) and returns");
    MomentPlayerJumping* lpUnprepared = Placed();
    static_cast<Moment*>(lpUnprepared)->Construct();
    lpUnprepared->SetState(Moment::E_STATE_INVALID_SEARCHING);
    gAssertLog.clear();
    Tick(lpUnprepared);
    Check(gAssertLog.size() == 1 && gAssertLog[0] == "mbPrepared" && HeadFlag(lpUnprepared, 18),
          "K28 Update before Prepare trips the non-gating \"mbPrepared\" assert (:263) and runs on");

    std::printf("FxDirector2PlayerJumping: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
