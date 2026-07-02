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
