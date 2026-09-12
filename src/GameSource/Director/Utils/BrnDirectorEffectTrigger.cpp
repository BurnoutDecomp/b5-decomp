#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"

#include <cstring>                                   // strcmp (the open-coded console compares)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Director/Camera/Camera.h"       // Camera::Camera / GetEffects (EnsureEffectIsPlaying)

// BrnDirector::EffectInterface.
//
// Bodied here (1 ledger function, whose primary file in the original is
// GameSource/Director/Utils/BrnDirectorEffectTrigger.cpp):
//   EffectInterface::Update(s32, const char* const*, bool*)
//
// Attested behaviour: clear the hook table (the inlined Array::Clear -- zero the count
// word at +0xCE4), then when liNumHooks >= 0 wrap + append every name -- each NULL name
// fires the streamed assert ("Registering a NULL effect hook name at index:" + index +
// " maybe there's some limit set elsewhere?"; folded static per convention, non-gating:
// the console still Set()s the null name) -- and latch mbGotHooks. The out-flag asks the
// caller to (re)enumerate the hooks while none have been registered yet.

namespace BrnDirector
{

void EffectInterface::Update(s32 liNumHooks, const char* const* lapHookNames,
                             bool* lpbRequestEnumerationOut)
{
    maHookNames.Clear();

    if (liNumHooks >= 0)
    {
        for (s32 liLoop = 0; liLoop < liNumHooks; ++liLoop)
        {
            CGS_ASSERT(lapHookNames[liLoop] != NULL,
                       "Registering a NULL effect hook name at index:");

            HookNameStringWrapper lHookNameStringWrapper;
            lHookNameStringWrapper.Set(lapHookNames[liLoop]);
            maHookNames.Append(lHookNameStringWrapper);
        }

        mbGotHooks = true;
    }

    *lpbRequestEnumerationOut = (mbGotHooks == false);
}

}


// ============================================================================
// BrnDirector::BackgroundEffectRequest (class TU).
//   GetBackgroundStartRequestBlendAmount   (guarded by its own tripwire)
//   RegisterAndUpdateRequest               (guarded by its own tripwire)
// ============================================================================
namespace BrnDirector
{
    // Non-gating guard, then the staged blend.
    f32 BackgroundEffectRequest::GetBackgroundStartRequestBlendAmount() const
    {
        CGS_ASSERT(HasBackgroundStartRequest(), "HasBackgroundStartRequest()");   // non-gating
        return mfBlendAmount;
    }

