#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETMENUITEM_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETMENUITEM_HPP

#include "types.hpp"
#include <cstddef>                                       // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetMenuItem.hpp
//
// ICE::ICEWidgetMenuItem -- the dev-only ICE camera-take editor's menu widget: a
// fixed-capacity list of up to four selectable text entries. Each entry owns a
// fixed-size text buffer; the live entry count follows the buffers. Built and
// populated by ICEWidgetMenu::Construct, which calls Prepare to set the active
// entry count and seed each active entry's text with a default label.
//
// LAYOUT (a flat struct; the base ICEWidget is empty, so the first member sits at
// offset 0):
//   macItemText[4][40] @0x00  -- four fixed-size NUL-terminated entry text buffers,
//                                40-byte stride each (4 * 40 = 160 bytes total)
//   miItemCount        @0xA0  -- number of active entries, clamped to [0,4]
//
// The 40-byte per-entry stride and the count field sitting immediately after the
// four buffers (+0xA0 == 4*40) are facts about how Prepare walks the entries: it
// advances the write cursor by 40 bytes per entry and stores the clamped count at
// +0xA0.
//
// FLAG: the member NAMES (macItemText / miItemCount) follow the project Hungarian
//       convention; the per-entry buffer LENGTH is modelled as the full 40-byte
//       stride (KI_ICE_MENU_ITEM_TEXT_LENGTH) because only the stride is recoverable
//       -- the entries are contiguous with no observed gap, so the buffer fills the
//       stride. The entry COUNT field type (s32) follows the clamp arithmetic.
// ============================================================================

namespace ICE
{
    // Capacity / buffer constants pinned by Prepare's clamp + stride.
    static const s32 KI_ICE_MENU_MAX_ITEMS        = 4;   // count clamped to [0,4]
    static const s32 KI_ICE_MENU_ITEM_TEXT_LENGTH = 40;  // per-entry stride (+40 bytes)

    struct ICEWidgetMenuItem : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Set the active entry count (clamped to [0,4]) and seed each active
        // entry's text buffer with the default label.
        // OWNED here (body in ICEWidgetMenuItem.cpp).
        // ------------------------------------------------------------------
        void Prepare(s32 liItemCount);

        // --- layout -------------------------------------------------------
        char macItemText[KI_ICE_MENU_MAX_ITEMS][KI_ICE_MENU_ITEM_TEXT_LENGTH]; // +0x00  four entry text buffers
        s32  miItemCount;                                                      // +0xA0  active entry count [0,4]
    };

    // Pin the offsets Prepare touches: it walks the entry buffers at a 40-byte
    // stride from offset 0 and stores the clamped count at +0xA0 (== 4 * 40). The
    // base is empty (EBO), so macItemText must land at offset 0.
    static_assert(offsetof(ICEWidgetMenuItem, macItemText)  == 0x00, "ICEWidgetMenuItem::macItemText offset");
    static_assert(offsetof(ICEWidgetMenuItem, miItemCount)  == 0xA0, "ICEWidgetMenuItem::miItemCount offset");
    static_assert(sizeof(((ICEWidgetMenuItem*)0)->macItemText[0]) == KI_ICE_MENU_ITEM_TEXT_LENGTH,
                  "ICEWidgetMenuItem entry stride must be 40 bytes");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETMENUITEM_HPP
