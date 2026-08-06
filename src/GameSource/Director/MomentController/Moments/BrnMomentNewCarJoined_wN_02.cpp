#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (the unhandled-case assert)
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"   // BrnDirector::Timestep::E_WORLD_NO_SLOMO
// NOTE (mutual exclusion, recorded in BrnMomentNewCarJoined.h): do NOT include
// GameSource/Director/MomentController/BrnMomentSubclasses.h here -- it carries a
// layout-stub MomentNewCarJoined that redefines this class.

// BrnDirector::MomentNewCarJoined -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentNewCarJoined.cpp; member names verbatim from the
// DecFIGS DWARF). PARTFILE (wave N, group 02).
//
// Bodied here:
//   Update  @0x82266C30  (DWARF cpp:81)
//   Release @0x8223B078  (DWARF cpp:200 -- its real home; see the LEDGER note on
//                         the Release body below)
// Construct / GetName / GetInstanceType land in this TU's sibling partfile.

namespace BrnDirector
{

namespace
{
    // Camera-state head bits (the moment family's shared vocabulary; the values are
    // the `oris` immediates the X360 ORs into the head doubleword at camera+0x138):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // oris 4
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // oris 8
    const u32 KU_HEAD_FLAG_PREPARING = 20;   // oris 0x10
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // oris 0x80

    // The blend duration both interpolaters are set up with (flt_82001C98 == 1.0f;
    // the same constant the X360 re-uses for the start-hook blend amount below).
    const f32 KF_BLEND_DURATION = 1.0f;

    // The border post-FX amount stamped on the moment's camera every frame it is
    // past PREPARING. FLAG: the console reads the MUTABLE .data global flt_82CDA5E0
    // (initial value 0.15f, read from the XEX); its owning static's home is not
    // recovered, so it is mirrored here as a local constant -- exactly the
    // MomentPlayerJumping::KF_JUMP_BORDER_POSTFX_AMOUNT precedent.
    const f32 KF_BORDER_POSTFX_AMOUNT = 0.15f;

    // The slow-mo the join camera requests (flt_8200177C == 0.2857143f == 2/7 --
    // the same scale ArbStateTakedown writes).
    const f32 KF_JOIN_SLOWMO_TIME_SCALE = 0.2857143f;

    // The window (from entering VALID) during which the "Rival_Join" start hook is
    // (re-)registered every frame (flt_82004014 == 0.1f).
    const f32 KF_START_HOOK_WINDOW = 0.1f;

    // FLAG (role inferred, VALUE is asm): the helper slot the blend runs to/from is
    // the literal 1 the X360 passes in r5 (state 1: FROM) and r6 (the return blend:
    // TO). Read as "the primary gameplay behaviour's helper slot" from the two
    // directions plus the moment's purpose; nothing in the asm names it.
    const s32 KI_GAMEPLAY_HELPER_SLOT = 1;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the Moment base type-erases it
    // to const void*). DECLARATION-ONLY named helpers per the moment-family
    // precedent; X360 shared-info offsets in comments. ----

    // FLAG (role inferred): the +1284 block's byte at +338 (0x152) gates the whole
    // moment -- a rival is streaming in / the join camera should play. The offset and
    // the two read sites are asm.
    bool MomentSharedInfo_IsNewCarJoining(const void* lpSharedInfo);           // +1284 block byte +338

    // FLAG (role inferred): the +1284 block's word at +344 (0x158) is handed to the
    // loose attachment's SetTarget, i.e. the race car the join camera frames.
    s32  MomentSharedInfo_GetNewCarJoinTargetIndex(const void* lpSharedInfo);  // +1284 block word +344

    // +1304 word -- the SAME slot (and therefore the same accessor) the
    // Bystander/PassengerSeesAction moments declare; here it is the car the loose
    // attachment ATTACHES to.
    s32  MomentSharedInfo_GetCrashVehicleIndex(const void* lpSharedInfo);      // +1304 word

