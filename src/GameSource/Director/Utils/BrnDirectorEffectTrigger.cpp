#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"

#include <cstring>                                   // strcmp (the open-coded console compares)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Director/Camera/Camera.h"       // Camera::Camera / GetEffects (EnsureEffectIsPlaying)

// BrnDirector::EffectInterface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Director/Utils/BrnDirectorEffectTrigger.cpp):
//   EffectInterface::Update(s32, const char* const*, bool*) @0x8221E0F0
//
// Asm walk: clear the hook table (the inlined Array::Clear -- `stw 0` into the
// count word at +0xCE4), then when liNumHooks >= 0 wrap + append every name --
// each NULL name fires the streamed assert ("Registering a NULL effect hook name
// at index:" + index + " maybe there's some limit set elsewhere?", cpp:57; folded
// static per convention, non-gating: the X360 still Set()s the null name) -- and
// latch mbGotHooks. The out-flag asks the caller to (re)enumerate the hooks while
// none have been registered yet.

namespace BrnDirector
{

// @ 0x8221E0F0
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
// BrnDirector::BackgroundEffectRequest (class TU) -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//   GetBackgroundStartRequestBlendAmount @0x823A79C8   (h:249 tripwire)
//   RegisterAndUpdateRequest             @0x82232E88   (h:361 tripwire)
// ============================================================================
namespace BrnDirector
{
    // h:249 -- non-gating guard, then the staged blend.
    f32 BackgroundEffectRequest::GetBackgroundStartRequestBlendAmount() const
    {
        CGS_ASSERT(HasBackgroundStartRequest(), "HasBackgroundStartRequest()");   // :249
        return mfBlendAmount;
    }

    // @ 0x82232E88 -- h:361. Apply the pending request against the live interface:
    // a stop request stops the named background hook; a start request registers it
    // (the X360 inlines RegisterStartingBackgroundEffectWithName's three stores).
    // NOTE (asm-pinned): the pending flag clears ONLY when the hook does NOT exist
    // yet -- an applied request stays pending and re-applies each frame.
    void BackgroundEffectRequest::RegisterAndUpdateRequest(EffectInterface* lpEffectInterface)
    {
        CGS_ASSERT(lpEffectInterface != 0, "lpEffectInterface != NULL");   // :361

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

// @ 0x821F1818 -- h:115 tripwire (non-gating), then the blend.
f32 EffectInterface::GetCurrentEffectBlendAmount() const
{
    CGS_ASSERT(mbHasCurrentEffectName, "HasCurrentEffectName()");   // :115 (non-gating)
    return mfCurrentEffectBlendAmount;
}

// @ 0x82203FD8 -- adopt a starting camera-PFX effect: drop the id form, raise the
// name form, copy the name, store the blend (the asm's store order).
void EffectInterface::RegisterStartingEffectWithName(const HookNameStringWrapper& lrName,
                                                     f32 lfBlend)
{
    mbHasCurrentEffectId   = false;
    mbHasCurrentEffectName = true;
    mCurrentEffectName.Set(lrName.mHookNameString);
    mfCurrentEffectBlendAmount = lfBlend;
}

// @ 0x821F1870 -- drop the current effect when the stopping name matches it (or
// when nothing is current -- the X360 falls into the same clear).
void EffectInterface::RegisterStoppingEffectWithName(const HookNameStringWrapper& lrName)
{
    if (!mbHasCurrentEffectName || mCurrentEffectName == lrName.mHookNameString)
        mbHasCurrentEffectName = false;
}

// @ 0x821F18C0 -- the background counterpart. FAITHFUL QUIRK: the X360 gates on
// the FOREGROUND has-flag (+0xD37) and compares against the FOREGROUND name
// (+0xCF4) -- not the background pair -- while clearing the BACKGROUND has-flag
// (+0xD38); reproduced as-is.
void EffectInterface::RegisterStoppingBackgroundEffectWithName(const HookNameStringWrapper& lrName)
{
    if (!mbHasCurrentEffectName || mCurrentEffectName == lrName.mHookNameString)
        mbHasCurrentBackgroundEffectName = false;
}

}


// ============================================================================
// BrnDirector::Camera::EnsureEffectIsPlaying @0x821F2720 -- a FREE function in
// namespace BrnDirector::Camera (r3 = Camera&, r4 = const EffectInterface&,
// r5 = const char*, f1 = f32; signature recovered from the asm, not from Hex-Rays).
//
// Asm walk, with the Camera-relative displacements resolved through mEffects @camera +0x68
// (BrnCameraEffects.h's X360-proven 0xBC block):
//   0x821F2744  stb 0, 0x11F(camera)      -> mEffects.mbHasStartHookNameString = false  (+0xB7)
//   0x821F2748  stb 0, 0x120(camera)      -> mEffects.mbHasStopHookNameString  = false  (+0xB8)
//   0x821F274C  stw 0, 0x0E4(camera)      -> mEffects.muRequestedPostFxId      = 0      (+0x7C)
//   0x821F2750  lbz 0xD37(source)         -> EffectInterface::mbHasCurrentEffectName
//               if clear                  -> jump straight to the request
//   0x821F275C  open-coded strcmp(source + 0xCF4, lpcHook)      -- mCurrentEffectName,
//               if DIFFERENT              -> jump to the request                 read inline
//   0x821F2790  bl EffectInterface::GetCurrentEffectName; open-coded strcmp against lpcHook,
//               if DIFFERENT              -> RETURN (do nothing)   <- note the asymmetry, it
//                                                                     is the third || term
//                                                                     short-circuiting
//   0x821F27D0  bl EffectInterface::GetCurrentEffectBlendAmount
//               if EQUAL to lfBlend       -> RETURN (already playing at this blend)
//   0x821F27DC  the request:
//                 HookNameStringWrapper::Set(camera + 0x68, lpcHook)  -> mStartHookNameString
//                 stfs lfBlend, 0x80(that)  == camera + 0xE8          -> mfStartHookNameBlendAmount
//                 stb  1,       0xB7(that)  == camera + 0x11F         -> mbHasStartHookNameString
//               i.e. exactly CameraEffects::SetStartHookName, but with the +0xB7 / +0x80
//               store order swapped -- both are independent stores, so this is the same
//               named operation.
//
// ⚠️ THE REDUNDANT SECOND NAME COMPARE IS FAITHFUL, not a transcription slip. The console
// evaluates the current-effect NAME twice: once inlined off +0xCF4 and once through the
// out-of-line accessor (which carries the h:112 tripwire). Reproduced as the three-term ||
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
    lrEffects.mbHasStartHookNameString = false;   // 0x821F2744
    lrEffects.mbHasStopHookNameString  = false;   // 0x821F2748
    lrEffects.muRequestedPostFxId      = 0;       // 0x821F274C

    if (!lrSource.HasCurrentEffectName()                                        // 0x821F2750
        || strcmp(lrSource.GetCurrentEffectName(), lpcHook) != 0                // 0x821F275C (inlined)
        || (strcmp(lrSource.GetCurrentEffectName(), lpcHook) == 0               // 0x821F2790 (call)
            && lrSource.GetCurrentEffectBlendAmount() != lfBlend))              // 0x821F27D0
    {
        // 0x821F27DC..0x821F27F4.
        lrEffects.SetStartHookName(lpcHook, lfBlend);
    }
}

}
}
