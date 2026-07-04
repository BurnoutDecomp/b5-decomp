// ===================================================================================
// BrnGui::ColourSelectionItem  -- implementation
//   class:BrnGui::ColourSelectionItem
//
//   ColourSelectionItem() @ 0x824F2130 -- installs the class vtable at this+0 and returns.
//   GetGradient()         @ 0x824E5500 -- copies both stored colour pointers out.
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelectionItem.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x824F2130
    // The X360 ctor installs this class's vtable (off_8207184C) at this+0 and returns; it stores
    // nothing else (the colour pointers / mbIsGradient are left for SetColour / SetGradient to
    // fill in). Because ColourSelectionItem is polymorphic (it overrides Selectable::Select), the
    // compiler emits the vtable store implicitly, so the C++ body is empty.
    ColourSelectionItem::ColourSelectionItem()
    {
    }

    // @ 0x824E5500
    // Hands back both gradient endpoint colours through the two out-pointers. Asserts each
    // out-pointer and each stored colour pointer is non-NULL, then copies mpcSelectionColour1 /
    // mpcSelectionColour2 out. const (reads only).
    void ColourSelectionItem::GetGradient(const rw::math::vpu::Vector4** lppv4OutColour1,
                                          const rw::math::vpu::Vector4** lppv4OutColour2) const
    {
        CGS_ASSERT(lppv4OutColour1 != 0, "lppv4OutColour1");   // @0x824E551C
        CGS_ASSERT(lppv4OutColour2 != 0, "lppv4OutColour2");   // @0x824E5544
        CGS_ASSERT(mpcSelectionColour1 != 0, "mpcSelectionColour1");   // @0x824E5568
        CGS_ASSERT(mpcSelectionColour2 != 0, "mpcSelectionColour2");   // @0x824E5590

        *lppv4OutColour1 = mpcSelectionColour1;   // stw @0x824E55BC
        *lppv4OutColour2 = mpcSelectionColour2;   // stw @0x824E55C4
    }
}
