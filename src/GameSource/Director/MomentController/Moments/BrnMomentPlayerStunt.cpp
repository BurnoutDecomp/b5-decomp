#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.h"
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                       // CgsCore::SPrintf
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"             // CgsResource::ID::HashString
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"   // Attrib::StringToKey
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"               // Camera::EnsureEffectIsPlaying / StopCurrentEffect

// BrnDirector::MomentPlayerStunt -- reconstructed from the console executable
// (home file BrnMomentPlayerStunt.cpp; member names verbatim from
// the declarations).
//
// Bodied here (6 ledger functions):
//   Construct   Update   Release
//   SetParameters   GetName   GetInstanceType

namespace BrnDirector
{

namespace
{
    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING      = 18;
    const u32 KU_HEAD_FLAG_PREPARING      = 20;
    const u32 KU_HEAD_FLAG_INHIBITED      = 23;
    const u32 KU_HEAD_FLAG_KEEP_RUNNING   = 29;   // (holding the released gate while the stunt runs)

    // Camera-state CURRENT flags the valid body publishes (roles not recovered):

    // The requested-post-FX id the 2dFlash mirror compares against (the console build
    // literal 575791 == 0x8C92F; FLAG: the id table's home is not recovered).
    const u32 KU_POSTFX_2DFLASH_ID = 575791u;

    // The camera race-end-effect amount forced every valid frame (read-only data)
    // and the fresh-stunt / landed timing windows.
    const f32 KF_STUNT_RACE_END_EFFECT = 0.15f;
    const f32 KF_FRESH_STUNT_WINDOW    = 0.1f;   // Jump_Effect only in the first 0.1s
    const f32 KF_LANDED_RELEASE_TIME   = 0.5f;   // grounded this long -> release
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; console shared-info offsets in comments). ----
    // Renamed from the offset-shaped GetStuntAbort493 now that it has a body: it is the
    // DirectorProfileData showtime-intro flag ArbStateRoaming already gates two of its own arms
    // on, so all three consumers now read it under one name. Aborting is what it MEANS here, not
    // what it is.

    // Vehicle-dynamics block (+1292) fields:


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
// two "lpIceTake != NULL" assert lines ( /).
static void MomentPlayerStunt_StageTakeReference(const MomentSharedInfo& lrSharedInfo,
                                                 Attrib::RefSpec& lrShotRef,
                                                 s32 liTakeAssertLine)
{
    // The staged take number = the 64-bit take field's LOW word (+1 with the
    //  wrap tripwire the console build inlines -- an unsigned
    // != 0 check, firing only at -1 -- then -1 back for the name).
    const s32 liTakeNumber =
        static_cast<s32>(lrSharedInfo.mpGameState->miActionRequestedCamera);
    CGS_ASSERT(static_cast<u32>(liTakeNumber + 1) != 0, "luTakeIndex > 0");   //  (non-gating)

    char lacTakeName[64];
    CgsCore::SPrintf(lacTakeName, 64, "World_Signature_%i", liTakeNumber);
    // The id is the name's CRC32 ZERO-extended to 64 bits: HashString @0x828D84A8 returns through
    // `nor r11, r9, r9 ; clrldi r3, r11, 0x20` (0x828D854C), and GetKeyAnim takes that register as
    // the id (0x82272B04 `mr r4, r3`).
    // ⭐ 2026-09-24 (FX-DIRECTOR): this used to widen the s32 return straight to u64, which
    // SIGN-extends every name whose CRC has its top bit set -- "World_Signature_305" is
    // 0x9CA2368A -- so the dictionary lookup missed, the "lpIceTake != NULL" assert fired and
    // the guid read below faulted at address 8 (scratch/bugtest/runs/fxdirector_moment_tick/
    // 20260924_180957, the first live run with the moment tick on).
    CgsResource::ID lTakeId;
    lTakeId.SetHash(static_cast<u64>(static_cast<u32>(
        CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacTakeName)))));
    ICE::ICETakeData* lpIceTake =
        lrSharedInfo.mpDirectorResourceManager->GetKeyAnim(lTakeId);
    // (One assert on the allocation path, one on the take swap -- both non-gating.)
    if (lpIceTake == 0)
    {
        CGS_FAIL("lpIceTake != NULL");
        (void)liTakeAssertLine;
    }

    char lacGuid[8];
    CgsCore::SPrintf(lacGuid, 8, "%i", ICETakeData_GetGuid(lpIceTake));

    Attrib::RefSpec lSource(Attrib::Gen::iceanim::ClassKey(),
                            Attrib::StringToKey(lacGuid));
    lrShotRef = lSource;   // the refcounted assign (resolves the collection)
    if (lSource.HasResolvedCollection())
        lSource.Clean();
}

