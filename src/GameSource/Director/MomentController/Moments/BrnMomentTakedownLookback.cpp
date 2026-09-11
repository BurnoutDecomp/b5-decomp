#include "GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.h"

#include <cmath>                                                      // sqrtf (the |delta| magnitude)
#include <cstring>                                                    // memcpy (the authored-default rig-params copy)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "BrnCommonTypes.h"                                           // Vector3
#include "GameSource/Director/Camera/Camera.h"                        // Camera::Camera (SetCamera / operator=)
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex

// BrnDirector::MomentTakedownLookback -- reconstructed from the console executable
// (home file BrnMomentTakedownLookback.cpp; member names verbatim from
// the declarations).
//
// Bodied here (4 ledger functions):
//   Construct   Update
//   Release   GetName

namespace BrnDirector
{

namespace
{
    // Read-only-data leaf floats (values proven from the console image, see the
    // committed sibling reaches): the along-segment band edges and the eligibility
    // range the look-back holds the victim inside.
    const f32 KF_ZERO      = 0.0f;    // band near edge / dot-sign test
    const f32 KF_ONE       = 1.0f;    // band far edge
    const f32 KF_MAX_RANGE = 60.0f;   // max victim distance, metres

    // Camera-state head bits the moment raises (the moment family's shared per-bit
    // vocabulary -- BrnCameraState.h; bit roles not yet recovered):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // searching / condition absent
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // searching inhibited

    // The authored default lookback-rig parameter sub-block the console build copies verbatim
    // from read-only data (8 doublewords == 64 bytes) into mLookbackRigParams at its
    // +0x10 sub-offset, then raises one bool at +0x119. The blob's field surface is
    // BehaviourRig::Parameters interior; its 64 leaf bytes are NOT individually
    // name-attested from this TU's console code, so they are carried as an opaque authored
    // default. FLAG: the values are the zero-image placeholder (that read-only-data block is
    // not available); the COPY + its sub-offset/flag are console-attested.
    const u8 KaLookbackRigParamsDefault[64] = { 0 };
    const u32 KU_LOOKBACK_RIG_PARAMS_SUB_OFFSET  = 0x10;  // copy target: mLookbackRigParams + 0x10
    const u32 KU_LOOKBACK_RIG_PARAMS_FLAG_OFFSET = 0x119; // flag raised at mLookbackRigParams + 0x119
}

namespace detail
{
    // ---- MomentSharedInfo reaches (the record is un-homed; the Moment base
    // type-erases it to const void*). DECLARATION-ONLY named helpers per the
    // moment-family precedent (BrnMomentStationaryCrash.cpp's detail reach); console
    // shared-info offsets in comments; role names FLAG-inferred from the uses. ----

    // The taken-down victim descriptor (+1284). While it reports a live victim the
    // look-back can search; it names the victim's race-car index.
    bool MomentSharedInfo_HasTakedownVictim(const void* lpSharedInfo);        // +1284 byte +0xDA (218)
    s32  MomentSharedInfo_GetVictimRaceCarIndex(const void* lpSharedInfo);    // +1284 word +0xE0 (224)

    // The "world" context the VehicleRef resolver needs (+1288).
    const void* MomentSharedInfo_GetWorld(const void* lpSharedInfo);          // +1288

    // Player-frame vectors the geometry reads (each a 16-byte Vector3 the console build
    // lvx128s straight out of the shared info; the SAME lanes are read out of the
    // resolved victim object). Roles FLAG-inferred:
    //   +0x210 (528)  the player's forward / along-track direction
    //   +0x220 (544)  the player's world position
    //   +0x330 (816)  a second along-segment reference position
    const Vector3& MomentSharedInfo_GetForward(const void* lpSharedInfo);          // +0x210
    const Vector3& MomentSharedInfo_GetPosition(const void* lpSharedInfo);         // +0x220
    const Vector3& MomentSharedInfo_GetSegmentReference(const void* lpSharedInfo); // +0x330

    // The same two lanes off a resolved vehicle object (VehicleRef::Get's return).
    const Vector3& Vehicle_GetPosition(const void* lpVehicle);               // +0x220
    const Vector3& Vehicle_GetSegmentReference(const void* lpVehicle);       // +0x330

