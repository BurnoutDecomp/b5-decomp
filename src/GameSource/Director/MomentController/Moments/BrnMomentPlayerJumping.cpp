#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"

#include <math.h>                                                    // fabsf (the grounded check)
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"    // the jump rig/bystander blocks
#include "GameSource/Director/BrnDirectorICEWrapper.h"               // ICEWrapper (the resource-manager header's inlines need it complete first)
#include "GameSource/Director/BrnDirectorResourceManager.h"          // the player-jumping shot group
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"        // Attrib::Gen::shotgroup + Attrib::DefaultDataArea

// BrnDirector::MomentPlayerJumping -- reconstructed from
// the console executable (home file BrnMomentPlayerJumping.cpp;
// member/constant names verbatim from the declarations).
//
// Bodied here (8 ledger functions):
//   Construct    Prepare        Update
//   Release    PrepareCameras
//   GetCameraStatus   GetName   GetInstanceType

namespace BrnDirector
{

namespace
{
    // The file-scope constants, values read from the console build's read-only data.
    // FLAG (name->value mapping inferred from each use site; the three values
    // and their sites are console-attested):
    const f32 KF_COOLDOWN_TIME    = 0.5f;   // kfCooldownTime  (Construct's cooldown seed)
    const f32 KF_MAX_JUMP_TIME    = 2.0f;   // kfMaxJumpTime   (the valid body releases past it)
    const f32 KF_MIN_TIME_IN_AIR  = 0.1f;   // kfMinTimeInAir  (the Jump_Effect start-hook window)

    // The grounded epsilon the searching state's absolute-value compare reads
    // (FLT_EPSILON; not one of the declared constants).
    const f32 KF_GROUNDED_EPSILON = 1.1920929e-7f;

    // The border post-FX amount the valid body stamps on every produced camera
    // (a data-segment global; initial value read from the console image. FLAG: its
    // owning static's home is not recovered -- mirrored as a local constant).
    const f32 KF_JUMP_BORDER_POSTFX_AMOUNT = 0.15f;

    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // (also this moment's preparing hold)
    const u32 KU_HEAD_FLAG_INHIBITED = 23;

    // Camera-state CURRENT flag bits the valid body raises on the produced
    // camera (the u64 at camera +0x140; the console ORs in 0x80 and 2). FLAG: the two bits' roles
    // are not recovered; indices are console-attested.
    const u32 KU_CAMERA_FLAG_1 = 1;
    const u32 KU_CAMERA_FLAG_7 = 7;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; console shared-info offsets in comments). ----
    u32  MomentSharedInfo_GetStuntFlags(const void* lpSharedInfo);          // +1284 word +228 (bit3 = jumping; bit0 active, bit1 first-frame, bit5 chained)
    bool MomentSharedInfo_IsCrashCameraActive(const void* lpSharedInfo);    // +1284 byte 256
    f32  MomentSharedInfo_GetAirborneHeight(const void* lpSharedInfo);      // +1292 f32 +1028 (0 == grounded)
    f32  MomentSharedInfo_GetTimestep(const void* lpSharedInfo);            // +1312 f32 (the sibling timestep)
    bool MomentSharedInfo_GetJumpEnableFlag1316(const void* lpSharedInfo);  // +1316 byte (the stunt-enable +1317's sibling)
}
using namespace detail;

// The inlined base Moment::Construct, the four
// collection Constructs (no arbitrator-state owner, this moment as owner), the
// interpolate handle clear, the interpolate parameter defaults, and the latch
// seeds. The console stores the mapping override LAST (after every other member) --
// order kept.
void MomentPlayerJumping::Construct()
{
    Moment::Construct();   // inlined in the console build (state/type/inhibit/camera)
    mRigCollection.Construct(0, this);
    mBystanderCollection.Construct(0, this);
    mDroppedRigCollection.Construct(0, this);
    mIceCollection.Construct(0, this);
    mInterpolater.Clear();                 // the console build zeroes the five handle fields inline
    mInterpolaterParams.Construct();       // {type 8, no name, slerp, sinusoidal} (inlined)
    mInterpolaterParams.meInterpolationMethod =
        Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR;   // 1 -> +0x374
    mfCooldownTimer = KF_COOLDOWN_TIME;
    meType          = E_INVALID_TYPE;
    mbPrepared      = false;
    mpParameters    = 0;
    mInterpolaterParams.meInterpolationMapping =
        Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;  // 3 -> +0x378 (stored LAST)
}

