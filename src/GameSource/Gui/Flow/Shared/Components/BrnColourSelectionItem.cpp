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

    // ---- Select -- an EMPTY override, verified, not assumed -------------------------
    // This class's vtable (off_8207184C) reads
    //   +0x00 Selectable::SetActive        +0x04 Selectable::SetHighlightable
    //   +0x08 Selectable::SetSelectable    +0x0C Selectable::SetHighlighted
    //   +0x10 0x8284CB38                   +0x14 Selectable::Update
    // Slot 4 (+0x10) is Select, and 0x8284CB38 is the image-wide ICF fold of a bare `blr`
    // (193 xrefs -- NOT _purecall): every other slot resolves to the real base body, so the
    // item genuinely overrides Select with an empty one. Declaring it pure would make the
    // class abstract (it is embedded 100-per-picker BY VALUE) and routing it to the base
    // default would run the base's selection behaviour, which the console suppresses here.
    void ColourSelectionItem::Select()
    {
    }
}