    // +1308 f32 -- the SAME slot MomentPlayerStunt declares (distinct from the
    // +1312 timestep its other siblings read).
    f32  MomentSharedInfo_GetFrameTimestep(const void* lpSharedInfo);          // +1308 f32
}
using namespace detail;

// ----------------------------------------------------------------------------
// @0x8223B078 -- cpp:200. Release all three behaviour handles (the X360 inlines
// BehaviourHandle::Release three times: the guarded UnSetBehaviourUsedByHandle +
// the four-field clear), drop both gates, raise the searching head bit and park
// the state machine back at SEARCHING.
//
// LEDGER NOTE (for the conductor): this function's ledger row lives under the
// GameSource/Director/Camera/BrnBehaviourManager.h TU key -- a DWARF-inlining
// misattribution (its body is three inlined BehaviourHandle::Release runs, so the
// line info pinned it to the manager header). Its DWARF home is THIS .cpp
// (cpp:200) and the declaration lives in BrnMomentNewCarJoined.h; the row needs
// reconciling to this TU.
// ----------------------------------------------------------------------------
bool MomentNewCarJoined::Release()
{
    mInterpolaterA.Release();     // guarded UnSet + 4-field clear @0x8223B094..0x8223B0B8
    mInterpolaterB.Release();     //                               @0x8223B0BC..0x8223B0E0
    mLooseAttachment.Release();   //                               @0x8223B0E4..0x8223B108
    SetConditionsNotMet();                                          // stb 0, 0x17A
    SetCanSwitchToMeNow(false);                                     // stb 0, 0x178
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING); // oris 4 on +0x148
    SetState(E_STATE_INVALID_SEARCHING);                            // stw 1, 0x174
    return true;
}