// Enter SEARCHING (before the once-only guard), then on
// the first call load the authored shots: six attached-rig + three dropped-rig +
// two bystander blocks off the parameter bank, and up to five ice ShotList refs
// off the resource manager's player-jumping shot group (null blocks fall back to
// the 24-byte default data area). Reports the prepared latch.
bool MomentPlayerJumping::Prepare(void* lrBehaviourController)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    SetState(E_STATE_INVALID_SEARCHING);
    if (!mbPrepared)
    {
        const Camera::BehaviourParameterBank& lrBank =
            lpBehaviourManager->GetBehaviourParameterBank();

        // The attached-rig shots, in the console build's call order. The arguments are
        // RECORD SLOT numbers in the bank's 14-block rig run (stride 288 from record
        // +4432); they were one short at every slot until 2026-09-12.
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(1));    // mRigRearQFwd
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(2));    // mRigFrontQCuFwd
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(7));    // mRigRoofFwd
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(10));   // mRigUnderbelly
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(9));    // mRigFrontQCuFwd2
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(5));    // mRigBootViewFwd

        // The bystander shots: slots 0 and 2 of the 7-block bystander run (stride 156 from
        // record +3336) -- the two blocks whose own names begin "Jump". Slot 1 is NOT one of
        // them; the second argument used to read 1.
        mBystanderCollection.AddShot(&lrBank.GetPlayerJumpingBystanderShotParams(0));   // mBystanderJumpLeftParameters
        mBystanderCollection.AddShot(&lrBank.GetPlayerJumpingBystanderShotParams(2));   // mBystanderJumpFromBehindParameters

        // The dropped-rig shots: the rig run's three "Drop" blocks, slots 11..13.
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(11));   // mRigDropUnderbelly
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(12));   // mRigDropFrontQCuFwd
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(13));   // mRigDropBootViewFwd

        // The ice shots: the shot group's ShotList array (key 0x7533C0E2_15246B49,
        // resolved through the generated Num/Get pair), clamped to the collection's
        // five slots. (AddShot's own "can't add shots when the behaviours have been
        // allocated (although this could be changed)" assert --  -- lands
        // with the collection bodies.)
        const Attrib::Gen::shotgroup& lrJumpShots =
            lpBehaviourManager->GetDirectorResourceManager()->GetJumpRig();   // rm +1320
        u32 luNumShots = lrJumpShots.Num_ShotList();
        if (luNumShots > 5u)
            luNumShots = 5u;
        for (u32 luShot = 0; luShot < luNumShots; ++luShot)
        {
            const void* lpShotData = lrJumpShots.GetShotListData(static_cast<s32>(luShot));
            if (lpShotData == 0)
                lpShotData = Attrib::DefaultDataArea(0x18u);
            mIceCollection.AddShot(
                reinterpret_cast<Camera::Camera::ShotReference*>(
                    const_cast<void*>(lpShotData)));
        }

        mbPrepared = true;
    }
    return mbPrepared;   // the console build re-reads the latch for the return
}

