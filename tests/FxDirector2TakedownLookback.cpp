// FX-DIRECTOR2 (crash parity 2026-09-25): MomentTakedownLookback, re-derived from the X360.
//
// The runner (run_fxdirector2_takedown_lookback.py) writes the revision's PRODUCTION BrnMomentTakedownLookback.cpp into
// fxd2_lookback.inc, which this file includes AFTER three explicit specializations that stand in for the behaviour
// manager (NewBehaviour<BehaviourRig>, the rig handle's GetProducedCamera and Release) -- so the moment's own code runs
// unchanged against a recording rig. VehicleRef (BrnVehicleRef.cpp), the RaceCarState record (BrnVehicleEvents.cpp)
// and the rig presets (BrnCameraRigParams.cpp) are the real ones.
//
// Against the ARTIST bodies:
//   Construct @0x8225EDB8   the inlined Moment::Construct, the handle cleared, the rig block Constructed, the victim
//                           ref cleared, mpParameters = 0
//   Update    @0x822662F0   SEARCHING: no takedown -> not met + head bit 18; a takedown binds the victim to
//                           GameState::meTakedownVictimID; the shot is on for a victim behind, within 60 m, not
//                           drawing level within a second; inhibited -> head bit 23 and nothing else; on -> the rig
//                           (NewBehaviour(handle, 0, this, 1)), CameraRig::ParamsBonnetLow, mbUseShake, SetParameters,
//                           StartLookingAtRaceCar(1, true), VALID. VALID: held (camera = the rig's) while behind and
//                           within 60 m, else back to SEARCHING and nothing else. Anything else asserts.
//   Release   @0x8223AA08   the handle released, not met, head bit 18, INACTIVE, true
// THE GEOMETRY is the console's: FxDirector2TakedownLookbackGolden.h is the Update's own words (SEARCHING
// 0x8226651C..0x822665B8, VALID 0x822663B8..0x82266458) run on the PPC/VMX128 emulator over 103 frames -- random ones,
// the 60 m and t = 1 edges, zero offset, zero closing speed -- 12 of which the vendor-form math (sequential dots,
// sqrtf, a / b) decides DIFFERENTLY. Every frame is checked twice: the ConsoleVpu helpers against the emulator bit
// for bit, and the production Update's decision in both states against the console's.
#include "GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.h"
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; gLastAssert = lpcText ? lpcText : ""; return 0; }
    void* EndAssert() { return nullptr; }
}
}

using namespace BrnDirector;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

// ---- recording stand-ins ------------------------------------------------------------------------------------
static int giNewBehaviours = 0, giProducedReads = 0, giHandleReleases = 0, giParamsConstructs = 0, giCameraConstructs = 0;
static const void* gpNewHandle = nullptr;
static const void* gpNewOwningState = reinterpret_cast<const void*>(1);
static const void* gpNewOwner = nullptr;
static s32 giNewArgB = -1;
static const void* gpParamsConstructed = nullptr;
alignas(16) static u8 gaRigSlot[sizeof(Camera::BehaviourRig) + 16];
static Camera::Camera gProducedCamera;

namespace BrnDirector
{
namespace Camera
{
    void Camera::Construct() { ++giCameraConstructs; }
    void ValidityAccount::MaskToFailFlags() {}   // Moment::PreUpdate's account mask (tested by run_fxdirector_moments.py)

    // The real body lives in BehaviourRig.cpp (run_fxdirector2_behaviour_rig.py). Here: the base's two stores (so
    // SetParameters' type tripwire holds) and a record of the block it ran on.
    void BehaviourRig::Parameters::Construct()
    {
        ++giParamsConstructs;
        gpParamsConstructed = this;
        Behaviour::Parameters::Construct();
        mType = eBehaviourRig;
    }

