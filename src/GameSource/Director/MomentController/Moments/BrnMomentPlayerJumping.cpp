#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"

#include <math.h>                                                    // fabsf (the grounded check)
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"    // the jump rig/bystander blocks
#include "GameSource/Director/BrnDirectorICEWrapper.h"               // ICEWrapper (the resource-manager header's inlines need it complete first)
#include "GameSource/Director/BrnDirectorResourceManager.h"          // the player-jumping shot group
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"        // Attrib::Gen::shotgroup + Attrib::DefaultDataArea

// BrnDirector::MomentPlayerJumping -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF primary file BrnMomentPlayerJumping.cpp;
// member/constant names verbatim from the DecFIGS DWARF).
//
// Bodied here (8 ledger functions):
//   Construct @0x8225F1F0    Prepare @0x82251048        Update @0x82275988
//   Release   @0x8223AED0    PrepareCameras @0x822755A0
//   GetCameraStatus @0x8220A358   GetName @0x821F7630   GetInstanceType @0x821F7628

namespace BrnDirector
{

namespace
{
    // The DWARF cpp:21-23 file constants, values read from the X360 rodata.
    // FLAG (name->value mapping inferred from each use site; the three values
    // and their sites are asm-attested):
    const f32 KF_COOLDOWN_TIME    = 0.5f;   // kfCooldownTime  (flt_82001DA0; Construct's cooldown seed)
    const f32 KF_MAX_JUMP_TIME    = 2.0f;   // kfMaxJumpTime   (flt_82001D9C; the valid body releases past it)
    const f32 KF_MIN_TIME_IN_AIR  = 0.1f;   // kfMinTimeInAir  (flt_82004014; the Jump_Effect start-hook window)

    // The grounded epsilon the searching state's VMX fabs-compare reads
    // (flt_82001770 = FLT_EPSILON; not one of the DWARF-named constants).
    const f32 KF_GROUNDED_EPSILON = 1.1920929e-7f;

    // The border post-FX amount the valid body stamps on every produced camera
    // (flt_82CDA5E0, a .data global; initial value read from the XEX. FLAG: its
    // owning static's home is not recovered -- mirrored as a local constant).
    const f32 KF_JUMP_BORDER_POSTFX_AMOUNT = 0.15f;

    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // oris 4
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // oris 8 (also this moment's preparing hold)
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // oris 0x80

    // Camera-state CURRENT flag bits the valid body raises on the produced
    // camera (the u64 @camera+0x140, `ori 0x80 | 2`). FLAG: the two bits' roles
    // are not recovered; indices are asm-attested.
    const u32 KU_CAMERA_FLAG_1 = 1;
    const u32 KU_CAMERA_FLAG_7 = 7;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; X360 shared-info offsets in comments). ----
    u32  MomentSharedInfo_GetStuntFlags(const void* lpSharedInfo);          // +1284 word +228 (bit3 = jumping; bit0 active, bit1 first-frame, bit5 chained)
    bool MomentSharedInfo_IsCrashCameraActive(const void* lpSharedInfo);    // +1284 byte 256
    f32  MomentSharedInfo_GetAirborneHeight(const void* lpSharedInfo);      // +1292 f32 +1028 (0 == grounded)
    f32  MomentSharedInfo_GetTimestep(const void* lpSharedInfo);            // +1312 f32 (the sibling timestep)
    bool MomentSharedInfo_GetJumpEnableFlag1316(const void* lpSharedInfo);  // +1316 byte (the stunt-enable +1317's sibling)
}
using namespace detail;

// @ 0x8225F1F0 -- cpp:177. The inlined base Moment::Construct, the four
// collection Constructs (no arbitrator-state owner, this moment as owner), the
// interpolate handle clear, the interpolate parameter defaults, and the latch
// seeds. The X360 stores the mapping override LAST (after every other member) --
// order kept.
void MomentPlayerJumping::Construct()
{
    Moment::Construct();   // inlined on the X360 (state/type/inhibit/camera)
    mRigCollection.Construct(0, this);
    mBystanderCollection.Construct(0, this);
    mDroppedRigCollection.Construct(0, this);
    mIceCollection.Construct(0, this);
    mInterpolater.Clear();                 // the X360 zeroes the five handle fields inline
    mInterpolaterParams.Construct();       // {type 8, no name, slerp, sinusoidal} (inlined)
    mInterpolaterParams.meInterpolationMethod = 1;   // E_METHOD_ROTATE_ABOUT_PLAYER_CAR
    mfCooldownTimer = KF_COOLDOWN_TIME;
    meType          = E_INVALID_TYPE;
    mbPrepared      = false;
    mpParameters    = 0;
    mInterpolaterParams.meInterpolationMapping = 3;  // E_MAPPING_EXPONENTIAL_OUT_X_CUBED
}

// @ 0x82251048 -- cpp:209. Enter SEARCHING (before the once-only guard), then on
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

        // The attached-rig shots (bank +0x1280 grid, the X360's call order).
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(0));    // bank+0x1280
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(1));    // bank+0x13A0
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(6));    // bank+0x1940
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(9));    // bank+0x1CA0
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(8));    // bank+0x1B80
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(4));    // bank+0x1700

        // The bystander shots (bank +0xD18 grid).
        mBystanderCollection.AddShot(&lrBank.GetPlayerJumpingBystanderShotParams(0));   // bank+0xD18
        mBystanderCollection.AddShot(&lrBank.GetPlayerJumpingBystanderShotParams(1));   // bank+0xE50

        // The dropped-rig shots (the same rig grid's tail).
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(10));   // bank+0x1DC0
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(11));   // bank+0x1EE0
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(12));   // bank+0x2000

        // The ice shots: the shot group's ShotList array (key 0x7533C0E2_15246B49,
        // resolved through the generated Num/Get pair), clamped to the collection's
        // five slots. (AddShot's own "can't add shots when the behaviours have been
        // allocated (although this could be changed)" assert -- cpp:159 -- lands
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
    return mbPrepared;   // the X360 re-reads the latch for the return
}

