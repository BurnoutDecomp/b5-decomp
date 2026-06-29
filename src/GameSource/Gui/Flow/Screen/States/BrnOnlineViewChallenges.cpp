// ===================================================================================
// BrnGui::OnlineViewChallenges  -- implementation
//   class:BrnGui::OnlineViewChallenges
//
// OnlineViewChallenges (ctor) @ 0x825005C0
//   Compiler-emitted construction of the online "view challenges" flow state and its
//   embedded GUI sub-objects. The X360 writes the state's own vtable (+0x000,
//   off_8207440C), two embedded sub-object vtables (this+0x40 +0x000 = off_8207306C and
//   this+0x40 +0x018 = off_82073068, with off_82055B70 first stored then overwritten),
//   constructs the embedded BrnGui::TextSelection widget (r3 = this+0x40 +0xA8 = this+0xE8,
//   bl TextSelection::TextSelection), then sets three more embedded sub-object slots
//   (this+0x40 +0xD68 = off_82072F8C, +0xED8 = off_82071820, +0x1860 and +0x18EC both
//   off_82072F68). Those per-sub-object vtable stores belong to widget types not
//   individually recoverable from this ctor; the recovered, modelled effect is "construct
//   the CgsGui::State base and the embedded TextSelection", which the member initialisation
//   here reproduces. Reconstructed from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineViewChallenges.h"

namespace BrnGui
{
    // @ 0x825005C0
    OnlineViewChallenges::OnlineViewChallenges()
        : CgsGui::State()       // state vtable + base bookkeeping (X360 +0x000)
        , mTextSelection()      // embedded widget ctor (X360 bl TextSelection::TextSelection)
    {
    }
}