// Zero the cooldown, release every collection + the
// interpolate handle (the inlined guarded UnSet + clear), drop the gates, raise
// the searching head bit, park at SEARCHING (not INACTIVE).
bool MomentPlayerJumping::Release()
{
    mfCooldownTimer = 0.0f;
    mRigCollection.Release();
    mBystanderCollection.Release();
    mDroppedRigCollection.Release();
    mIceCollection.Release();
    mInterpolater.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

const char* MomentPlayerJumping::GetName() const
{
    return "MomentPlayerJumping";
}

Moment::EType MomentPlayerJumping::GetInstanceType()
{
    return E_MOMENT_PLAYER_JUMPING;
}

// Prepare the active sequence's collection(s); every
// Prepare runs unconditionally with its own (non-gating) assert, and the
// function always reports true.
bool MomentPlayerJumping::PrepareCameras(Camera::BehaviourManager& lrBehaviourManager)
{
    switch (meType)
    {
    case E_TYPE_ICE_RIG:
    {
        const bool lbIce = mIceCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbIce, "mIceCollection.Prepare(lBehaviourManager)");             //  (non-gating)
        break;
    }
    case E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE:
    {
        const bool lbDropped = mDroppedRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbDropped, "mDroppedRigCollection.Prepare(lBehaviourManager)");  //  (non-gating)
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");             //  (non-gating)
        break;
    }
    case E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE:
    {
        const bool lbBystander = mBystanderCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbBystander, "mBystanderCollection.Prepare(lBehaviourManager)"); //  (non-gating)
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");             //  (non-gating)
        break;
    }
    case E_TYPE_BYSTANDER_SHOT:
    {
        const bool lbBystander = mBystanderCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbBystander, "mBystanderCollection.Prepare(lBehaviourManager)"); //  (non-gating)
        break;
    }
    case E_TYPE_DROPPED_RIG_SHOT:
    {
        const bool lbDropped = mDroppedRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbDropped, "mDroppedRigCollection.Prepare(lBehaviourManager)");  //  (non-gating)
        break;
    }
    case E_TYPE_ATTACHED_RIG_SHOT:
    {
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");             //  (non-gating)
        break;
    }
    default:
        CGS_ASSERT(false, "invalid type");                                          //  (non-gating)
        break;
    }
    return true;
}

