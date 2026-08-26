#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"           // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // MovieClipInstance::GetCurrentFrameOneBased

// BrnGui::BoostMessageSlot -- reconstructed from BURNOUT_X360_ARTIST.XEX (the
// asserts name the original source
// GameSource/Gui/Flow/HUD/Components/BrnBoostMessageSlot.cpp).
//
// Bodied here (9 functions):
//   Construct         @0x82411D30 (cpp:52)      Prepare        @0x82420C88
//   SetMessage        @0x82420D60 (cpp:152)     SetMessageText @0x82411E30 (cpp:177)
//   Refresh           @0x82411EE8               ShuffleUp      @0x82411F40 (cpp:283/284/285)
//   ShuffleDown       @0x824120E8 (cpp:313/314/315)
//   ResetSlotPosition @0x82412290 (cpp:343)     Update         @0x82420E60 (cpp:266)
//
// FLOAT-ARG ABI NOTE: SetMessage's f32 argument rides f1 and BURNS the matching GPR
// slot -- the X360 takes lpcText in r4, liMessageId in r5, lfTimeToLive in f1 (r6
// skipped), leBoostType in r7 and liBoostAmount in r8 (asm 0x82420D74..0x82420D84),
// which is exactly the declared parameter order. Same for Refresh (f1 = the ttl,
// r5 = the bool) and Update (f1 = the time step).

namespace BrnGui
{

namespace
{
    // The slot clip's shuffle-settle frames and the item clip's transition-settle
    // frames. ONE-BASED: the X360 inlines GetCurrentFrameOneBased as the ZERO-based
    // MovieClipInstance +0x28 halfword plus one, then compares these immediates.
    const s32 KAI_SLOT_SETTLE_FRAMES[4] = { 10, 19, 27, 34 };   // cmpwi 0xA/0x13/0x1B/0x22 @0x82420F10..2C
    const s32 KAI_ITEM_SETTLE_FRAMES[3] = { 13, 45, 66 };       // cmpwi 0xD/0x2D/0x42      @0x82420F7C..90

    // The :266 tripwire's ceiling -- a shuffle must have settled by the last settle
    // frame (the assert TEXT bakes the same 34).
    const s32 KI_SLOT_TRANSITION_FRAME_LIMIT = 34;              // cmpwi 0x22 @0x82420F48

    // The shuffle bounds: ShuffleUp asserts below it, ShuffleDown asserts above 0.
    const s32 KI_MAX_SLOT_POSITION = 3;                         // cmpwi 3 @0x82411FDC

    // The composite key buffer the X360 formats the item name into: SPrintf is
    // capped at 128 and the hard NUL lands at index 127 (stb @0x82420D4C).
    const s32 KI_COMPONENT_PATH_LENGTH = 128;

    // The shuffle label buffer ("<from>-<to>"): SPrintf capped at 0x20.
    const s32 KI_SHUFFLE_LABEL_LENGTH = 32;                     // li r4, 0x20 @0x824120A8 / @0x82412250
}

// ===================================================================================
// Construct  @ 0x82411D30 -- cpp:52
//
// The streamed "Invalid state interface" tripwire fires ahead of the base's own
// h:113 one (the X360 emits both inside the single `if (!lpStateInterface)` arm --
// asm 0x82411D4C..0x82411DE0 -- i.e. the local assert, then the inlined base
// Construct). The name/parent arguments are accepted and dropped, exactly as on the
// console (r4 and r6 are never read); the embedded item is constructed with a NULL
// name and NULL parent off the same interface.
// ===================================================================================
void BoostMessageSlot::Construct(const char* /*lpacName*/, CgsGui::StateInterface* lpStateInterface,
                                 const char* /*lpacParentName*/)
{
    CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");   // :52 (streamed; folded, non-gating)

    BrnFlaptComponent::Construct(lpStateInterface);   // inlined: the h:113 tripwire, the iface store, mAptRef.SetInvalid()

    mMessageItem.Construct(0, lpStateInterface, 0);   // @0x82411E00 (r4 = 0, r6 = 0)

    mbInUse        = false;   // +0x34
    mbInTransition = false;   // +0x35
    miSlotPosition = 0;       // +0x3C
    mfTimeToLive   = 0.0f;    // +0x40 (flt_82001CC0 == 0.0f)
    miMessageId    = -1;      // +0x38 (no message)
}

// ===================================================================================
// Prepare  @ 0x82420C88
//
// The base Prepare body is INLINED on the console (the h:133 "lacName != NULL"
// tripwire, FileRef::FindComponent, the two-word mAptRef copy, the
// BrnFlaptMovieClipRef.h:272 "mpMovieClipInst" tripwire and ResetTimeline -- asm
// 0x82420CA0..0x82420D18); calling the reconstructed out-of-line base reproduces it
// exactly. No parent prefix is passed, so the slot binds under its bare name. The
// embedded item then prepares under the composite "<name>_MessageItem".
// ===================================================================================
void BoostMessageSlot::Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile)
{
    BrnFlaptComponent::Prepare(lpacName, lrFile, 0);

    char lacComponentName[KI_COMPONENT_PATH_LENGTH];
    CgsCore::SPrintf(lacComponentName, KI_COMPONENT_PATH_LENGTH, "%s_%s", lpacName, "MessageItem");
    lacComponentName[KI_COMPONENT_PATH_LENGTH - 1] = '\0';

    mMessageItem.Prepare(lacComponentName, lrFile);   // @0x82420D50
}