    // ⛔ THE TWO RIG-HANDLE RESOLVER SHIMS ARE GONE, on the BrnArbStateDriveThru.cpp
    // precedent: they were the de-inlined per-instantiation copies of two accessors this tree
    // already owns, and mRigCameraHandle is ALREADY the canonical
    // Camera::BehaviourHandle<Camera::BehaviourRig>, so both were reachable by name all along --
    // GetBehaviour() and GetProducedCamera(). Three mounted arbitrator states call them that way.

    // Aim the freshly allocated rig at race car #1 and snap. The console inlines the
    // rig's setters into the moment (mLookingAtRef @rig+0x450 = {E_RACE_CAR,1,0,set},
    // mbLooking @+0x46A = true, mbSnap @+0x46B = true); expressed as a named helper
    // (the BrnMomentHardStop.cpp Behaviour_SetUseCollisionPolicy precedent) because
    // those rig members are private. DECLARATION-ONLY.
    void BehaviourRig_StartLookingAtRaceCarSnapped(Camera::BehaviourRig* lpRig);

    // dot3 / subtract / length of Vector3 lanes (the console build uses the three-lane
    // vector dot, a vector subtract, and a reciprocal-square-root-refined magnitude).
    inline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
    }
    inline Vector3 Sub3(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lResult;
        lResult.x = lrA.x - lrB.x;
        lResult.y = lrA.y - lrB.y;
        lResult.z = lrA.z - lrB.z;
        return lResult;
    }
    inline f32 Length3(const Vector3& lrV) { return sqrtf(Dot3(lrV, lrV)); }
}
using namespace detail;

// The inlined base Moment::Construct, the rig handle clear,
// the authored lookback-rig Parameters::Construct, the victim clear, and the
// parameters reset.
void MomentTakedownLookback::Construct()
{
    Moment::Construct();          // inlined in the console build (state/type/inhibit/camera)
    mRigCameraHandle.Clear();     // the console build zeroes the five handle fields inline
    mLookbackRigParams.Construct();
    mVictim.mbSet = false;        // the console build clears the victim ref's set byte (+0x2D0)
    mpParameters  = 0;
}

const char* MomentTakedownLookback::GetName() const
{
    return "MomentTakedownLookback";
}

// The inlined guarded rig-handle release, the gates
// dropped, the searching head bit raised, then back to INACTIVE (this moment parks
// at state 0, like MomentTumbling::Release -- not its siblings' SEARCHING).
bool MomentTakedownLookback::Release()
{
    mRigCameraHandle.Release();     // the inlined guarded manager-hold drop + clear
    SetConditionsNotMet();          // 0 -> +0x17A
    SetCanSwitchToMeNow(false);     // 0 -> +0x178
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);   // @+0x148
    SetState(E_STATE_INVALID_INACTIVE);   // 0 -> +0x174
    return true;
}