// The inlined base Moment::Construct, the handle clear,
// the parameter reset and the land-time zero.
void MomentPlayerStunt::Construct()
{
    Moment::Construct();   // inlined in the console build (state/type/inhibit/camera)
    mpParameters = 0;
    mIceCam.Construct();       // the console build zeroes the five handle fields inline
    mfLandTime = 0.0f;
}

// The inlined guarded handle Release, the gate clears,
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

// Adopt the tuning record (no type assert in the console build).
void MomentPlayerStunt::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

const char* MomentPlayerStunt::GetName() const
{
    return "MomentPlayerStunt";
}

Moment::EType MomentPlayerStunt::GetInstanceType()
{
    return E_MOMENT_PLAYER_STUNT;
}

// The per-frame stunt state machine:
//   SEARCHING        the abort trio (dynamics +1098 / crash flag 39 / player
//                    493) or a clear stunt-active bit -> not met. Otherwise,
//                    once a take is staged (count > 0, the +1317 enable, the
//                    crash camera active): allocate the ICE cam on the staged
//                    "World_Signature_<index>" take (the stack shot reference),
//                    raise its collision policy, clear its base first-frame
//                    gate, latch the stunt's first-frame bit, and enter
//                    FOUND_PREPARING.
//   FOUND_PREPARING  failed -> Release (the virtual); not yet switchable
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
void MomentPlayerStunt::Update(f32 /*lfTimeStep*/, Camera::BehaviourManager& lrBehaviourController,
                               const MomentSharedInfo& lrSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        &lrBehaviourController;

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        const bool lbAbort =
            lrSharedInfo.mpPlayerCar->mRaceCarState.mbCrashing
            || lrSharedInfo.mpPlayerCrashInfo->mbHitWater
            || lrSharedInfo.mpGameState->mShowTimeInfo.mbInIntro;
        const u32 luStuntFlags   = static_cast<u32>(lrSharedInfo.mpGameState->miThisFramesActionFlags);
        const bool lbFirstFrame  = (luStuntFlags & 2) != 0;

        // Not-met only when aborting or NEITHER stunt bit is up (the console code proceeds
        // to the take gates on bit 0 OR bit 1).
        if (lbAbort || ((luStuntFlags & 1) != 1 && !lbFirstFrame))
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        }
        else if (lrSharedInfo.mpGameState->miActionRequestedCamera > 0
                 && lrSharedInfo.mbAllowStuntMoment
                 && lrSharedInfo.mpGameState->mbCanUseSlomo)
        {
            if (!IsInhibited())
            {
                Attrib::RefSpec lShotRef;
                MomentPlayerStunt_StageTakeReference(lrSharedInfo, lShotRef, 90);

                lpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mIceCam, 0, this, 1);
                mIceCam.Get()->SetParameters(
                    reinterpret_cast<Camera::BehaviourIceAnim::ShotReference*>(&lShotRef));
                mIceCam.Get()->SetUseCollisionPolicy(true);   // +0xE28
                mIceCam.Get()->ClearBaseFirstFrameGate();     // base +0x28 = 0

                mbHasCrashed            = false;
                mbFirstTimeForThisStunt = lbFirstFrame;
                mfTimeInState           = 0.0f;
                mfLandTime              = 0.0f;
                SetState(E_STATE_INVALID_FOUND_PREPARING);

                if (lShotRef.HasResolvedCollection())
                    lShotRef.Clean();
            }
            else
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            }
        }
        return;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mIceCam.Get()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            return;
        }
        if (!mIceCam.Get()->CanSwitchToMeNow())
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
        CGS_FAIL_AT("unhandled case in switch", "..\\..\\..\\GameSource\\Director/MomentController/Moments/BrnMomentPlayerStunt.cpp", 275);   //  (non-gating)
        return;
    }

    // ---- the VALID body ----
    SetCamera(mIceCam.GetCamera());
    // ARTIST @0x82272834: moment+0x120 == camera+0x110 == effects+0xA8.
    GetNonConstCamera().GetEffects().mfBlackBarAmount = KF_STUNT_RACE_END_EFFECT;

    // The stunt current flags: 7/15 by the crash-stunt latch, 19 always, and
    // flag 20 mirroring whether the camera's requested post-FX is the 2dFlash.
    GetNonConstCamera().mState.SetFlag(
        mbIsCrashStunt ? Camera::CameraState::E_FLAG_CRASH_STUNT : Camera::CameraState::E_FLAG_JUMP_CAMERA, true);
    const bool lb2dFlashRequested =
        GetNonConstCamera().GetEffects().muRequestedPostFxId == KU_POSTFX_2DFLASH_ID;
    GetNonConstCamera().mState.SetFlag(Camera::CameraState::E_FLAG_JUMP_PHOTO, lb2dFlashRequested);

    if (!mbFirstTimeForThisStunt && lb2dFlashRequested)
    {
        Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                      *lrSharedInfo.mpEffectInterface,
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
    if (lrSharedInfo.mpPlayerCar->mRaceCarState.mfTimeInAir == 0.0f)
        mfLandTime += lrSharedInfo.mfTimestep;
    else
        mfLandTime = 0.0f;

    // The two-phase release ladder.
    const bool lbReleaseCondition =
        mIceCam.Get()->CanSwitchFromMeNow()
        || mfLandTime > KF_LANDED_RELEASE_TIME
        || !lrSharedInfo.mpGameState->mbCanUseSlomo;
    const bool lbAbort =
        lrSharedInfo.mpPlayerCar->mRaceCarState.mbCrashing
        || lrSharedInfo.mpPlayerCrashInfo->mbHitWater
        || lrSharedInfo.mpGameState->mShowTimeInfo.mbInIntro;
    if (!mbHasCrashed && lbAbort)
        mbHasCrashed = true;

    if (lbReleaseCondition || lbAbort)
    {
        Camera::StopCurrentEffect(GetNonConstCamera(),
                                  *lrSharedInfo.mpEffectInterface);
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
    // staged -- drop the manager hold, stage the new reference (the console build's
    // resolve pins the take collection; the behaviour object itself stays live
    // in its pool slot), and re-raise the collision/first-frame gates.
    const u32 luStuntFlags = static_cast<u32>(lrSharedInfo.mpGameState->miThisFramesActionFlags);
    if ((luStuntFlags & 2) != 0 || (luStuntFlags & 1) == 1 || (luStuntFlags & 0x20) != 0)
    {
        mbFirstTimeForThisStunt = (luStuntFlags & 2) != 0;
        if (lrSharedInfo.mpGameState->miActionRequestedCamera > 0)
        {
            // The console releases the handle FIRST, then re-resolves the (still
            // live) pool slot through the kept allocation key to poke it; the
            // behaviour pointer is captured up front here -- the same object.
            Camera::BehaviourIceAnim* lpBehaviour = mIceCam.Get();
            mIceCam.Release();

            Attrib::RefSpec lShotRef;
            MomentPlayerStunt_StageTakeReference(lrSharedInfo, lShotRef, 248);

            lpBehaviour->SetUseCollisionPolicy(true);   // +0xE28
            lpBehaviour->ClearBaseFirstFrameGate();     // base +0x28 = 0
            mfTimeInState = 0.0f;
            SetState(E_STATE_VALID);

            if (lShotRef.HasResolvedCollection())
                lShotRef.Clean();
        }
    }

    mfTimeInState += lrSharedInfo.mfTimestep;
}

}

// ---- [FX-DIRECTOR 2026-09-24] the vtable one-liners the moment factory needs --------------------
// MomentController::NewMoment's AllocateVoid<MomentPlayerStunt> placement-constructs the moment, which emits its
// vftable, so every slot needs a body. Read off the console vftable off_820074AC (AllocateVoid<MomentPlayerStunt>
// @0x8222EA98 stores it at +0) and the ICF-folded slot bodies it points at:
//   slot 1 Prepare          0x821F7560  meState = E_STATE_INVALID_SEARCHING, return true (the shared
//                                       body the export names MomentBystanderSeesAction::Prepare)
//   slot 5 Destruct         0x8284CB38  `blr` (the one empty body all twelve moments share)
namespace BrnDirector
{
bool MomentPlayerStunt::Prepare(Camera::BehaviourManager& /*lrBehaviourController*/)
{
    SetState(E_STATE_INVALID_SEARCHING);   // li r10, 1 ; stw r10, 0x174(r3)
    return true;                           // li r3, 1
}

void MomentPlayerStunt::Destruct()
{
    // 0x8284CB38 is a lone `blr`: nothing to tear down.
}
}