// ===================================================================================
// SetMessage  @ 0x82420D60 -- cpp:152
//
// Stage a message into a free slot: latch the ttl / id / in-use, snap the stack
// position back to the baseline, push the text (with the boost amount) and play the
// item's TransitionIn carrying the boost type.
// ===================================================================================
void BoostMessageSlot::SetMessage(const char* lpcText, s32 liMessageId, f32 lfTimeToLive,
                                  s32 leBoostType, s32 liBoostAmount)
{
    CGS_ASSERT(!mbInTransition && !mbInUse,
               "Slot must be available and not in transition");   // :152 (streamed; folded, non-gating)

    mfTimeToLive = lfTimeToLive;   // stfs f31, 0x40
    miMessageId  = liMessageId;    // stw  r26,  0x38
    mbInUse      = true;           // stb  1,    0x34

    ResetSlotPosition();                                                          // @0x82420E2C
    mMessageItem.SetText(lpcText, liBoostAmount);                                 // @0x82420E40
    mMessageItem.SetTransition(BoostMessageItem::E_TRANSITION_ON, leBoostType);    // @0x82420E50 (r4 = 0)
}

// ===================================================================================
// SetMessageText  @ 0x82411E30 -- cpp:177
//
// Update the live message's text. A pure forward onto the embedded item once the
// in-use tripwire has been streamed.
// ===================================================================================
void BoostMessageSlot::SetMessageText(const char* lpcText, s32 liBoostAmount)
{
    CGS_ASSERT(mbInUse, "Setting text on an expired object is nonsensical");   // :177 (streamed; folded, non-gating)

    mMessageItem.SetText(lpcText, liBoostAmount);   // @0x82411ED8
}

// ===================================================================================
// Refresh  @ 0x82411EE8
//
// Raise the ttl to the requested floor (fcmpu/bge -- it never LOWERS it) and, when
// asked and nothing is transitioning on either clip, pulse the item's
// "MessageRefresh". No asserts in this body.
// ===================================================================================
void BoostMessageSlot::Refresh(f32 lfTimeToLive, bool lbPlayRefreshAnim)
{
    if (mfTimeToLive < lfTimeToLive)
        mfTimeToLive = lfTimeToLive;

    if (lbPlayRefreshAnim && !mbInTransition && !mMessageItem.IsInTransition())
    {
        // NOTE: the X360 tests the item's latch TWICE -- once off `this` (lbz
        // 0x2C(r3) @0x82411F10) and once off a freshly formed &mMessageItem (lbz
        // 0x20(r11) @0x82411F20, the head of the inlined item-side play helper).
        // Same byte, same answer, so one source-level test reproduces it.
        mMessageItem.PlayLabel("MessageRefresh");   // item mAptRef -> GotoAndPlayLabel @0x8246F3E8
    }
}

// ===================================================================================
// ShuffleUp  @ 0x82411F40 -- cpp:283 / :284 / :285
//
// Step the slot one stack position up: play the slot clip's "<from>-<to>" transition
// frame, raise the shuffle latch and commit the new position. All three tripwires are
// streamed and NON-GATING on the console (the shuffle plays regardless).
// ===================================================================================
void BoostMessageSlot::ShuffleUp()
{
    CGS_ASSERT(!mbInTransition, "Cannot shuffle when already shuffling");               // :283
    CGS_ASSERT(miSlotPosition < KI_MAX_SLOT_POSITION, "Shuffling beyond our bounds");   // :284
    CGS_ASSERT(mbInUse, "Cannot shuffle when not in use");                              // :285

    char lacLabel[KI_SHUFFLE_LABEL_LENGTH];
    CgsCore::SPrintf(lacLabel, KI_SHUFFLE_LABEL_LENGTH, "%d-%d", miSlotPosition, miSlotPosition + 1);

    mAptRef.GotoAndPlayLabel(lacLabel);   // @0x824120C4 (sub_8246F3E8 on this+0x04)

    mbInTransition = true;
    ++miSlotPosition;
}

