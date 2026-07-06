#include "GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.h"

#include <cmath>                                                      // sqrtf (the |delta| magnitude)
#include <cstring>                                                    // memcpy (the authored-default rig-params copy)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "BrnCommonTypes.h"                                           // Vector3
#include "GameSource/Director/Camera/Camera.h"                        // Camera::Camera (SetCamera / operator=)
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex

// BrnDirector::MomentTakedownLookback -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentTakedownLookback.cpp; member names verbatim from the
// DecFIGS DWARF).
//
// Bodied here (4 ledger functions):
//   Construct @0x8225EDB8   Update  @0x822662F0
//   Release   @0x8223AA08   GetName @0x821F75C0

namespace BrnDirector
{

namespace
{
    // XEX rodata leaf floats (values proven from the decrypted image, see the
    // committed sibling reaches): the along-segment band edges and the eligibility
    // range the look-back holds the victim inside.
    const f32 KF_ZERO      = 0.0f;    // flt_82001CC0 (band near edge / dot-sign test)
    const f32 KF_ONE       = 1.0f;    // flt_82001C98 (band far edge)
    const f32 KF_MAX_RANGE = 60.0f;   // flt_82004C6C (max victim distance, metres)

    // Camera-state head bits the moment raises (the moment family's shared per-bit
    // vocabulary -- BrnCameraState.h; bit roles not yet recovered):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // oris 0x40000   searching / condition absent
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // oris 0x800000  searching inhibited

    // The authored default lookback-rig parameter sub-block the X360 copies verbatim
    // from rodata (unk_82CDAA10: 8 qwords == 64 bytes) into mLookbackRigParams at its
    // +0x10 sub-offset, then raises one bool at +0x119. The blob's field surface is
    // BehaviourRig::Parameters interior; its 64 leaf bytes are NOT individually
    // name-attested from this TU's asm, so they are carried as an opaque authored
    // default. FLAG: the values are the zero-image placeholder (the rodata dump for
    // 0x82CDAA10 is not available); the COPY + its sub-offset/flag are asm-attested.
    const u8 KaLookbackRigParamsDefault[64] = { 0 };      // unk_82CDAA10 (@0x82CDAA10)
    const u32 KU_LOOKBACK_RIG_PARAMS_SUB_OFFSET  = 0x10;  // copy target: mLookbackRigParams + 0x10
    const u32 KU_LOOKBACK_RIG_PARAMS_FLAG_OFFSET = 0x119; // stb 1 at mLookbackRigParams + 0x119
}

namespace detail
{
    // ---- MomentSharedInfo reaches (the record is un-homed; the Moment base
    // type-erases it to const void*). DECLARATION-ONLY named helpers per the
    // moment-family precedent (BrnMomentStationaryCrash.cpp's detail reach); X360
    // shared-info offsets in comments; role names FLAG-inferred from the uses. ----

    // The taken-down victim descriptor (+1284). While it reports a live victim the
    // look-back can search; it names the victim's race-car index.
    bool MomentSharedInfo_HasTakedownVictim(const void* lpSharedInfo);        // +1284 byte +0xDA (218)
    s32  MomentSharedInfo_GetVictimRaceCarIndex(const void* lpSharedInfo);    // +1284 word +0xE0 (224)

    // The "world" context the VehicleRef resolver needs (+1288).
    const void* MomentSharedInfo_GetWorld(const void* lpSharedInfo);          // +1288

    // Player-frame vectors the geometry reads (each a 16-byte Vector3 the X360
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

    // ---- Rig-handle resolvers (the BrnMomentStationaryCrash.cpp precedent): the
    // de-inlined per-type BehaviourHandle accessors the X360 bl's. ----
    Camera::BehaviourRig* sub_821FCFB8(void* lpHandle);        // GetBehaviour<BehaviourRig>   @0x821FCFB8
    const Camera::Camera& sub_821FD020(void* lpHandle);        // GetProducedCamera            @0x821FD020

    // Aim the freshly allocated rig at race car #1 and snap. The X360 inlines the
    // rig's setters into the moment (mLookingAtRef @rig+0x450 = {E_RACE_CAR,1,0,set},
    // mbLooking @+0x46A = true, mbSnap @+0x46B = true); expressed as a named helper
    // (the BrnMomentHardStop.cpp Behaviour_SetUseCollisionPolicy precedent) because
    // those rig members are private. DECLARATION-ONLY.
    void BehaviourRig_StartLookingAtRaceCarSnapped(Camera::BehaviourRig* lpRig);

    // dot3 / subtract / length of Vector3 lanes (the X360 vmsum3fp128 / vsubfp /
    // rsqrt-refined magnitude).
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

// @ 0x8225EDB8 -- cpp:34. The inlined base Moment::Construct, the rig handle clear,
// the authored lookback-rig Parameters::Construct, the victim clear, and the
// parameters reset.
void MomentTakedownLookback::Construct()
{
    Moment::Construct();          // inlined on the X360 (state/type/inhibit/camera)
    mRigCameraHandle.Clear();     // the X360 zeroes the five handle fields inline
    mLookbackRigParams.Construct();
    mVictim.mbSet = false;        // the X360 clears the victim ref's set byte (+0x2D0)
    mpParameters  = 0;
}

// @ 0x821F75C0 -- cpp:229.
const char* MomentTakedownLookback::GetName() const
{
    return "MomentTakedownLookback";
}

// @ 0x8223AA08 -- cpp:171. The inlined guarded rig-handle release, the gates
// dropped, the searching head bit raised, then back to INACTIVE (this moment parks
// at state 0, like MomentTumbling::Release -- not its siblings' SEARCHING).
bool MomentTakedownLookback::Release()
{
    mRigCameraHandle.Release();     // the inlined guarded manager-hold drop + clear
    SetConditionsNotMet();          // stb 0, +0x17A
    SetCanSwitchToMeNow(false);     // stb 0, +0x178
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);   // oris 0x40000 @+0x148
    SetState(E_STATE_INVALID_INACTIVE);   // stw 0, +0x174
    return true;
}

// @ 0x822662F0 -- cpp:72. The per-frame look-back state machine:
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

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   // :74 (non-gating)

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

        // The parametric projection of the victim onto the player segment (the X360
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

                // Copy the authored default rig sub-block (unk_82CDAA10) into the
                // lookback rig parameters, then raise its one authored bool.
                std::memcpy(reinterpret_cast<u8*>(&mLookbackRigParams)
                                + KU_LOOKBACK_RIG_PARAMS_SUB_OFFSET,
                            KaLookbackRigParamsDefault, sizeof(KaLookbackRigParamsDefault));
                reinterpret_cast<u8*>(&mLookbackRigParams)[KU_LOOKBACK_RIG_PARAMS_FLAG_OFFSET] = 1;

                sub_821FCFB8(&mRigCameraHandle)->SetParameters(&mLookbackRigParams);

                // Aim the rig's look-at at the taken-down race car and snap (the X360
                // resolves the rig again, then inlines its look-at/snap setters).
                BehaviourRig_StartLookingAtRaceCarSnapped(sub_821FCFB8(&mRigCameraHandle));

                SetState(E_STATE_VALID);   // stw 3, +0x174
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
            SetCamera(sub_821FD020(&mRigCameraHandle));   // mCamera = rig produced camera
        }
        else
        {
            SetState(E_STATE_INVALID_SEARCHING);
        }
        break;
    }

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :158 (non-gating)
        break;
    }
}

}
