// ===================================================================================
// BrnGui::TextSelection  -- implementation
//   class:BrnGui::TextSelection
//
// TextSelection (ctor) @ 0x824F9A48
//   Default-constructs the text-selection widget. The X360 writes the object's own vtable
//   (+0x000) and an embedded sub-object vtable (+0x018), then runs a 100-iteration loop
//   that initialises the 0x18-stride row table at +0x238 (each entry receives its row
//   vtable off_8207182C), and finally sets a trailing field at +0xB98. Those stores are
//   the compiler's initialisation of the widget's embedded sub-objects, whose individual
//   types are not recoverable from this ctor; the recovered effect is "construct an empty
//   100-row selection widget", which this body reproduces by zero-initialising the
//   reserved body. Reconstructed from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"

namespace BrnGui
{
    // @ 0x824F9A48
    TextSelection::TextSelection()
    {
        // Clear the row table / widget body (the X360 reinitialises every embedded
        // sub-object vtable; the observable post-state is an empty 100-row widget).
        for (u32 lu = 0; lu < sizeof(maWidgetBodyReserved); ++lu)
            maWidgetBodyReserved[lu] = 0;
    }
}