// ===================================================================================
// ShuffleDown  @ 0x824120E8 -- cpp:313 / :314 / :315
//
// The downward counterpart: "<from>-<to>" with to == from - 1, guarded above the
// baseline instead of below the ceiling.
// ===================================================================================
void BoostMessageSlot::ShuffleDown()
{
    CGS_ASSERT(!mbInTransition, "Cannot shuffle when already shuffling");   // :313
    CGS_ASSERT(miSlotPosition > 0, "Shuffling below the baseline");         // :314
    CGS_ASSERT(mbInUse, "Cannot shuffle when not in use");                  // :315

    char lacLabel[KI_SHUFFLE_LABEL_LENGTH];
    CgsCore::SPrintf(lacLabel, KI_SHUFFLE_LABEL_LENGTH, "%d-%d", miSlotPosition, miSlotPosition - 1);

    mAptRef.GotoAndPlayLabel(lacLabel);   // @0x8241226C (sub_8246F3E8 on this+0x04)

    mbInTransition = true;
    --miSlotPosition;
}

// ===================================================================================
// ResetSlotPosition  @ 0x82412290 -- cpp:343
//
// Snap the slot clip to the baseline frame and zero the stack position.
// ===================================================================================
void BoostMessageSlot::ResetSlotPosition()
{
    CGS_ASSERT(!mbInTransition,
               "Please wait for all transitions to complete before changing slot position");   // :343 (streamed; folded)

    mAptRef.GotoAndStopLabel("Position0");   // @0x82412330
    miSlotPosition = 0;
}

// ===================================================================================
// Update  @ 0x82420E60 -- cpp:266
//
// The per-frame tick, in two independent halves:
//   1. While in use, run the ttl down; at expiry transition the item out; once the
//      ttl has gone negative AND both clips have settled, free the slot.
//   2. UNCONDITIONALLY (the console falls into this even when the slot is idle),
//      consume the slot clip's shuffle latch and the item clip's transition latch on
//      their settle frames.
// ===================================================================================
void BoostMessageSlot::Update(f32 lfTimeStep)
{
    if (mbInUse)
    {
        if (mfTimeToLive < 0.0f)   // flt_82001CC0 == 0.0f
        {
            // Already run out: free the slot the moment neither clip is animating.
            if (!mbInTransition && !mMessageItem.IsInTransition())
            {
                mbInUse        = false;   // stb r30, 0x34
                miSlotPosition = 0;       // stw r30, 0x3C
                miMessageId    = -1;      // stw r11, 0x38
            }
        }
        else
        {
            const f32 lfRemaining = mfTimeToLive - lfTimeStep;
            if (lfRemaining > 0.0f)
            {
                mfTimeToLive = lfRemaining;
            }
            else if (!mMessageItem.IsInTransition())
            {
                // Time up with the item settled: play it out, then commit the (now
                // non-positive) ttl so the expiry arm above picks it up next frame.
                // The X360 RELOADS +0x40 and re-subtracts after the call
                // (0x82420ECC/0x82420ED0) -- SetTransition cannot have touched it, so
                // that reload is redundant, NOT a second decrement.
                mMessageItem.SetTransition(BoostMessageItem::E_TRANSITION_OFF, -1);   // @0x82420EC8 (r4 = 1, r5 = -1)
                mfTimeToLive = mfTimeToLive - lfTimeStep;
            }
            // else: the item is still transitioning IN at expiry -- the X360 branches
            // clear of the `stfs 0x40` entirely (0x82420EB8), so the ttl FREEZES until
            // the item settles.
        }
    }

    // ---- the slot clip's shuffle latch ---------------------------------------
    const s32 liSlotFrame = mAptRef.mpMovieClipInst->GetCurrentFrameOneBased();
    if (liSlotFrame == KAI_SLOT_SETTLE_FRAMES[0] || liSlotFrame == KAI_SLOT_SETTLE_FRAMES[1]
        || liSlotFrame == KAI_SLOT_SETTLE_FRAMES[2] || liSlotFrame == KAI_SLOT_SETTLE_FRAMES[3])
    {
        mbInTransition = false;
    }

    // The tripwire runs AFTER the clear (the console re-reads both the latch and the
    // frame here -- 0x82420F34/0x82420F40); non-gating.
    CGS_ASSERT(!mbInTransition || liSlotFrame <= KI_SLOT_TRANSITION_FRAME_LIMIT,
               "!mbInTransition || mMovieClipRef.GetCurrentFrameOneBased()<=34");   // :266

    // ---- the item clip's transition latch ------------------------------------
    const s32 liItemFrame = mMessageItem.GetAptRef().mpMovieClipInst->GetCurrentFrameOneBased();
    if (liItemFrame == KAI_ITEM_SETTLE_FRAMES[0] || liItemFrame == KAI_ITEM_SETTLE_FRAMES[1]
        || liItemFrame == KAI_ITEM_SETTLE_FRAMES[2])
    {
        mMessageItem.ClearInTransition();
    }
}

}