    // Apply the pending request against the live interface: a stop request stops the
    // named background hook; a start request registers it (the console inlines
    // RegisterStartingBackgroundEffectWithName's three stores).
    // NOTE (attested): the pending flag clears ONLY when the hook does NOT exist yet --
    // an applied request stays pending and re-applies each frame.
    void BackgroundEffectRequest::RegisterAndUpdateRequest(EffectInterface* lpEffectInterface)
    {
        CGS_ASSERT(lpEffectInterface != 0, "lpEffectInterface != NULL");

        if (mbStartRequested)
        {
            if (mbStopRequest)
            {
                if (lpEffectInterface->HookExists(mHookName.mHookNameString))
                {
                    lpEffectInterface->RegisterStoppingBackgroundEffectWithName(mHookName);
                    return;
                }
            }
            else
            {
                if (lpEffectInterface->HookExists(mHookName.mHookNameString))
                {
                    lpEffectInterface->RegisterStartingBackgroundEffectWithName(mHookName, mfBlendAmount);
                    return;
                }
            }
            mbStartRequested = false;
        }
    }

// ============================================================================
// The EffectInterface current-effect surface (class:BrnDirector::EffectInterface,
// batch 15): the blend accessor and the three register entry points.
// ============================================================================

// The tripwire (non-gating), then the blend.
f32 EffectInterface::GetCurrentEffectBlendAmount() const
{
    CGS_ASSERT(mbHasCurrentEffectName, "HasCurrentEffectName()");   // non-gating
    return mfCurrentEffectBlendAmount;
}

// Adopt a starting camera-PFX effect: drop the id form, raise the name form, copy the
// name, store the blend -- in the console's own store order.
void EffectInterface::RegisterStartingEffectWithName(const HookNameStringWrapper& lrName,
                                                     f32 lfBlend)
{
    mbHasCurrentEffectId   = false;
    mbHasCurrentEffectName = true;
    mCurrentEffectName.Set(lrName.mHookNameString);
    mfCurrentEffectBlendAmount = lfBlend;
}

// Does the game currently publish a camera-PFX hook by this name?
// Attested behaviour: bail FALSE when the hook table has not been enumerated yet
// (mbGotHooks, +0xD36); otherwise wrap the raw name into a stack HookNameStringWrapper
// through HookNameStringWrapper::Set and ask the table. The tail call is
// Array<HookNameStringWrapper,100>::Contains.
bool EffectInterface::HookExists(const char* lpcName) const
{
    if (!mbGotHooks)
    {
        return false;
    }

    HookNameStringWrapper lHookNameStringWrapper;
    lHookNameStringWrapper.Set(lpcName);

    return maHookNames.Contains(lHookNameStringWrapper);
}

// The background counterpart of RegisterStartingEffectWithName. NO STANDALONE CONSOLE
// SYMBOL -- the console inlines it into its only caller,
// BackgroundEffectRequest::RegisterAndUpdateRequest, whose three stores ARE this body,
// in this order:
//   store true into the flag at +0xD38   -> mbHasCurrentBackgroundEffectName = true
//   HookNameStringWrapper::Set on +0xD15 -> mCurrentBackgroundHookName
//   store the blend into +0xCF0          -> mfCurrentBackgroundEffectBlendAmount = lfBlend
// (Note the flag is raised BEFORE the name copy -- reproduced.) Unlike the foreground twin it
// does NOT clear mbHasCurrentEffectId; the console really does leave the id form alone here.
void EffectInterface::RegisterStartingBackgroundEffectWithName(const HookNameStringWrapper& lrName,
                                                               f32 lfBlend)
{
    mbHasCurrentBackgroundEffectName = true;
    mCurrentBackgroundHookName.Set(lrName.mHookNameString);
    mfCurrentBackgroundEffectBlendAmount = lfBlend;
}

// Drop the current effect when the stopping name matches it (or when nothing is
// current -- the console falls into the same clear).
void EffectInterface::RegisterStoppingEffectWithName(const HookNameStringWrapper& lrName)
{
    if (!mbHasCurrentEffectName || mCurrentEffectName == lrName.mHookNameString)
        mbHasCurrentEffectName = false;
}

// The background counterpart. FAITHFUL QUIRK: the console gates on the FOREGROUND
// has-flag (+0xD37) and compares against the FOREGROUND name (+0xCF4) -- not the
// background pair -- while clearing the BACKGROUND has-flag (+0xD38); reproduced as-is.
void EffectInterface::RegisterStoppingBackgroundEffectWithName(const HookNameStringWrapper& lrName)
{
    if (!mbHasCurrentEffectName || mCurrentEffectName == lrName.mHookNameString)
        mbHasCurrentBackgroundEffectName = false;
}

}


