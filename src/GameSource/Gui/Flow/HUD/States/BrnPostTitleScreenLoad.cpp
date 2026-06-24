#include "GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::PostTitleScreenLoad::GetResourcesToLoad, reconstructed from BURNOUT_X360_ARTIST.XEX
// @ 0x825080B0 (semantic parity, not byte match).
//
// X360 body: the function ignores both out-params and unconditionally runs the assert sequence
//   BeginAssert(); FireAssert("Should not get here", "<...>/BrnPostTitleScreenLoad.h", 63); EndAssert();
// then returns -- a "Should not get here" tripwire. This state is driven by its own
// mpGuiCache / video bookkeeping and is never asked for resources through the FSM's
// sResourceTuple loading path, so the override exists only to flag misuse. (Contrast
// BrnGui::BootLegal::GetResourcesToLoad @ 0x82508090, which returns a real .rdata table.)
//
// The Hex-Rays render drops the two pointer args because the body never touches them; the asm
// signature is the CgsGui::State virtual `void GetResourcesToLoad(const sResourceTuple**, u32*)`.
// Under CGS_ASSERT the original's d:\p4-baked file/line is dropped per policy; the plain
// condition string is forwarded. The assert is unconditional, so it is modelled as CGS_ASSERT(false, ...).

namespace BrnGui
{
    void PostTitleScreenLoad::GetResourcesToLoad(const CgsGui::sResourceTuple** /*lppResourceTuples*/,
                                                 u32* /*lpuNumberOfResources*/) const
    {
        CGS_ASSERT(false, "Should not get here");
    }
}
