#include "GameSource/Director/MomentController/Moments/BrnMomentPassengerSeesAction.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"   // the passenger-cam moment block

// BrnDirector::MomentPassengerSeesAction -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF primary file BrnMomentPassengerSeesAction.cpp;
// member names verbatim from the DecFIGS DWARF).
//
// Bodied here (7 ledger functions):
//   Construct @0x8225EE38   Prepare @0x821F75D0   Update @0x8225EEB8
//   Release   @0x8223AA80   SetParameters @0x821F75F0
//   GetName   @0x821F75F8   GetInstanceType @0x829DA908

namespace BrnDirector
{
    class AllVehicleData;   // BrnDirectorAllVehicleData.h (pointer-only reach here)

namespace
{
    // The crash window the passenger cam may start in / hold past (the 0.5s
    // rodata @0x82001DA0 both compares read).
    const f32 KF_PASSENGER_CRASH_WINDOW = 0.5f;

    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // oris 4
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // oris 8
    const u32 KU_HEAD_FLAG_PREPARING = 20;   // oris 0x10 (also the valid body's expired hold)
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // oris 0x80
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; X360 shared-info offsets in comments). ----
    bool MomentSharedInfo_IsPlayerCrashing(const void* lpSharedInfo);        // +1284 byte 249
    const AllVehicleData* MomentSharedInfo_GetAllVehicleData(const void* lpSharedInfo);   // +1288
    s32  MomentSharedInfo_GetCrashVehicleIndex(const void* lpSharedInfo);    // +1304 word
}
using namespace detail;

// @ 0x8225EE38 -- cpp:34. The inlined base Moment::Construct, the handle clear,
// the two vehicle-ref set-flag clears, and the parameter reset.
void MomentPassengerSeesAction::Construct()
{
    Moment::Construct();      // inlined on the X360 (state/type/inhibit/camera)
    mPassengerCam.Clear();    // the X360 zeroes the five handle fields inline
    mWitness.mbSet  = 0;
    mIncident.mbSet = 0;
    mpParameters    = 0;
}

// @ 0x821F75D0 -- cpp:55.
bool MomentPassengerSeesAction::Prepare(void* /*lrBehaviourController*/)
{
    mfTimeCrashing = 0.0f;
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// @ 0x8223AA80 -- cpp:211. The inlined guarded handle Release, the gate clears,
// the searching head bit, park at INACTIVE (0).
bool MomentPassengerSeesAction::Release()
{
    mPassengerCam.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_INACTIVE);
    return true;
}

// @ 0x821F75F0 -- cpp:243.
void MomentPassengerSeesAction::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

// @ 0x821F75F8 -- cpp:269.
const char* MomentPassengerSeesAction::GetName() const
{
    return "MomentPassengerSeesAction";
}

// @ 0x829DA908.
Moment::EType MomentPassengerSeesAction::GetInstanceType()
{
    return E_MOMENT_PASSENGER_SEES_ACTION;
}

// @ 0x8225EEB8 -- cpp:74. Every frame the crash timer integrates while crashing
// (else clears). Then:
//   SEARCHING        in a crash's first 0.5s: seed the incident ref to the raw
//                    -1 record, resolve it against the vehicle data (the X360
//                    calls Get TWICE back-to-back -- kept), and require the
//                    crash vehicle index unchanged across the resolve; then
//                    (unless inhibited, bit 23) point the witness ref at the
//                    crash car, allocate the passenger cam with the bank's
//                    passenger block, and enter FOUND_PREPARING (bit 19).
//   FOUND_PREPARING  failed -> the virtual Release; not switchable -> the
//                    shared bit-20 hold; else VALID (and run its body now).
//   VALID            mirror the produced camera; past the 0.5s window take the
//                    shared bit-20 hold; once the crash ends, the virtual
//                    Release + back to SEARCHING.
void MomentPassengerSeesAction::Update(f32 lfTimeStep, void* lrBehaviourController,
                                       const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    mfTimeCrashing = MomentSharedInfo_IsPlayerCrashing(lSharedInfo)
                         ? mfTimeCrashing + lfTimeStep
                         : 0.0f;

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        bool lbStart = false;
        if (MomentSharedInfo_IsPlayerCrashing(lSharedInfo)
            && mfTimeCrashing < KF_PASSENGER_CRASH_WINDOW)
        {
            const s32 liCrashVehicle = MomentSharedInfo_GetCrashVehicleIndex(lSharedInfo);
            const AllVehicleData* lpAllVehicles = MomentSharedInfo_GetAllVehicleData(lSharedInfo);

            // Seed the incident ref to the raw invalid record then resolve it
            // (the X360 issues the resolve twice, back to back -- kept), and
            // require the crash vehicle unchanged across the resolve.
            mIncident.mbSet          = 1;
            mIncident.meType         = VehicleRef::EType(0);
            mIncident.muRef          = 0;
            mIncident.miRaceCarIndex = -1;
            mIncident.Get(lpAllVehicles);
            mIncident.Get(lpAllVehicles);
            lbStart = liCrashVehicle == MomentSharedInfo_GetCrashVehicleIndex(lSharedInfo);

            if (lbStart)
            {
                if (IsInhibited())
                {
                    SetCanSwitchToMeNow(false);
                    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
                    return;
                }
                mWitness.SetToRaceCar(EActiveRaceCarIndex(liCrashVehicle));
                lpBehaviourManager->NewBehaviour<Camera::BehaviourPassengerCam>(
                    mPassengerCam, 0, this, 1);
                mPassengerCam.GetBehaviour()->SetParameters(
                    &lpBehaviourManager->GetBehaviourParameterBank().GetPassengerCamMomentParams());
                SetCanSwitchToMeNow(false);
                SetState(E_STATE_INVALID_FOUND_PREPARING);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
                return;
            }
        }

        SetConditionsNotMet();
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        return;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mPassengerCam.GetBehaviour()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            return;
        }
        if (!mPassengerCam.GetBehaviour()->CanSwitchToMeNow())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            return;
        }
        SetState(E_STATE_VALID);
        break;   // fall into the valid body this frame

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :198 (non-gating)
        return;
    }

    // ---- the VALID body ----
    SetCamera(mPassengerCam.GetProducedCamera());
    if (mfTimeCrashing > KF_PASSENGER_CRASH_WINDOW)
    {
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
        return;
    }
    if (!MomentSharedInfo_IsPlayerCrashing(lSharedInfo))
    {
        Release();   // the live-vtable call (slot 4)
        SetState(E_STATE_INVALID_SEARCHING);
    }
}

}
