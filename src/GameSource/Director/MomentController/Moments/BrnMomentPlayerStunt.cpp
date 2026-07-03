#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                       // CgsCore::SPrintf
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"             // CgsResource::ID::HashString
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"   // Attrib::StringToKey
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"               // Camera::EnsureEffectIsPlaying / StopCurrentEffect

// BrnDirector::MomentPlayerStunt -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentPlayerStunt.cpp; member names verbatim from the
// DecFIGS DWARF).
//
// Bodied here (6 ledger functions):
//   Construct @0x8225F310   Update @0x82272750   Release @0x8223AF78
//   SetParameters @0x821F76A8   GetName @0x821F7648   GetInstanceType @0x821F7640

namespace BrnDirector
{

namespace
{
    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING      = 18;   // oris 4
    const u32 KU_HEAD_FLAG_PREPARING      = 20;   // oris 0x10
    const u32 KU_HEAD_FLAG_INHIBITED      = 23;   // oris 0x80
    const u32 KU_HEAD_FLAG_KEEP_RUNNING   = 29;   // oris 0x2000 (holding the released gate while the stunt runs)

    // Camera-state CURRENT flags the valid body publishes (roles not recovered):
    const u32 KU_STATE_FLAG_STUNT       = 7;    // ori 0x80 (a plain stunt frame)
    const u32 KU_STATE_FLAG_CRASH_STUNT = 15;   // ori 0x8000 (a crash-stunt frame)
    const u32 KU_STATE_FLAG_2DFLASH     = 20;   // ori/andc 0x100000 (mirrors the 2dFlash request)

    // The requested-post-FX id the 2dFlash mirror compares against (the X360
    // literal 575791 == 0x8C92F; FLAG: the id table's home is not recovered).
    const u32 KU_POSTFX_2DFLASH_ID = 575791u;

    // The camera race-end-effect amount forced every valid frame (XEX rodata
    // @0x82CDA5E0) and the fresh-stunt / landed timing windows.
    const f32 KF_STUNT_RACE_END_EFFECT = 0.15f;
    const f32 KF_FRESH_STUNT_WINDOW    = 0.1f;   // Jump_Effect only in the first 0.1s
    const f32 KF_LANDED_RELEASE_TIME   = 0.5f;   // grounded this long -> release
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; X360 shared-info offsets in comments). ----
    bool MomentSharedInfo_IsCrashCameraActive(const void* lpSharedInfo);       // +1284 byte 256
    bool MomentSharedInfo_GetStuntAbort493(const void* lpSharedInfo);          // +1284 byte 493 (aborts the stunt cam)
    u32  MomentSharedInfo_GetStuntFlags(const void* lpSharedInfo);             // +1284 word +228 (bit0 active, bit1 first-frame, bit5 chained)
    s64  MomentSharedInfo_GetStuntTakeField(const void* lpSharedInfo);        // +1284 s64 +232 (the gate is >0; the LOW word -- BE +236 -- is the staged take number)
    f32  MomentSharedInfo_GetFrameTimestep(const void* lpSharedInfo);          // +1308 f32 (NOTE: distinct from the +1312 timestep its siblings read)
    bool MomentSharedInfo_GetStuntEnableFlag1317(const void* lpSharedInfo);    // +1317 byte
    const DirectorResourceManager*
         MomentSharedInfo_GetDirectorResourceManager(const void* lpSharedInfo); // +1340
    const EffectInterface*
         MomentSharedInfo_GetEffectInterface(const void* lpSharedInfo);        // +1352

    // Vehicle-dynamics block (+1292) fields:
    bool MomentSharedInfo_GetDynamicsAbort1098(const void* lpSharedInfo);      // +1292 byte +1098 (aborts the stunt cam)
    f32  MomentSharedInfo_GetAirborneHeight(const void* lpSharedInfo);         // +1292 f32 +1028 (0 == grounded; gates the land timer)

    bool MomentSharedInfo_GetCrashFlag39(const void* lpSharedInfo);            // +1356 byte +39 (aborts the stunt cam)

