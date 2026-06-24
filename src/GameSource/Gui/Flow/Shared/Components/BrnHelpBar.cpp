// ===================================================================================
// BrnGui::HelpBar  -- implementation
//   class:BrnGui::HelpBar
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// item records and embedded animators sit inside reserved spans of the owning header
// (their full element layout is not recovered by these accessors), so the returned
// interior pointer is taken from the named member's address.
//
// NOTE: the third ledger function, HelpBar::HelpBar @ 0x82515328, is NOT reconstructed
// here -- see the block note at the bottom of this file.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @ 0x824E2280 - return item `luIndex`'s name hash by value. The X360 tests both ends
    // of the index (signed `< 0` and `>= 7`), streaming the dynamic "Invalid item index"
    // message into the assert buffer (BrnHelpBar.h:197) on failure; the house assert
    // forwards the static text. Then loads the u32 at this + 428*luIndex + 0x110
    // (mulli 0x1AC / add / lwz 0x110) and returns it.
    u32 HelpBar::GetItemNameHash(u32 luIndex)
    {
        CGS_ASSERT(static_cast<s32>(luIndex) >= 0 &&
                       static_cast<s32>(luIndex) < KI_MAX_ITEMS,
                   "Invalid item index");

        return maItems[luIndex].muNameHash;
    }

    // @ 0x824E2330 - return the interior pointer to item `luIndex`'s animator. Same
    // dual-ended index bound as GetItemNameHash, faulting "Invalid item index"
    // (BrnHelpBar.h:241). Then returns this + 648*luIndex + 0xC40 (mulli 0x288 / add /
    // addi 0xC40) == &maAnimators[648*luIndex].
    HelpBar::Animator* HelpBar::GetAnimator(u32 luIndex)
    {
        CGS_ASSERT(static_cast<s32>(luIndex) >= 0 &&
                       static_cast<s32>(luIndex) < KI_MAX_ITEMS,
                   "Invalid item index");

        return &maAnimators[luIndex];
    }

    // ------------------------------------------------------------------------------------
    // BLOCKED: HelpBar::HelpBar @ 0x82515328 is left declared-but-undefined.
    //
    // The X360 ctor is a heavy embedded-aggregate constructor: it installs the bar's own
    // vtable (off_8207710C @ +0x00) and a chain of sub-component vtables in the head
    // (off_82072FE8 / off_82072F88 at +0x8C, +0x118, +0x1A8, ... 21 entries), then loops
    // 7 times (one per help item, 0x288 stride from +0xC40) running two
    // `vector constructor iterator` passes per item -- CgsGui::AnimChannel::AnimChannel
    // over 6 elements at item+0x14 (0x24 stride) and CgsGui::AnimData::AnimData over 2
    // elements at item+0xF0 (0xB0 stride) -- interleaved with two more sub-object vtable
    // installs per item (off_8206CED8 then off_8206CEE8 at item+0x00) and -1 sentinels at
    // item+0xEC / +0x250 / +0x284.
    //
    // Faithful reconstruction needs the full per-item layout, including the several
    // polymorphic sub-objects whose vtable symbols (off_8207710C, off_82072FE8,
    // off_82072F88, off_8206CED8, off_8206CEE8) are not identified in this TU. Modeling
    // those as raw addresses would assert symbol identities that are not recovered, and a
    // whole-struct zero would be WRONG (the asm writes specific vtables and -1 sentinels,
    // not zeros). This is an un-homed aggregate constructor that cannot be grown
    // additively from the accessor offsets alone, so it is blocked rather than faked.
    // ------------------------------------------------------------------------------------
}