// ----------------------------------------------------------------------------
// @0x82266C30 -- cpp:81. The per-frame new-car-joined state machine:
//   SEARCHING        while a rival is joining and the moment is not inhibited,
//                    allocate the loose attachment (attached to the crash vehicle,
//                    targeted at the joining car) and interpolater A, and start the
//                    1s blend FROM the gameplay helper slot TO the loose attachment.
//   FOUND_PREPARING  A failed -> the virtual Release + SEARCHING; A not yet
//                    switchable -> hold on the bit-20 head flag; otherwise zero the
//                    in-state timer, enter VALID and RUN THE VALID BODY THE SAME
//                    FRAME (the X360 falls through).
//   VALID            once the return blend (B) is switchable, publish ITS camera and
//                    release the moment as soon as it will also switch away; until
//                    then publish A's camera, and the frame the join flag clears,
//                    start the return blend (loose attachment -> gameplay slot).
// The camera tail (border post-FX, the 2/7 slow-mo, the "Rival_Join" start hook and
// the in-state timer) runs even on the release frame -- the X360 falls through.
//
// SIGNATURE (from the asm prologue, NOT the pseudocode): r3 = this,
// f1 = lfTimeStep -- which this body NEVER READS (the float rides an FPR and skips
// its GPR slot, so r5/r6 are the next two args): the moment integrates the shared
// info's own +1308 frame timestep instead. r5 = the BehaviourManager, r6 = the
// MomentSharedInfo (type-erased to const void* by the family Update signature).
// ----------------------------------------------------------------------------
void MomentNewCarJoined::Update(f32 /*lfTimeStep -- dead arg, see the banner*/,
                                void* lrBehaviourController,
                                const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
        if (MomentSharedInfo_IsNewCarJoining(lSharedInfo))
        {
            if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            }
            else
            {
                lpBehaviourManager->NewBehaviour<Camera::BehaviourLooseAttachment>(
                    mLooseAttachment, 0, this, 2);
                mLooseAttachment.GetBehaviour()->SetParameters(&mLooseAttachmentParameters);
                mLooseAttachment.GetBehaviour()->AttachTo(
                    MomentSharedInfo_GetCrashVehicleIndex(lSharedInfo));
                mLooseAttachment.GetBehaviour()->SetTarget(
                    MomentSharedInfo_GetNewCarJoinTargetIndex(lSharedInfo));

                lpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                    mInterpolaterA, 0, this, 1);
                mInterpolaterA.GetBehaviour()->SetParameters(&mInterpolateParams);
                // The moment itself runs at 2/7 slow-mo (see the tail); its blend
                // must not -- the X360 stores the enumerator 1 into Behaviour +0x04.
                mInterpolaterA.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

                // The blend OUT: FROM the gameplay helper slot TO the loose attachment
                // (arg direction asm-pinned at 0x82266F90..0x82266FA0: r5 = 1,
                // r6 = the loose attachment's helper index).
                //
                // ⚠️ SHAPE FALLBACK (behaviour-neutral): the console calls the COMBINED
                // overload Setup(f32, BehaviourHelperIndex, BehaviourHelperIndex,
                // BehaviourManager&) @0x8224EE58, which is NOT declared in the committed
                // BrnBehaviourInterpolate.h. This wave may not edit that header, so the
                // decomposed named-setup shape (the ArbStateOnlineRaceIntro precedent) is
                // used: it reaches an identical end state and fires no assert (mbSetup is
                // still false while the references are written); it only evaluates asserts
                // the console path skips. Swap to the combined call once the overload is
                // declared -- see the shared-header request in the wave spec.
                mInterpolaterA.GetBehaviour()->SetupDuration(KF_BLEND_DURATION);
                mInterpolaterA.GetBehaviour()->SetupCameraAFromHelper(
                    Camera::BehaviourHelperIndex(KI_GAMEPLAY_HELPER_SLOT), *lpBehaviourManager);
                mInterpolaterA.GetBehaviour()->SetupCameraBFromHelper(
                    mLooseAttachment.GetBehaviourHelperIndex(), *lpBehaviourManager);
                mInterpolaterA.GetBehaviour()->Setup();

                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
                SetState(E_STATE_INVALID_FOUND_PREPARING);
            }
        }
        else
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        }
        return;

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mInterpolaterA.GetBehaviour()->HasFailed())
        {
            Release();   // the live-vtable call (lwz vtbl+0x10; bctrl)
            SetState(E_STATE_INVALID_SEARCHING);
            return;
        }
        if (!mInterpolaterA.GetBehaviour()->CanSwitchToMeNow())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            return;
        }
        mfTimeInState = 0.0f;
        SetState(E_STATE_VALID);
        // fall through -- the X360 runs the VALID body the same frame

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :187 (non-gating)
        return;
    }

    // ---- the VALID body ----
    if (mInterpolaterB.IsAllocated()
        && mInterpolaterB.GetBehaviour()->CanSwitchToMeNow())
    {
        // The return blend has taken over: publish its camera, and let go of the
        // moment as soon as it will also switch away.
        SetCamera(mInterpolaterB.GetProducedCamera());
        if (mInterpolaterB.GetBehaviour()->CanSwitchFromMeNow())
        {
            Release();   // the live-vtable call
            // The caller's own state store AFTER Release (redundant -- Release already
            // parks at SEARCHING -- but it is what the console does; kept).
            SetState(E_STATE_INVALID_SEARCHING);
        }
    }
    else
    {
        if (!MomentSharedInfo_IsNewCarJoining(lSharedInfo))
        {
            // FAITHFUL ODDITY -- KEEP: if B is allocated but not yet switchable and
            // the join flag has cleared, the console RE-ALLOCATES B every frame
            // (NewBehaviour on a live handle re-Prepares it, releasing the old hold
            // first). Do not "fix" this into a one-shot.
            lpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                mInterpolaterB, 0, this, 1);

            // @0x82266DB0: `stb r26(=1), 0x32E(looseBehaviour)` -- the console sets the
            // loose attachment's detach-requested byte here so the attachment eases off
            // while the return blend runs. RESTORED in wave O, once the member landed in
            // BrnBehaviourLooseAttachment.h. The store and the offset are asm; the NAME
            // is role-inferred from the sibling tunable Parameters::mfDetachLerpAmount.
            mLooseAttachment.GetBehaviour()->mbDetachRequested = 1;

            mInterpolaterB.GetBehaviour()->SetParameters(&mInterpolateParams);
            mInterpolaterB.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

            // The blend BACK: FROM the loose attachment TO the gameplay helper slot
            // (direction asm-pinned at 0x82266DF8..0x82266E08: r5 = the loose
            // attachment's helper index, r6 = 1). Same combined-overload shape
            // fallback as the outward blend above.
            mInterpolaterB.GetBehaviour()->SetupDuration(KF_BLEND_DURATION);
            mInterpolaterB.GetBehaviour()->SetupCameraAFromHelper(
                mLooseAttachment.GetBehaviourHelperIndex(), *lpBehaviourManager);
            mInterpolaterB.GetBehaviour()->SetupCameraBFromHelper(
                Camera::BehaviourHelperIndex(KI_GAMEPLAY_HELPER_SLOT), *lpBehaviourManager);
            mInterpolaterB.GetBehaviour()->Setup();
        }
        // Hold interpolater A's output until B takes over.
        SetCamera(mInterpolaterA.GetProducedCamera());
    }

    // ---- the common tail (runs on the release frame too -- the X360 falls through) ----
    GetNonConstCamera().SetRequestedBorderPostFX(KF_BORDER_POSTFX_AMOUNT);      // mEffects +0xA8
    GetNonConstCamera().SetRequestedTimeDilation(KF_JOIN_SLOWMO_TIME_SCALE);    // mEffects +0x9C
    // The single float compare in this function: `fcmpu; bge` used to SKIP the hook,
    // i.e. the hook runs on strictly-less-than and is skipped when unordered -- which
    // is exactly C++ `<` (false for NaN). No polarity correction needed.
    if (mfTimeInState < KF_START_HOOK_WINDOW)
    {
        GetNonConstCamera().GetEffects().SetStartHookName("Rival_Join", 1.0f);
    }
    mfTimeInState += MomentSharedInfo_GetFrameTimestep(lSharedInfo);
}

}
