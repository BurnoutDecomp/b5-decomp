#include "GameSource/Director/MomentController/Moments/BrnMomentStationaryCrash.h"

#include <cmath>                                                      // fabsf (the rotation wrap test)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                            // rw::math::vpu::Dot
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"         // Attrib::Gen::shotgroup (Num_ShotList / GetShotListData)
// NOTE: the DirectorResourceManager reached below is BrnBehaviourIceAnim.h's
// minimal slice (grown with the crash banks + GetKeyAnimFromGuid) -- the real
// BrnDirectorResourceManager.h home is MUTUALLY EXCLUSIVE with that slice.

// BrnDirector::MomentStationaryCrash -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentStationaryCrash.cpp; member names verbatim from
// the DecFIGS DWARF).
//
// Bodied here (5 ledger functions):
//   Construct @0x8225F518   Prepare @0x821F7688   Update  @0x82272EA8
//   Release   @0x8223B148   GetName @0x821F76B0

namespace BrnDirector
{

namespace
{
    const f32 KF_TWO_PI  = 6.2831855f;    // a full integrated turn -> the tumbling variant
    const f32 KF_HALF_PI = 1.5707964f;    // max rotation rate for the stationary take
    const f32 KF_MAX_SPEED_SQ = 100.0f;   // |velocity|^2 eligibility bound (the splat/vcmpgtfp compare)

    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // oris 4
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // oris 8
    const u32 KU_HEAD_FLAG_PREPARING = 20;   // oris 0x10
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // oris 0x80
    const u32 KU_HEAD_FLAG_VALID     = 28;   // oris 0x1000

    const u32 KU_STATE_FLAG_KEEP_GATE = 1;   // mState current flag gating stay-valid
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the Moment base type-erases
    // it to const void*). DECLARATION-ONLY named helpers per the moment-family
    // precedent; X360 shared-info offsets in comments; role names FLAG-inferred. ----
    bool MomentSharedInfo_IsPlayerCrashing(const void* lpSharedInfo);          // +1284 byte 249
    f32  MomentSharedInfo_GetTimeCrashing(const void* lpSharedInfo);           // +1284 f32 +252
    f32  MomentSharedInfo_GetTimestep(const void* lpSharedInfo);               // +1312 f32
    const DirectorResourceManager*
         MomentSharedInfo_GetDirectorResourceManager(const void* lpSharedInfo); // +1340

    // Vehicle-dynamics block (+1292) vector lanes (each a 16-byte Vector3 the X360
    // lvx128s straight into the two vmsum3fp128 dot products):
    const rw::math::vpu::Vector3& MomentSharedInfo_GetCrashRotationAxis(const void* lpSharedInfo);   // +1292 vec +528
    const rw::math::vpu::Vector3& MomentSharedInfo_GetLinearVelocity(const void* lpSharedInfo);      // +1292 vec +816
    const rw::math::vpu::Vector3& MomentSharedInfo_GetAngularVelocity(const void* lpSharedInfo);     // +1292 vec +832

    // The ICE take's play length (take data +0x2C f32; the X360 compares it, less
    // 0.1s, against the time already spent crashing). DECLARATION-ONLY (the
    // ICETakeData field surface is owned by the ICE data TUs).
    f32 ICETakeData_GetDuration(const ICE::ICETakeData* lpTakeData);