// The active sequence's leading collection: FAILED when
// it failed, else READY exactly when it can be switched to. Sequences check only
// their FIRST collection here.
MomentPlayerJumping::EStatus MomentPlayerJumping::GetCameraStatus()
{
    EStatus leStatus = E_STATUS_WAITING;
    switch (meType)
    {
    case E_TYPE_ICE_RIG:
        if (mIceCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mIceCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    case E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE:
    case E_TYPE_DROPPED_RIG_SHOT:
        if (mDroppedRigCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mDroppedRigCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    case E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE:
    case E_TYPE_BYSTANDER_SHOT:
        if (mBystanderCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mBystanderCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    case E_TYPE_ATTACHED_RIG_SHOT:
        if (mRigCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mRigCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    default:
        CGS_ASSERT(false, "invalid type");   //  (non-gating)
        break;
    }
    return leStatus;
}

// The per-frame state machine:
//   SEARCHING        run the landing cooldown while grounded (the vector
//                    absolute-value compare of the airborne height
//                    against FLT_EPSILON, scalar here); when the player-jumping
//                    gate holds (stunt-flags bit 3 + the crash camera active +
//                    the +1316 enable) and the moment is not inhibited, pick the
//                    ATTACHED_RIG_SHOT sequence, PrepareCameras (asserted), zero
//                    the jump timer and enter FOUND_PREPARING (bit 19).
//   FOUND_PREPARING  WAITING -> the bit-19 hold; READY -> VALID (and run its
//                    body now); FAILED -> the virtual Release + SEARCHING.
//   VALID            integrate the jump timer and pull the frame's camera from
//                    the active collection; register the "Jump_Effect" start
//                    hook while the jump is under 0.1s; release (back to
//                    SEARCHING) past 2.0s / on a camera failure / once landed /
//                    when the crash camera drops -- then stamp the border
//                    post-FX + the two camera-state flags and adopt the camera
//                    EVEN on the release frame (the console build falls through).
void MomentPlayerJumping::Update(f32 lfTimeStep, void* lrBehaviourController,
                                 const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    CGS_ASSERT(mbPrepared, "mbPrepared");   //  (non-gating)

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
        // The landing cooldown integrates only while grounded.
        if (!(fabsf(MomentSharedInfo_GetAirborneHeight(lSharedInfo)) > KF_GROUNDED_EPSILON))
            mfCooldownTimer += MomentSharedInfo_GetTimestep(lSharedInfo);

        if ((MomentSharedInfo_GetStuntFlags(lSharedInfo) & 0x8) != 0
            && MomentSharedInfo_IsCrashCameraActive(lSharedInfo)
            && MomentSharedInfo_GetJumpEnableFlag1316(lSharedInfo))
        {
            if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
                return;
            }
            meType = E_TYPE_ATTACHED_RIG_SHOT;
            const bool lbPrepared = PrepareCameras(*lpBehaviourManager);
            CGS_ASSERT(lbPrepared, "PrepareCameras(lBehaviourManager)");   //  (non-gating)
            mfJumpTimer = 0.0f;
            SetCanSwitchToMeNow(false);
            SetState(E_STATE_INVALID_FOUND_PREPARING);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
            return;
        }
        SetConditionsNotMet();
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        return;

    case E_STATE_INVALID_FOUND_PREPARING:
    {
        const EStatus leStatus = GetCameraStatus();
        if (leStatus == E_STATUS_WAITING)
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
            return;
        }
        if (leStatus == E_STATUS_READY)
        {
            SetState(E_STATE_VALID);
            break;   // fall into the valid body this frame
        }
        if (leStatus < 3)   // == E_STATUS_FAILED
        {
            Release();   // the live-vtable call (slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
            return;
        }
        CGS_ASSERT(false, "Unhandled status");   //  (non-gating)
        // The console's post-assert re-check: FAILED/WAITING exit, anything else
        // falls into the valid body (unreachable for the 3-value status; kept).
        if (leStatus == E_STATUS_FAILED || leStatus == E_STATUS_WAITING)
            return;
        break;
    }

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   //  (non-gating)
        return;
    }

    // ---- the VALID body ----
    mfJumpTimer += lfTimeStep;
    Camera::Camera lCamera;
    const bool lbCameraFailed = !UpdateCamera(lCamera);

    if (mfJumpTimer < KF_MIN_TIME_IN_AIR)
        lCamera.GetEffects().SetStartHookName("Jump_Effect", 1.0f);

    if (mfJumpTimer > KF_MAX_JUMP_TIME
        || lbCameraFailed
        || MomentSharedInfo_GetAirborneHeight(lSharedInfo) == 0.0f
        || !MomentSharedInfo_IsCrashCameraActive(lSharedInfo))
    {
        Release();   // the live-vtable call (slot 4)
        SetState(E_STATE_INVALID_SEARCHING);
    }

    // The camera tail runs even on the release frame (the console build falls through):
    // the border post-FX request, the two camera-state flags, then adopt.
    lCamera.SetRequestedBorderPostFX(KF_JUMP_BORDER_POSTFX_AMOUNT);   // (the Camera-level request wrapper; mEffects +0xA8)
    lCamera.mState.SetFlag(KU_CAMERA_FLAG_7, true);
    lCamera.mState.SetFlag(KU_CAMERA_FLAG_1, true);
    SetCamera(lCamera);
}

// BehaviourCollection<TBehaviour,TParameters,N>::HasActiveCamera
// (declared, NON-const). The console asserts carry this .cpp's source
// path, so the body is defined here (distinct from
// GetCamera, whose asserts carry the .h). One instantiation forced below (the N=4 dropped-rig
// collection).
//
// Console body: read mbAllocatedShots at +0x71 (assert), read mbHasCurrentCamera at +0x70
// (assert), then return the literal true (the guarded reads prove both latches).
// Bools at +0x70/+0x71 => maShots size 0x64 => N*0x18+4=0x64 => N=4.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::HasActiveCamera()
{
    CGS_ASSERT(mbAllocatedShots, "mbAllocatedShots");
    CGS_ASSERT(mbHasCurrentCamera, "mbHasCurrentCamera");
    return true;
}

// Explicit instantiation forcing the console build's out-of-line copy (dropped-rig collection, N=4).
template bool BehaviourCollection<
    Camera::BehaviourRig,
    Camera::BehaviourRig::Parameters, 4>::HasActiveCamera();

}
