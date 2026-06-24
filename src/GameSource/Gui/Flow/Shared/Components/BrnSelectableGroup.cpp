// ===================================================================================
// BrnGui::SelectableGroup  -- implementation
//   class:BrnGui::SelectableGroup
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @ 0x8240FC98 -- assert 0 <= index < count (BrnSelectableGroup.h:248), set the
    // queried flag (*(this+0xC) |= 0x10), return maSelectables[index] (this[42+index]).
    u32 SelectableGroup::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miSelectableCount,
                   "Invalid index passed to SelectableGroup::GetSelectable()");
        muFlags |= KU_FLAG_QUERIED;
        return maSelectables[liIndex];
    }

    // @ 0x8240FD60 -- assert 0 <= highlighted < 100 (BrnSelectableGroup.h:298), return
    // maSelectables[highlighted] (this[42 + highlightedIndex]).
    u32 SelectableGroup::GetHighlighted()
    {
        CGS_ASSERT(miHighlightedIndex >= 0 && miHighlightedIndex < KI_MAX_SELECTABLES,
                   "No highlighted index to get");
        return maSelectables[miHighlightedIndex];
    }
}