// The per-frame look-back state machine:
//   SEARCHING  eligible only while a takedown victim is live; bind the victim to
//              its race car, resolve its world transform, and test the geometry:
//              the victim's projection along the player segment must fall OUTSIDE
//              the [0,1] band AND the victim must be behind the player (the along-
//              track dot is negative) AND within ~60m. When eligible (and not
//              inhibited): allocate the rig behaviour, push the authored lookback
//              rig parameters, aim the rig's look-at at the taken-down race car,
//              and enter VALID. When inhibited: raise the inhibited head bit.
//   VALID      hold while the victim stays behind (the along-track dot is negative)
//              and within ~60m, mirroring the rig-produced camera; otherwise drop
//              back to SEARCHING.
void MomentTakedownLookback::Update(f32 /*lfTimeStep*/, void* lrBehaviourController,
                                    const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   //  (non-gating)

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        // ---- eligibility gate: a live takedown victim ----
        if (!MomentSharedInfo_HasTakedownVictim(lSharedInfo))
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
            break;
        }

        // Bind the victim ref to the taken-down race car, resolve its transform.
        mVictim.SetToRaceCar(static_cast<EActiveRaceCarIndex>(
            MomentSharedInfo_GetVictimRaceCarIndex(lSharedInfo)));

        const void* lpWorld  = MomentSharedInfo_GetWorld(lSharedInfo);
        const void* lpVictim = mVictim.Get(lpWorld);

        // delta  = victim position - player position            (the +0x220 lanes)
        // delta2 = victim segment-ref - player segment-ref      (the +0x330 lanes)
        const Vector3& lrForward = MomentSharedInfo_GetForward(lSharedInfo);
        const Vector3  lDelta  = Sub3(Vehicle_GetPosition(lpVictim),
                                      MomentSharedInfo_GetPosition(lSharedInfo));
        const Vector3  lDelta2 = Sub3(Vehicle_GetSegmentReference(lpVictim),
                                      MomentSharedInfo_GetSegmentReference(lSharedInfo));

        const f32 lfAlongDot = Dot3(lrForward, lDelta);    // dot(forward, delta)
        const f32 lfSegDot   = Dot3(lrForward, lDelta2);   // dot(forward, delta2)
        const f32 lfDist     = Length3(lDelta);            // |delta|

        // The parametric projection of the victim onto the player segment (the console build
        // reciprocal-of-dot term, sign-flipped): outside [0,1] == the victim is not
        // between the two segment references.
        const f32 lfProjection = -lfAlongDot * (1.0f / lfSegDot);

        const bool lbOutsideBand = (KF_ZERO > lfProjection) || (lfProjection > KF_ONE);
        const bool lbBehind      = (KF_ZERO > lfAlongDot);
        const bool lbInRange     = (KF_MAX_RANGE > lfDist);

        if (lbOutsideBand && lbBehind && lbInRange)
        {
            if (!IsInhibited())
            {
                // ---- allocate the rig, push the authored lookback rig params ----
                lpBehaviourManager->NewBehaviour<Camera::BehaviourRig>(
                    mRigCameraHandle, 0, this, 1);

                // Copy the authored default rig sub-block into the
                // lookback rig parameters, then raise its one authored bool.
                std::memcpy(reinterpret_cast<u8*>(&mLookbackRigParams)
                                + KU_LOOKBACK_RIG_PARAMS_SUB_OFFSET,
                            KaLookbackRigParamsDefault, sizeof(KaLookbackRigParamsDefault));
                reinterpret_cast<u8*>(&mLookbackRigParams)[KU_LOOKBACK_RIG_PARAMS_FLAG_OFFSET] = 1;

                mRigCameraHandle.GetBehaviour()->SetParameters(&mLookbackRigParams);

                // Aim the rig's look-at at the taken-down race car and snap (the console build
                // resolves the rig again, then inlines its look-at/snap setters).
                BehaviourRig_StartLookingAtRaceCarSnapped(mRigCameraHandle.GetBehaviour());

                SetState(E_STATE_VALID);   // 3 -> +0x174
            }
            else
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            }
        }
        else
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        }
        break;
    }

    case E_STATE_VALID:
    {
        const void* lpWorld  = MomentSharedInfo_GetWorld(lSharedInfo);
        const void* lpVictim = mVictim.Get(lpWorld);

        const Vector3& lrForward = MomentSharedInfo_GetForward(lSharedInfo);
        const Vector3  lDelta = Sub3(Vehicle_GetPosition(lpVictim),
                                     MomentSharedInfo_GetPosition(lSharedInfo));

        const f32 lfAlongDot = Dot3(lrForward, lDelta);
        const f32 lfDist     = Length3(lDelta);

        // Hold only while the victim stays behind (along-track dot negative) AND in
        // range; either failing drops back to SEARCHING.
        if ((KF_ZERO > lfAlongDot) && (KF_MAX_RANGE > lfDist))
        {
            SetCamera(mRigCameraHandle.GetProducedCamera());   // mCamera = rig produced camera
        }
        else
        {
            SetState(E_STATE_INVALID_SEARCHING);
        }
        break;
    }

    default:
        CGS_ASSERT(false, "unhandled case in switch");   //  (non-gating)
        break;
    }
}

}
