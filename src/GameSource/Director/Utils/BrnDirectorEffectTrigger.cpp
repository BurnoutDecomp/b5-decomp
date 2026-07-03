#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