    // The behaviour manager, reduced to what the moment sees: a rig in a pool slot, and the handle naming it.
    template <>
    void BehaviourManager::NewBehaviour<BehaviourRig>(BehaviourHandle<BehaviourRig>& lrHandle, void* lpOwningState,
                                                      const void* lpOwner, s32 liArgB)
    {
        ++giNewBehaviours;
        gpNewHandle      = &lrHandle;
        gpNewOwningState = lpOwningState;
        gpNewOwner       = lpOwner;
        giNewArgB        = liArgB;
        std::memset(gaRigSlot, 0, sizeof(gaRigSlot));
        lrHandle.mbAllocated = true;
        lrHandle.mpBehaviour = reinterpret_cast<BehaviourRig*>(gaRigSlot);
    }

    template <>
    const Camera& BehaviourHandle<BehaviourRig>::GetProducedCamera() const
    {
        ++giProducedReads;
        return gProducedCamera;
    }

    template <>
    bool BehaviourHandle<BehaviourRig>::Release()
    {
        ++giHandleReleases;
        Clear();
        return true;
    }
}
    void Moment::SetParameters(const Parameters*) {}
    void Moment::Destruct() {}
}

#include "fxd2_lookback.inc"

#include "FxDirector2TakedownLookbackGolden.h"

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
static f32 Float(u32 luBits) { f32 lfValue; std::memcpy(&lfValue, &luBits, 4); return lfValue; }
static bool Same(f32 lfValue, u32 luGolden)
{
    const f32 lfGolden = Float(luGolden);
    if (lfGolden != lfGolden)
        return lfValue != lfValue;
    return Bits(lfValue) == luGolden;
}
static Vector3 V3(const u32 lauWords[3]) { return Vector3{ Float(lauWords[0]), Float(lauWords[1]), Float(lauWords[2]), 0.0f }; }
static bool Decide(f32 lfDistance, f32 lfZDistance, f32 lfTime)
{
    return ((0.0f > lfTime) || (lfTime > 1.0f)) && (0.0f > lfZDistance) && (60.0f > lfDistance);
}

alignas(16) static u8 gaMomentSlot[sizeof(MomentTakedownLookback) + 16];
alignas(16) static Camera::VehicleInfo gaCars[8];
static AllVehicleData gWorld;
static GameState gGameState;
static MomentSharedInfo gInfo;
static MomentTakedownLookback::Parameters gMomentParameters;
static u8 gaManager[64];   // the manager is only ever passed through to the stand-ins

static const s32 KI_VICTIM = 2;

static MomentTakedownLookback* FreshMoment()
{
    std::memset(gaMomentSlot, 0xCD, sizeof(gaMomentSlot));
    MomentTakedownLookback* lpMoment = new (gaMomentSlot) MomentTakedownLookback;
    Moment* lpBase = lpMoment;
    lpBase->Construct();
    lpBase->SetParameters(&gMomentParameters);
    lpBase->Prepare(gaManager);
    return lpMoment;
}

static void SetFrame(const LookbackCase& lrCase)
{
    Camera::VehicleInfo& lrPlayer = gInfo.mPlayerInfo;
    lrPlayer.mRaceCarState.mTransform.zAxis = V3(lrCase.mauPlayerZ);
    lrPlayer.mRaceCarState.mTransform.wAxis = V3(lrCase.mauPlayerPos);
    lrPlayer.mRaceCarState.mLinearVelocity  = V3(lrCase.mauPlayerVel);
    gaCars[KI_VICTIM].mRaceCarState.mTransform.wAxis = V3(lrCase.mauVictimPos);
    gaCars[KI_VICTIM].mRaceCarState.mLinearVelocity  = V3(lrCase.mauVictimVel);
}

static bool HeadFlag(MomentTakedownLookback* lpMoment, u32 luBit)
{
    return lpMoment->mCamera.mState.mHeadFlags.IsBitSet(luBit);
}