    // The Gen::iceanim take guid carried in a raw ShotList data block (data +0xC;
    // the X360 wraps the block in a stack Gen::iceanim and reads its
    // mpAttributeData +0xC). DECLARATION-ONLY.
    s32 IceAnimShotData_GetAnimGuid(const void* lpShotData);
}
using namespace detail;

// @ 0x8225F518 -- cpp:34. The inlined base Moment::Construct, the handle clear,
// and the take/rotation/tumbling/parameters resets (mfTimeCrashing is NOT touched
// here -- Prepare seeds it).
void MomentStationaryCrash::Construct()
{
    Moment::Construct();        // inlined on the X360 (state/type/inhibit/camera)
    mIceCameraHandle.Clear();   // the X360 zeroes the five handle fields inline
    muTake            = 0;
    mfRotationAngle   = 0.0f;
    mbIsTumblingCrash = false;
    mpParameters      = 0;
}

// @ 0x821F7688 -- cpp:56.
bool MomentStationaryCrash::Prepare(void* /*lrBehaviourController*/)
{
    mfTimeCrashing = 0.0f;
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// @ 0x8223B148 -- cpp:196. The inlined guarded BehaviourHandle::Release, then back
// to SEARCHING.
bool MomentStationaryCrash::Release()
{
    mIceCameraHandle.Release();
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// @ 0x821F76B0 -- cpp:252.
const char* MomentStationaryCrash::GetName() const
{
    return "MomentStationaryCrash";
}

// @ 0x82272EA8 -- cpp:75. Every frame: integrate the crash rotation (the
// angular-velocity . rotation-axis dot, scaled by the shared timestep) while the
// player is crashing, latching the tumbling variant past a full turn. Then:
//   SEARCHING        eligible while (nearly) stationary (|v|^2 < 100) and the
//                    rotation rate is below pi/2: resolve the tumbling- or
//                    stationary-crash shot group, read its first ICE take, and --
//                    if the take still fits in the crash (duration - 0.1s vs the
//                    time already crashing) and the moment is not inhibited --
//                    allocate the ICE-anim behaviour with that shot.
//   FOUND_PREPARING  failed -> Release (the virtual) + SEARCHING; not yet
//                    switchable -> hold; switchable -> VALID and RUN THE VALID
//                    BODY THE SAME FRAME (the X360 falls through).
//   VALID            mirror the produced camera; drop back to SEARCHING when the
//                    keep gate clears or the take finishes/fails.
void MomentStationaryCrash::Update(f32 /*lfTimeStep*/, void* lrBehaviourController,
                                   const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    // ---- the per-frame rotation integrator (before the state machine) ----
    const f32 lfRotationRate = rw::math::vpu::Dot(
        MomentSharedInfo_GetAngularVelocity(lSharedInfo),
        MomentSharedInfo_GetCrashRotationAxis(lSharedInfo));
    if (MomentSharedInfo_IsPlayerCrashing(lSharedInfo))
    {
        mfRotationAngle += MomentSharedInfo_GetTimestep(lSharedInfo) * lfRotationRate;
        if (fabsf(mfRotationAngle) > KF_TWO_PI)
            mbIsTumblingCrash = true;
    }
    else
    {
        mbIsTumblingCrash = false;
        mfRotationAngle   = 0.0f;
    }

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        const rw::math::vpu::Vector3& lv3Velocity =
            MomentSharedInfo_GetLinearVelocity(lSharedInfo);
        if (KF_MAX_SPEED_SQ > rw::math::vpu::Dot(lv3Velocity, lv3Velocity)
            && lfRotationRate < KF_HALF_PI)
        {
            const DirectorResourceManager* lpResourceManager =
                lpBehaviourManager->GetDirectorResourceManager();
            const Attrib::Gen::shotgroup& lrShotGroup = mbIsTumblingCrash
                ? lpResourceManager->GetTumblingCrashShots()      // rm +1448
                : lpResourceManager->GetStationaryCrashShots();   // rm +1464

            CGS_ASSERT(lrShotGroup.Num_ShotList() > 0,
                       "lpShotGroup->Num_ShotList() > 0");   // :110 (non-gating)

            // The group's first ShotList block (the X360 builds a stack
            // Gen::iceanim over it; the 24-byte null-element fallback is the
            // caller's here).
            const void* lpShotData = lrShotGroup.GetShotListData(0);
            if (lpShotData == 0)
                lpShotData = Attrib::DefaultDataArea(0x18u);

            ICE::ICETakeData* lpTakeData =
                MomentSharedInfo_GetDirectorResourceManager(lSharedInfo)
                    ->GetKeyAnimFromGuid(IceAnimShotData_GetAnimGuid(lpShotData));
            CGS_ASSERT(lpTakeData != 0, "lpTakeData != NULL");   // :114 (non-gating)

            if (ICETakeData_GetDuration(lpTakeData) - 0.1f
                < MomentSharedInfo_GetTimeCrashing(lSharedInfo))
            {
                // Too far into the crash for the take to fit.
                SetConditionsNotMet();
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
            }
            else if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            }
            else
            {
                lpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mIceCameraHandle, 0, this, 1);
                mIceCameraHandle.GetBehaviour()->SetParameters(
                    reinterpret_cast<Camera::BehaviourIceAnim::ShotReference*>(
                        const_cast<void*>(lpShotData)));
                mIceCameraHandle.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28
                SetCanSwitchToMeNow(false);
                SetState(E_STATE_INVALID_FOUND_PREPARING);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
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

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mIceCameraHandle.GetBehaviour()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
            break;
        }
        if (!mIceCameraHandle.GetBehaviour()->CanSwitchToMeNow())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            break;
        }
        SetState(E_STATE_VALID);
        // fall through -- the X360 runs the VALID body the same frame

    case E_STATE_VALID:
        SetCamera(mIceCameraHandle.GetProducedCamera());
        SetCanSwitchFromMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_VALID);

        if (!GetNonConstCamera().mState.IsFlagSet(KU_STATE_FLAG_KEEP_GATE)
            || mIceCameraHandle.GetBehaviour()->HasFinishedOrFailed())
        {
            Release();   // the live-vtable call (slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
        }
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :183 (non-gating)
        break;
    }
}

}
