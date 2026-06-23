#pragma once

// ===================================================================================
// BrnGui::TextSelection  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h
//
// A reusable text-selection widget: a list of up to 100 selectable text rows (the X360
// ctor @0x824F9A48 initialises a 100-entry, 0x18-stride row table at object +0x238).
// It is embedded as a sub-object inside several car-select / crash-nav screen states.
//
// No prior source carries this widget's full member layout (no Feb-2007 leak entry and no
// DecFIGS DWARF for this TU). The recovered shape comes only from the ctor's vtable/field
// initialisation sequence: a polymorphic head (virtual; main vtable @+0x000, an embedded
// sub-object vtable @+0x018), the 100-entry row table @+0x238, and a trailing field
// @+0xB98 -- total size 0xB9C. The widget's sub-object types are not individually
// recoverable from this single ctor, so the body is kept as reserved byte-spans (the
// virtual gives the object the leading vtable the X360 writes at +0). All access is by
// name. The per-row and per-sub-object vtable stores the X360 emits are the compiler's
// own initialisation of those unrecovered sub-objects and are not reproduced field-by-
// field here.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    struct TextSelection
    {
        static const s32 KI_MAX_ROWS = 100;   // X360 ctor loop count (r11 = 0x63 .. -1)

        // @ 0x824F9A48 - default-construct the widget (clears the row table). Polymorphic so
        // the object carries the leading vtable the X360 stores at +0x000.
        TextSelection();
        virtual ~TextSelection() {}

    private:
        // Reserved storage spanning the unrecovered widget body. The leading vtable is
        // provided by the virtual above; everything from just past it up to the trailing
        // field at +0xB98 (object size 0xB9C) is layout-recovery padding only.
        u8 maWidgetBodyReserved[0xB9C - sizeof(void*)];
    };
}