int main()
{
    namespace ConsoleVpu = BrnDirector::Camera::Utils::ConsoleVpu;

    gWorld.mpRaceCars = gaCars;
    gWorld.mePlayerRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    gWorld.mUsedRaceCars.UnSetAll();
    gWorld.mUsedRaceCars.SetBit(0u);
    gWorld.mUsedRaceCars.SetBit(static_cast<u32>(KI_VICTIM));
    gInfo.mpAllVehicleData = &gWorld;
    gInfo.mpGameState = &gGameState;
    gGameState.mbTakedownActive = false;
    gGameState.meTakedownVictimID = static_cast<EActiveRaceCarIndex>(KI_VICTIM);
    gProducedCamera.mfFOV = 42.5f;

    // ================= Construct @0x8225EDB8 over pool garbage =================
    std::memset(gaMomentSlot, 0xCD, sizeof(gaMomentSlot));
    MomentTakedownLookback* lpMoment = new (gaMomentSlot) MomentTakedownLookback;
    lpMoment->mRigCameraHandle.mbAllocated = true;
    lpMoment->mVictim.mbSet = true;
    Moment* lpBase = lpMoment;
    lpBase->Construct();
    Check(lpMoment->GetState() == Moment::E_STATE_INVALID_INACTIVE && lpMoment->GetType() == Moment::E_MOMENT_TAKEDOWN_LOOKBACK
              && !lpMoment->IsInhibited() && giCameraConstructs == 1,
          "K1 Construct: the inlined Moment::Construct -- INACTIVE, type 3 (vtable slot 7), not inhibited, Camera::Construct");
    Check(!lpMoment->mRigCameraHandle.mbAllocated && lpMoment->mRigCameraHandle.mpBehaviour == nullptr
              && giParamsConstructs == 1 && gpParamsConstructed == &lpMoment->mLookbackRigParams
              && !lpMoment->mVictim.mbSet && lpMoment->mpParameters == nullptr,
          "K2 Construct: the handle's five words zeroed, the rig block Constructed, the victim ref cleared (+0x2D0), "
          "mpParameters = 0");
    lpBase->SetParameters(&gMomentParameters);
    Check(lpMoment->mpParameters == &gMomentParameters && lpBase->Prepare(gaManager)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING,
          "K3 SetParameters stores the block; Prepare -> SEARCHING, true");

    // ================= SEARCHING without a takedown =================
    lpMoment->PreUpdate();
    lpBase->Update(1.0f / 30.0f, gaManager, &gInfo);
    Check(!lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 18)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING && giNewBehaviours == 0
              && !lpMoment->mVictim.mbSet,
          "K4 no takedown this frame (GameState::mbTakedownActive): not met, cannot switch to me, head bit 18, no rig, "
          "the victim not bound");

    // ================= every golden frame, SEARCHING then VALID =================
    gGameState.mbTakedownActive = true;
    const int liCases = static_cast<int>(sizeof(kaLookbackCases) / sizeof(kaLookbackCases[0]));
    int liVendorWrong = 0, liOn = 0;
    const MomentTakedownLookback* lpFirstOn = nullptr;
    for (int i = 0; i < liCases; ++i)
    {
        const LookbackCase& lrCase = kaLookbackCases[i];
        const Vector3 lPlayerZ = V3(lrCase.mauPlayerZ), lToVictim = V3(lrCase.mauVictimPos) - V3(lrCase.mauPlayerPos);
        const Vector3 lNetVictimVel = V3(lrCase.mauVictimVel) - V3(lrCase.mauPlayerVel);
        const f32 lfDistance = ConsoleVpu::Dot3(ConsoleVpu::NormalizeFast(lToVictim), lToVictim);
        const f32 lfZDistance = ConsoleVpu::Dot3(lPlayerZ, lToVictim);
        const f32 lfTime = ConsoleVpu::Divide(-lfZDistance, ConsoleVpu::Dot3(lPlayerZ, lNetVictimVel));
        const bool lbHelpers = Same(lfDistance, lrCase.muDistance) && Same(lfZDistance, lrCase.muZDistance)
                            && Same(lfTime, lrCase.muTime) && Same(lfZDistance, lrCase.muValidZDistance)
                            && Same(lfDistance, lrCase.muValidDistance);
        const bool lbConsoleOn = Decide(Float(lrCase.muDistance), Float(lrCase.muZDistance), Float(lrCase.muTime));
        if (lrCase.mbVendorDecidesDifferently)
            ++liVendorWrong;

        // SEARCHING
        MomentTakedownLookback* lpCase = FreshMoment();
        SetFrame(lrCase);
        const int liNewBefore = giNewBehaviours;
        lpCase->PreUpdate();
        static_cast<Moment*>(lpCase)->Update(1.0f / 30.0f, gaManager, &gInfo);
        const bool lbOn = lpCase->GetState() == Moment::E_STATE_VALID;
        const bool lbSearchingRight = (lbOn == lbConsoleOn) && ((giNewBehaviours - liNewBefore) == (lbOn ? 1 : 0))
            && lpCase->mVictim.mbSet && lpCase->mVictim.meType == BrnDirector::VehicleRef::E_RACE_CAR
            && lpCase->mVictim.miRaceCarIndex == KI_VICTIM
            && (lbOn ? lpCase->ConditionsAreMet() : (!lpCase->ConditionsAreMet() && HeadFlag(lpCase, 18)));
        if (lbOn)
        {
            ++liOn;
            if (lpFirstOn == nullptr)
                lpFirstOn = lpCase;
        }

        // VALID: the console holds while behind and within 60 m
        const bool lbConsoleHolds = (0.0f > Float(lrCase.muValidZDistance)) && (60.0f > Float(lrCase.muValidDistance));
        lpCase->SetState(Moment::E_STATE_VALID);
        lpCase->mRigCameraHandle.mbAllocated = true;
        lpCase->mRigCameraHandle.mpBehaviour = reinterpret_cast<Camera::BehaviourRig*>(gaRigSlot);
        lpCase->mCamera.mfFOV = -1.0f;
        const int liProducedBefore = giProducedReads;
        lpCase->PreUpdate();
        static_cast<Moment*>(lpCase)->Update(1.0f / 30.0f, gaManager, &gInfo);
        const bool lbHeld = lpCase->GetState() == Moment::E_STATE_VALID;
        const bool lbValidRight = (lbHeld == lbConsoleHolds)
            && (lbHeld ? (giProducedReads == liProducedBefore + 1 && lpCase->mCamera.mfFOV == 42.5f)
                       : (giProducedReads == liProducedBefore && lpCase->mCamera.mfFOV == -1.0f
                          && lpCase->GetState() == Moment::E_STATE_INVALID_SEARCHING && lpCase->ConditionsAreMet()));

        char lacName[220];
        std::snprintf(lacName, sizeof(lacName),
                      "G%03d %s: helpers %s the console; SEARCHING %s (console %s)%s; VALID %s (console %s)", i,
                      lrCase.mpcLabel, lbHelpers ? "==" : "!=", lbOn ? "on" : "off", lbConsoleOn ? "on" : "off",
                      lrCase.mbVendorDecidesDifferently ? " [the vendor forms decide the other way]" : "",
                      lbHeld ? "held" : "dropped", lbConsoleHolds ? "holds" : "drops");
        Check(lbHelpers && lbSearchingRight && lbValidRight, lacName);
    }
    std::printf("%d frames, %d shots on; the vendor forms decide %d of them the other way\n", liCases, liOn, liVendorWrong);
    Check(liVendorWrong > 0 && liOn > 0, "K5 the table discriminates (the vendor forms get frames wrong) and has shots on");

    // ================= the shot itself (an eligible frame) =================
    const LookbackCase* lpEligible = nullptr;
    for (const LookbackCase& lrCase : kaLookbackCases)
        if (Decide(Float(lrCase.muDistance), Float(lrCase.muZDistance), Float(lrCase.muTime)) && !lrCase.mbVendorDecidesDifferently)
        {
            lpEligible = &lrCase;
            break;
        }
    lpMoment = FreshMoment();
    SetFrame(*lpEligible);
    lpMoment->PreUpdate();
    static_cast<Moment*>(lpMoment)->Update(1.0f / 30.0f, gaManager, &gInfo);
    const Camera::BehaviourRig* lpRig = reinterpret_cast<const Camera::BehaviourRig*>(gaRigSlot);
    Check(gpNewHandle == &lpMoment->mRigCameraHandle && gpNewOwningState == nullptr && gpNewOwner == lpMoment
              && giNewArgB == 1,
          "K6 on: NewBehaviour<BehaviourRig>(the rig handle (+0x2B0), 0, this, 1) (0x822666AC)");
    Check(std::memcmp(&lpMoment->mLookbackRigParams.mRigParams, &Camera::Utils::CameraRig::ParamsBonnetLow,
                      sizeof(Camera::Utils::CameraRig::Params)) == 0
              && lpMoment->mLookbackRigParams.mbUseShake,
          "K7 the block's rig is CameraRig::ParamsBonnetLow (8 doublewords from 0x82CDAA10) and mbUseShake = 1 "
          "(`stb 1, 0x2A9`)");
    Check(lpRig->mpParameters == &lpMoment->mLookbackRigParams && !lpRig->mbIsPrepared,
          "K8 the rig is handed the block (BehaviourRig::SetParameters, 0x821F3B10)");
    Check(lpRig->mLookingAtRef.mbSet && lpRig->mLookingAtRef.meType == BrnDirector::VehicleRef::E_RACE_CAR
              && lpRig->mLookingAtRef.miRaceCarIndex == 1 && lpRig->mLookingAtRef.muRef == 0u && lpRig->mbLooking
              && lpRig->mbSnap && !lpRig->mbDetached,
          "K9 the rig looks at race car ONE and snaps (the six stores 0x82266700..0x82266714), not at the victim");
    Check(lpMoment->GetState() == Moment::E_STATE_VALID && lpMoment->ConditionsAreMet() && lpMoment->CanSwitchToMeNow(),
          "K10 on: VALID; the gates PreUpdate raised are left alone");

    // ---- inhibited ----
    lpMoment = FreshMoment();
    lpMoment->SetInhibited(true);
    lpMoment->mCamera.mState.mHeadFlags.UnSetAll();
    const int liNewBefore = giNewBehaviours;
    lpMoment->PreUpdate();
    static_cast<Moment*>(lpMoment)->Update(1.0f / 30.0f, gaManager, &gInfo);
    Check(giNewBehaviours == liNewBefore && !lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 23)
              && !HeadFlag(lpMoment, 18) && lpMoment->ConditionsAreMet()
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING,
          "K11 inhibited (+0x17B): no rig, cannot switch to me, head bit 23 (`oris 0x80`) -- and the conditions stay met");

    // ---- Release @0x8223AA08 ----
    lpMoment = FreshMoment();
    SetFrame(*lpEligible);
    lpMoment->PreUpdate();
    static_cast<Moment*>(lpMoment)->Update(1.0f / 30.0f, gaManager, &gInfo);
    lpMoment->PreUpdate();
    const int liReleasesBefore = giHandleReleases;
    const bool lbReleased = static_cast<Moment*>(lpMoment)->Release();
    Check(lbReleased && giHandleReleases == liReleasesBefore + 1 && !lpMoment->mRigCameraHandle.mbAllocated
              && !lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 18)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_INACTIVE,
          "K12 Release: the rig handle released, not met, cannot switch to me, head bit 18, INACTIVE, true");

    // ---- any other state asserts ----
    const unsigned luAssertsBefore = gAsserts;
    static_cast<Moment*>(lpMoment)->Update(1.0f / 30.0f, gaManager, &gInfo);
    Check(gAsserts == luAssertsBefore + 1 && gLastAssert == "unhandled case in switch",
          "K13 Update in INACTIVE trips \"unhandled case in switch\" (:158)");
    Check(gAsserts == 1, "K14 no other assert fired");

    std::printf("FxDirector2TakedownLookback: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
