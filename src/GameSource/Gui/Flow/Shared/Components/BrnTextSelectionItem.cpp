// ===================================================================================
// BrnGui::TextSelectionItem  -- implementation
//   class:BrnGui::TextSelectionItem
//
//   TextSelectionItem() @ 0x824F2120 -- installs the class vtable at this+0 and returns.
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelectionItem.h"

namespace BrnGui
{
    // @ 0x824F2120
    // The X360 ctor materialises this class's vtable (off_8207182C) and stores it at this+0
    // (lis/addi off_8207182C; stw r11,0(r3); blr); it stores nothing else. Because
    // TextSelectionItem is polymorphic (it overrides Selectable::Select -- DWARF
    // BrnTextSelectionItem.h:96), the compiler emits the vtable store implicitly, so the C++
    // body is empty. off_8207182C is the per-row selection-item vtable; TextSelection embeds
    // these rows (0x18 stride at +0x238, ctor @0x824F9A48).
    TextSelectionItem::TextSelectionItem()
    {
    }
}