// ============================================================================
// BrnDirector::Camera::EnsureEffectIsPlaying -- a FREE function in namespace
// BrnDirector::Camera taking (Camera&, const EffectInterface&, const char*, f32); the
// signature is recovered from the attested calls, not from a decompiler's guess.
//
// Attested behaviour, with the camera-relative fields resolved through mEffects
// @camera +0x68 (BrnCameraEffects.h's console-proven 0xBC block):
//   clear mEffects.mbHasStartHookNameString (+0xB7)
//   clear mEffects.mbHasStopHookNameString  (+0xB8)
//   zero  mEffects.muRequestedPostFxId      (+0x7C)
//   read EffectInterface::mbHasCurrentEffectName (+0xD37)
//               if clear                  -> jump straight to the request
//   compare mCurrentEffectName (+0xCF4, read inline) against lpcHook,
//               if DIFFERENT              -> jump to the request
//   call EffectInterface::GetCurrentEffectName and compare against lpcHook again,
//               if DIFFERENT              -> RETURN (do nothing)   <- note the asymmetry, it
//                                                                     is the third || term
//                                                                     short-circuiting
//   call EffectInterface::GetCurrentEffectBlendAmount
//               if EQUAL to lfBlend       -> RETURN (already playing at this blend)
//   the request:
//                 HookNameStringWrapper::Set(mEffects +0x00, lpcHook) -> mStartHookNameString
//                 store lfBlend into mEffects +0x80          -> mfStartHookNameBlendAmount
//                 store true  into mEffects +0xB7            -> mbHasStartHookNameString
//               i.e. exactly CameraEffects::SetStartHookName, but with the +0xB7 / +0x80
//               store order swapped -- both are independent stores, so this is the same
//               named operation.
//
// ⚠️ THE REDUNDANT SECOND NAME COMPARE IS FAITHFUL, not a transcription slip. The console
// evaluates the current-effect NAME twice: once inlined off +0xCF4 and once through the
// out-of-line accessor (which carries the tripwire). Reproduced as the three-term ||
// below, which is the only shape that reproduces the branch table exactly -- in particular
// the A && B && !C case, where the console silently does NOTHING.
// ============================================================================
namespace BrnDirector
{
namespace Camera
{

void EnsureEffectIsPlaying(Camera& lrCamera, const EffectInterface& lrSource,
                           const char* lpcHook, f32 lfBlend)
{
    CameraEffects& lrEffects = lrCamera.GetEffects();

    // The three unconditional clears at the head (the previous frame's request is dropped
    // before anything else is decided).
    lrEffects.mbHasStartHookNameString = false;   // +0xB7
    lrEffects.mbHasStopHookNameString  = false;   // +0xB8
    lrEffects.muRequestedPostFxId      = 0;       // +0x7C

    if (!lrSource.HasCurrentEffectName()                                        // +0xD37
        || strcmp(lrSource.GetCurrentEffectName(), lpcHook) != 0                // inlined off +0xCF4
        || (strcmp(lrSource.GetCurrentEffectName(), lpcHook) == 0               // through the accessor
            && lrSource.GetCurrentEffectBlendAmount() != lfBlend))
    {
        lrEffects.SetStartHookName(lpcHook, lfBlend);
    }
}

// ============================================================================
// BrnDirector::Camera::StopCurrentEffect -- BODIED 2026-08-01.
// Takes (Camera&, const EffectInterface&); no float argument, no return.
//
// Attested behaviour (camera fields resolved through mEffects @camera +0x68):
//   clear mEffects.mbHasStartHookNameString (+0xB7)
//   zero  mEffects.muRequestedPostFxId      (+0x7C)
//   read  EffectInterface::mbHasCurrentEffectName (+0xD37)
//   when SET: HookNameStringWrapper::Set(mEffects +0x21, mCurrentEffectName at +0xCF4)
//               -- mEffects +0x21 is mStopHookNameString -- then raise mEffects +0xB8,
//               and RETURN.
//   when CLEAR: read mbHasCurrentEffectId (+0xD39) AND the current effect id (+0xCE8);
//               when that id != 0x7BEC6 (the null effect id), store 0x7BEC6 into
//               mEffects.muRequestedPostFxId, i.e. REQUEST the null post-FX rather than
//               leaving it at the 0 written at entry.
//
// ⚠️ Note the two different "off" values for muRequestedPostFxId: 0 unconditionally at entry,
// then the null-effect id 507078 on the id path. Both stores are the console's; keep both.
// ============================================================================
void StopCurrentEffect(Camera& lrCamera, const EffectInterface& lrSource)
{
    CameraEffects& lrEffects = lrCamera.GetEffects();

    lrEffects.mbHasStartHookNameString = false;   // +0xB7
    lrEffects.muRequestedPostFxId      = 0;       // +0x7C

    if (lrSource.HasCurrentEffectName())
    {
        // Ask for the live hook to be stopped by name.
        lrEffects.mStopHookNameString.Set(lrSource.GetCurrentEffectName());
        lrEffects.mbHasStopHookNameString = true;
        return;
    }

    // Nothing is playing by name: if an effect ID is live and it is not already the null one,
    // request the null effect id.
    bool lbRequestNullEffectId = false;
    if (lrSource.HasCurrentEffectId() &&
        lrSource.GetCurrentEffectId() != lrSource.GetNullEffectId())
    {
        lbRequestNullEffectId = true;
    }

    if (lbRequestNullEffectId)
    {
        lrEffects.muRequestedPostFxId = lrSource.GetNullEffectId();
    }
}

// ============================================================================
// BrnDirector::Camera::RequestStartEffectHook -- BODIED 2026-08-01.
//
// NO STANDALONE CONSOLE SYMBOL: the console emits the same three stores inline at every
// site. The clearest copy is ArbStateRoaming::ProcessPossiblePaybackEffects, working off
// &mCamera.mEffects (mCamera @state +0x10, mEffects @camera +0x68):
//   HookNameStringWrapper::Set(mEffects +0x00, name) -> mEffects.mStartHookNameString
//   store the constant 1.0f into mEffects +0x80      -> mEffects.mfStartHookNameBlendAmount
//   store true into mEffects +0xB7                   -> mEffects.mbHasStartHookNameString
// which is exactly CameraEffects::SetStartHookName(name, blend). Unlike
// EnsureEffectIsPlaying this is UNCONDITIONAL -- it never consults the EffectInterface.
// ============================================================================
void RequestStartEffectHook(Camera& lrCamera, const char* lpcHook, f32 lfBlend)
{
    lrCamera.GetEffects().SetStartHookName(lpcHook, lfBlend);
}

// ============================================================================
// BrnDirector::Camera::EnsureEffectIsStopped -- BODIED 2026-09-12 (was declaration-only).
// A free function in namespace BrnDirector::Camera taking (Camera&, const EffectInterface&,
// const char*); no float argument, no return. The complement of EnsureEffectIsPlaying --
// make sure the NAMED hook is not left playing on lrCamera.
//
// Walk, with the camera-relative displacements resolved through mEffects @camera +0x68:
//   * when a start-hook request is pending (mEffects +0xB7) and its name (mEffects +0x00) is
//     the hook we are stopping, drop the request -- the console open-codes the compare
//     against mStartHookNameString rather than calling the accessor.
//   * then, when the interface publishes a current effect NAME (+0xD37) that is this same
//     hook, AND the start-hook request is (now) clear, ask for the hook to be stopped by
//     name: mEffects.mStopHookNameString = lpcHook, mEffects +0xB8 = true.
// Note the second block re-reads the start-hook flag AFTER the first block may have cleared
// it, so a request raised THIS frame suppresses the stop -- reproduced as written.
// The console's stop-name copy uses the CALLER's hook string, not the interface's.
// ============================================================================
void EnsureEffectIsStopped(Camera& lrCamera, const EffectInterface& lrSource,
                           const char* lpcHook)
{
    CameraEffects& lrEffects = lrCamera.GetEffects();

    if (lrEffects.mbHasStartHookNameString
        && strcmp(lrEffects.mStartHookNameString.mHookNameString, lpcHook) == 0)
    {
        lrEffects.mbHasStartHookNameString = false;
    }

    if (lrSource.HasCurrentEffectName()
        && strcmp(lrSource.GetCurrentEffectName(), lpcHook) == 0
        && !lrEffects.mbHasStartHookNameString)
    {
        lrEffects.mStopHookNameString.Set(lpcHook);
        lrEffects.mbHasStopHookNameString = true;
    }
}

// ============================================================================
// BrnDirector::Camera::RequestStartEffectHookReset -- BODIED 2026-09-12.
//
// NO STANDALONE CONSOLE SYMBOL: the shipped build emits the same six stores inline at each
// site. Two identical copies (ArbStateOnlineCarSelect::Update's SELECTING_LIVERY arm and
// ArbStateOnlineRaceIntro::Update's epilogue) give, against mEffects @camera +0x68:
//   clear mEffects +0xB7   -> mbHasStartHookNameString = false
//   clear mEffects +0xB8   -> mbHasStopHookNameString  = false
//   zero  mEffects +0x7C   -> muRequestedPostFxId      = 0
//   HookNameStringWrapper::Set(mEffects +0x00, name)  \
//   store the blend into mEffects +0x80                > == CameraEffects::SetStartHookName
//   raise mEffects +0xB7                              /
// i.e. EnsureEffectIsPlaying's three unconditional clears followed by an UNCONDITIONAL
// start-hook request: the previous frame's whole request state is reset before the new hook is
// armed, and the EffectInterface is never consulted.
// ============================================================================
void RequestStartEffectHookReset(Camera& lrCamera, const char* lpcHook, f32 lfBlend)
{
    CameraEffects& lrEffects = lrCamera.GetEffects();

    lrEffects.mbHasStartHookNameString = false;
    lrEffects.mbHasStopHookNameString  = false;
    lrEffects.muRequestedPostFxId      = 0;

    lrEffects.SetStartHookName(lpcHook, lfBlend);
}

}
}