    // The staged take's guid (take data +0x8; the "%i" the reference is keyed
    // by). DECLARATION-ONLY (the ICE data TU owns the field surface).
    s32 ICETakeData_GetGuid(const ICE::ICETakeData* lpTakeData);
}
using namespace detail;

// The stack shot reference the stunt moment stages for its ICE takes: resolve
// the staged take ("World_Signature_<index>"), format its guid (take data +8)
// and compose the iceanim reference {ClassKey, StringToKey(guid)}, assigning it
// with the refcounted RefSpec operator= (the source is Clean()ed when the
// assign left it holding a resolved collection). Shared by the state-1
// allocation and the mid-flight take swap. lpTakeAssertLine distinguishes the
// two "lpIceTake != NULL" assert lines (:90 / :248).
static void MomentPlayerStunt_StageTakeReference(const void* lSharedInfo,
                                                 Attrib::RefSpec& lrShotRef,
                                                 s32 liTakeAssertLine)
{
    // The staged take number = the 64-bit take field's LOW word (+1 with the
    // BrnICEMoviePlayer.cpp:65 wrap tripwire the X360 inlines -- an unsigned
    // != 0 check, firing only at -1 -- then -1 back for the name).
    const s32 liTakeNumber =
        static_cast<s32>(MomentSharedInfo_GetStuntTakeField(lSharedInfo));
    CGS_ASSERT(static_cast<u32>(liTakeNumber + 1) != 0, "luTakeIndex > 0");   // BrnICEMoviePlayer.cpp:65 (non-gating)

    char lacTakeName[64];
    CgsCore::SPrintf(lacTakeName, 64, "World_Signature_%i", liTakeNumber);
    ICE::ICETakeData* lpIceTake =
        MomentSharedInfo_GetDirectorResourceManager(lSharedInfo)->GetKeyAnim(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacTakeName)));
    // (:90 on the allocation path, :248 on the take swap -- both non-gating.)
    if (lpIceTake == 0)
    {
        CGS_ASSERT(false, "lpIceTake != NULL");
        (void)liTakeAssertLine;
    }

    char lacGuid[8];
    CgsCore::SPrintf(lacGuid, 8, "%i", ICETakeData_GetGuid(lpIceTake));

    Attrib::RefSpec lSource(Attrib::Gen::iceanim::ClassKey(),
                            Attrib::StringToKey(lacGuid));
    lrShotRef = lSource;   // the refcounted assign @0x8280DFB0 (resolves the collection)
    if (lSource.HasResolvedCollection())
        lSource.Clean();   // @0x8280DB60
}

// @ 0x8225F310 -- cpp:35. The inlined base Moment::Construct, the handle clear,
// the parameter reset and the land-time zero.
void MomentPlayerStunt::Construct()
{
    Moment::Construct();   // inlined on the X360 (state/type/inhibit/camera)
    mpParameters = 0;
    mIceCam.Clear();       // the X360 zeroes the five handle fields inline
    mfLandTime = 0.0f;
}