// @ 0x8223AED0 -- cpp:680. Zero the cooldown, release every collection + the
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

// @ 0x821F7630 -- cpp:745.
const char* MomentPlayerJumping::GetName() const
{
    return "MomentPlayerJumping";
}

// @ 0x821F7628 -- cpp:731.
Moment::EType MomentPlayerJumping::GetInstanceType()
{
    return E_MOMENT_PLAYER_JUMPING;
}

// @ 0x822755A0 -- cpp:410. Prepare the active sequence's collection(s); every
// Prepare runs unconditionally with its own (non-gating) assert, and the
// function always reports true.
bool MomentPlayerJumping::PrepareCameras(Camera::BehaviourManager& lrBehaviourManager)
{
    switch (meType)
    {
    case E_TYPE_ICE_RIG:
    {
        const bool lbIce = mIceCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbIce, "mIceCollection.Prepare(lBehaviourManager)");             // :416 (non-gating)
        break;
    }
    case E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE:
    {
        const bool lbDropped = mDroppedRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbDropped, "mDroppedRigCollection.Prepare(lBehaviourManager)");  // :422 (non-gating)
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");             // :423 (non-gating)
        break;
    }
    case E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE:
    {
        const bool lbBystander = mBystanderCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbBystander, "mBystanderCollection.Prepare(lBehaviourManager)"); // :429 (non-gating)
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");             // :430 (non-gating)
        break;
    }
    case E_TYPE_BYSTANDER_SHOT:
    {
        const bool lbBystander = mBystanderCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbBystander, "mBystanderCollection.Prepare(lBehaviourManager)"); // :436 (non-gating)
        break;
    }
    case E_TYPE_DROPPED_RIG_SHOT:
    {
        const bool lbDropped = mDroppedRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbDropped, "mDroppedRigCollection.Prepare(lBehaviourManager)");  // :442 (non-gating)
        break;
    }
    case E_TYPE_ATTACHED_RIG_SHOT:
    {
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");             // :448 (non-gating)
        break;
    }
    default:
        CGS_ASSERT(false, "invalid type");                                          // :454 (non-gating)
        break;
    }
    return true;
}

// @ 0x8220A358 -- cpp:465. The active sequence's leading collection: FAILED when
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
        CGS_ASSERT(false, "invalid type");   // :543 (non-gating)
        break;
    }
    return leStatus;
}

// @ 0x82275988 -- cpp:261. The per-frame state machine:
//   SEARCHING        run the landing cooldown while grounded (the VMX
//                    splat/vandc/vcmpgtfp fabs-compare of the airborne height
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
//                    EVEN on the release frame (the X360 falls through).
void MomentPlayerJumping::Update(f32 lfTimeStep, void* lrBehaviourController,
                                 const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    CGS_ASSERT(mbPrepared, "mbPrepared");   // :263 (non-gating)

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
            CGS_ASSERT(lbPrepared, "PrepareCameras(lBehaviourManager)");   // :285 (non-gating)
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
        CGS_ASSERT(false, "Unhandled status");   // :332 (non-gating)
        // The X360's post-assert re-check: FAILED/WAITING exit, anything else
        // falls into the valid body (unreachable for the 3-value status; kept).
        if (leStatus == E_STATUS_FAILED || leStatus == E_STATUS_WAITING)
            return;
        break;
    }

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :400 (non-gating)
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

    // The camera tail runs even on the release frame (the X360 falls through):
    // the border post-FX request, the two camera-state flags, then adopt.
    lCamera.SetRequestedBorderPostFX(KF_JUMP_BORDER_POSTFX_AMOUNT);   // (the Camera-level request wrapper; mEffects +0xA8)
    lCamera.mState.SetFlag(KU_CAMERA_FLAG_7, true);
    lCamera.mState.SetFlag(KU_CAMERA_FLAG_1, true);
    SetCamera(lCamera);
}

// BehaviourCollection<TBehaviour,TParameters,N>::HasActiveCamera() @ X360 0x821FD1A8
// (declared BrnMomentPlayerJumping.h:63, NON-const). The X360 asserts carry the .cpp source
// path (BrnMomentPlayerJumping.cpp:123/124), so the body is defined here (distinct from
// GetCamera, whose asserts carry the .h). One instantiation forced below (the N=4 dropped-rig
// collection).
//
// asm 0x821FD1A8: lbz mbAllocatedShots@+0x71 (assert cpp:123), lbz mbHasCurrentCamera@+0x70
// (assert cpp:124), then li r3,1 -> return true (the guarded reads prove both latches, so the
// X360 returns the literal). Bools @+0x70/+0x71 => maShots size 0x64 => N*0x18+4=0x64 => N=4.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::HasActiveCamera()
{
    CGS_ASSERT(mbAllocatedShots, "mbAllocatedShots");       // BrnMomentPlayerJumping.cpp:123
    CGS_ASSERT(mbHasCurrentCamera, "mbHasCurrentCamera");   // BrnMomentPlayerJumping.cpp:124
    return true;
}

// Explicit instantiation forcing the X360 0x821FD1A8 out-of-line copy (dropped-rig collection, N=4).
template bool BehaviourCollection<
    Camera::BehaviourRig,
    Camera::BehaviourRig::Parameters, 4>::HasActiveCamera();

}
