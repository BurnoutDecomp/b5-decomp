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
}