// @ 0x8223AF78 -- cpp:288. The inlined guarded handle Release, the gate clears,
// the searching head bit, back to SEARCHING. Returns true.
bool MomentPlayerStunt::Release()
{
    mIceCam.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// @ 0x821F76A8 -- cpp:320. Adopt the tuning record (no type assert on the X360).
void MomentPlayerStunt::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

// @ 0x821F7648 -- cpp:346.
const char* MomentPlayerStunt::GetName() const
{
    return "MomentPlayerStunt";
}

// @ 0x821F7640 -- cpp:332.
Moment::EType MomentPlayerStunt::GetInstanceType()
{
    return E_MOMENT_PLAYER_STUNT;
}

// @ 0x82272750 -- cpp:70. The per-frame stunt state machine:
//   SEARCHING        the abort trio (dynamics +1098 / crash flag 39 / player
//                    493) or a clear stunt-active bit -> not met. Otherwise,
//                    once a take is staged (count > 0, the +1317 enable, the
//                    crash camera active): allocate the ICE cam on the staged
//                    "World_Signature_<index>" take (the stack shot reference),
//                    raise its collision policy, clear its base first-frame
//                    gate, latch the stunt's first-frame bit, and enter
//                    FOUND_PREPARING.
//   FOUND_PREPARING  failed -> Release (the virtual); not yet switchable ->
//                    hold (bit 20); else VALID (clearing the stopped-effect
//                    latch) and run the valid body this frame.
//   VALID            mirror the produced camera; force the 0.15 race-end
//                    effect; publish the stunt current flags (7/15 by the
//                    crash-stunt latch, 19, and the 2dFlash mirror on flag 20);
//                    play the 2dFlash on non-first frames / raise the
//                    Jump_Effect hook in a fresh stunt's first 0.1s; integrate
//                    the land timer while grounded; run the two-phase release
//                    (stop the effect one frame, release the next) when the rig
//                    releases, the car stays landed past 0.5s, the crash camera
//                    drops, or the abort trio fires (latching mbHasCrashed);
//                    and swap to the newly staged take mid-flight when the
//                    stunt chain continues (bits 1/0/5 of the stunt flags).
void MomentPlayerStunt::Update(f32 /*lfTimeStep*/, void* lrBehaviourController,
                               const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        const bool lbAbort =
            MomentSharedInfo_GetDynamicsAbort1098(lSharedInfo)
            || MomentSharedInfo_GetCrashFlag39(lSharedInfo)
            || MomentSharedInfo_GetStuntAbort493(lSharedInfo);
        const u32 luStuntFlags   = MomentSharedInfo_GetStuntFlags(lSharedInfo);
        const bool lbFirstFrame  = (luStuntFlags & 2) != 0;

        // Not-met only when aborting or NEITHER stunt bit is up (the asm proceeds
        // to the take gates on bit 0 OR bit 1).
        if (lbAbort || ((luStuntFlags & 1) != 1 && !lbFirstFrame))
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        }
        else if (MomentSharedInfo_GetStuntTakeField(lSharedInfo) > 0
                 && MomentSharedInfo_GetStuntEnableFlag1317(lSharedInfo)
                 && MomentSharedInfo_IsCrashCameraActive(lSharedInfo))
        {
            if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            }
            else
            {
                Attrib::RefSpec lShotRef;
                MomentPlayerStunt_StageTakeReference(lSharedInfo, lShotRef, 90);   // cpp:90

                lpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mIceCam, 0, this, 1);
                mIceCam.GetBehaviour()->SetParameters(
                    reinterpret_cast<Camera::BehaviourIceAnim::ShotReference*>(&lShotRef));
                mIceCam.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28
                mIceCam.GetBehaviour()->ClearBaseFirstFrameGate();     // base +0x28 = 0

                mbHasCrashed            = false;
                mbFirstTimeForThisStunt = lbFirstFrame;
                mfTimeInState           = 0.0f;
                mfLandTime              = 0.0f;
                SetState(E_STATE_INVALID_FOUND_PREPARING);

                if (lShotRef.HasResolvedCollection())
                    lShotRef.Clean();
            }
        }
        return;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mIceCam.GetBehaviour()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            return;
        }
        if (!mIceCam.GetBehaviour()->CanSwitchToMeNow())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            return;
        }
        SetState(E_STATE_VALID);
        mbStoppedEffect = false;
        break;   // fall into the valid body this frame

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :275 (non-gating)
        return;
    }

    // ---- the VALID body ----
    SetCamera(mIceCam.GetProducedCamera());
    GetNonConstCamera().GetEffects().mfRaceEndEffectAmount = KF_STUNT_RACE_END_EFFECT;

    // The stunt current flags: 7/15 by the crash-stunt latch, 19 always, and
    // flag 20 mirroring whether the camera's requested post-FX is the 2dFlash.
    GetNonConstCamera().mState.SetFlag(
        mbIsCrashStunt ? KU_STATE_FLAG_CRASH_STUNT : KU_STATE_FLAG_STUNT, true);
    const bool lb2dFlashRequested =
        GetNonConstCamera().GetEffects().muRequestedPostFxId == KU_POSTFX_2DFLASH_ID;
    GetNonConstCamera().mState.SetFlag(KU_STATE_FLAG_2DFLASH, lb2dFlashRequested);

    if (!mbFirstTimeForThisStunt && lb2dFlashRequested)
    {
        Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                      *MomentSharedInfo_GetEffectInterface(lSharedInfo),
                                      "2dFlash", 1.0f);
    }
    if (mfTimeInState < KF_FRESH_STUNT_WINDOW && mbFirstTimeForThisStunt)
    {
        Camera::CameraEffects& lrEffects = GetNonConstCamera().GetEffects();
        lrEffects.mStartHookNameString.Set("Jump_Effect");
        lrEffects.mfStartHookNameBlendAmount = 1.0f;
        lrEffects.mbHasStartHookNameString   = true;
        mbStoppedEffect = false;
    }

    // The land timer: integrate while grounded, reset while airborne.
    if (MomentSharedInfo_GetAirborneHeight(lSharedInfo) == 0.0f)
        mfLandTime += MomentSharedInfo_GetFrameTimestep(lSharedInfo);
    else
        mfLandTime = 0.0f;

    // The two-phase release ladder.
    const bool lbReleaseCondition =
        mIceCam.GetBehaviour()->CanSwitchFromMeNow()
        || mfLandTime > KF_LANDED_RELEASE_TIME
        || !MomentSharedInfo_IsCrashCameraActive(lSharedInfo);
    const bool lbAbort =
        MomentSharedInfo_GetDynamicsAbort1098(lSharedInfo)
        || MomentSharedInfo_GetCrashFlag39(lSharedInfo)
        || MomentSharedInfo_GetStuntAbort493(lSharedInfo);
    if (!mbHasCrashed && lbAbort)
        mbHasCrashed = true;

    if (lbReleaseCondition || lbAbort)
    {
        Camera::StopCurrentEffect(GetNonConstCamera(),
                                  *MomentSharedInfo_GetEffectInterface(lSharedInfo));
        if (mbStoppedEffect)
        {
            mIceCam.Release();
            SetState(E_STATE_INVALID_SEARCHING);
        }
        mbStoppedEffect = true;
    }
    else
    {
        SetCanSwitchFromMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_KEEP_RUNNING);
    }

    // The mid-flight take swap: when the stunt chain continues (first-frame /
    // active / chained bits), re-latch the first-frame bit and -- with a take
    // staged -- drop the manager hold, stage the new reference (the X360's
    // resolve pins the take collection; the behaviour object itself stays live
    // in its pool slot), and re-raise the collision/first-frame gates.
    const u32 luStuntFlags = MomentSharedInfo_GetStuntFlags(lSharedInfo);
    if ((luStuntFlags & 2) != 0 || (luStuntFlags & 1) == 1 || (luStuntFlags & 0x20) != 0)
    {
        mbFirstTimeForThisStunt = (luStuntFlags & 2) != 0;
        if (MomentSharedInfo_GetStuntTakeField(lSharedInfo) > 0)
        {
            // The X360 releases the handle FIRST, then re-resolves the (still
            // live) pool slot through the kept allocation key to poke it; the
            // behaviour pointer is captured up front here -- the same object.
            Camera::BehaviourIceAnim* lpBehaviour = mIceCam.GetBehaviour();
            mIceCam.Release();

            Attrib::RefSpec lShotRef;
            MomentPlayerStunt_StageTakeReference(lSharedInfo, lShotRef, 248);   // cpp:248

            lpBehaviour->SetUseCollisionPolicy(true);   // +0xE28
            lpBehaviour->ClearBaseFirstFrameGate();     // base +0x28 = 0
            mfTimeInState = 0.0f;
            SetState(E_STATE_VALID);

            if (lShotRef.HasResolvedCollection())
                lShotRef.Clean();
        }
    }

    mfTimeInState += MomentSharedInfo_GetFrameTimestep(lSharedInfo);
}

}
