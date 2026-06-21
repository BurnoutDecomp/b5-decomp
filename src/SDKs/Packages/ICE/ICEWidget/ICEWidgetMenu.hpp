#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETMENU_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETMENU_HPP

#include "types.hpp"
#include <cstddef>                                            // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"          // ICE::ICEWidget base
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenuItem.hpp"  // ICE::ICEWidgetMenuItem (the row entries)

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp
//
// ICE::ICEWidgetMenu -- the dev-only ICE camera-take editor's on-screen menu
// widget. A scrollable grid of selectable text cells: a fixed number of rows,
// each row an ICEWidgetMenuItem with up to four text columns. The widget draws
// its own background box, a highlight box behind the selected row, an optional
// scrollbar, and then each visible cell as on-screen text (or an icon for the
// "<ICON>" cell marker). ICEController owns the family: ConstructMenus builds
// each menu, RefreshMenu fills the cells, JoyHandler drives MoveUp/MoveDown, and
// RenderMenus draws them.
//
// The menu's geometry is data-driven by a per-style table family keyed on the
// style index (mu... member at +0x00). Construct seeds the row/column/visible
// counts from that table; GetWidth sums the per-style column widths to size the
// background box. The five tables (row count, visible-line count, column count,
// a "selectable" flag, and the per-column width array) live in the ICE data
// segment and are reached only through the style index, so they are NOT modelled
// as members here -- they are file-local statics in the .cpp keyed by style.
//
// LAYOUT (all offsets PROVEN by the function bodies; pinned with static_assert):
//   miStyle           @0x00  -- style index; the key into the per-style tables.
//   miNumRows         @0x04  -- number of rows (row buffer is miNumRows entries).
//   miNumColumns      @0x08  -- number of columns per row (cells per ICEWidgetMenuItem).
//   miMaxVisibleRows  @0x0C  -- rows shown at once (scroll window height).
//   miScrollTop       @0x10  -- index of the first visible row (scroll position).
//   miSelectedRow     @0x14  -- selected row index, or -1 when this menu is not
//                               selectable (set from the per-style selectable flag).
//   maSelectBox       @0x20  -- a 4-float rect seeded for selectable menus (the
//                               initial highlight box: left/top/right/bottom-ish
//                               20/100/20/128). Only written when selectable.
//   mpRows            @0x30  -- ICEWidgetMenuItem* row buffer (miNumRows entries,
//                               164-byte stride), allocated from the ICE edit heap.
//   mfX               @0x34  -- screen-space left edge of the menu.
//   mfY               @0x38  -- screen-space top edge of the menu.
//
// FLAG: there is no layout reference for this widget. Every member NAME follows
//       the project Hungarian convention; every member TYPE/OFFSET is DERIVED from
//       the field accesses across the seven bodies. The roles of the count fields
//       (rows vs columns vs visible vs scroll-top vs selected) are inferred from how
//       Construct seeds them and how Render/Move* consume them.
// FLAG: maSelectBox is modelled as a 4-float rect because Construct stores a single
//       16-byte vector (20/100/20/128) into +0x20 only for selectable menus; the
//       exact corner roles of those four constants are not independently attested.
// FLAG: the gap between miSelectedRow (+0x14, ends +0x18) and maSelectBox (+0x20)
//       is unmodelled padding -- no body reads +0x18 or +0x1C, so it is left as
//       reserved bytes rather than invented fields.
// ============================================================================

namespace ICE
{
    // The ICE edit-heap manager Construct allocates the row buffer from. Referenced
    // by pointer only here, so an incomplete type suffices (full home:
    // SDKs/Packages/ICE/ICEMemory.hpp).
    struct ICEMemory;

    // The menu cell-text "empty" sentinel: ClearMenuContent seeds every cell with
    // it and GetNumNonEmptyItems treats a cell equal to it as empty. PLACEHOLDER:
    // the exact characters of the empty-cell string are not recoverable, so it is
    // modelled as the empty string (its observable role is "compares equal to a
    // freshly cleared cell"). FLAG: placeholder text.
    extern const char* const KPC_ICE_MENU_EMPTY_CELL;

    struct ICEWidgetMenu : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Build the menu over `lpMemory`: seed the row/column/visible counts and
        // the selectable flag from the per-style tables (keyed on liStyle), store
        // the screen position, allocate the row buffer from the ICE edit heap, and
        // Prepare() each row entry. OWNED here (body in the .cpp).
        // ------------------------------------------------------------------
        void Construct(ICEMemory* lpMemory, s32 liStyle, f32 lfX, f32 lfY);

        // ------------------------------------------------------------------
        // Reset every cell back to the empty sentinel (clamping the over-long
        // sentinel to a diagnostic string). OWNED here.
        // ------------------------------------------------------------------
        void ClearMenuContent();

        // ------------------------------------------------------------------
        // The width of the menu's background box: the left/right margins plus the
        // sum of the per-style column widths. OWNED here.
        // ------------------------------------------------------------------
        f32 GetWidth() const;

        // ------------------------------------------------------------------
        // The count of rows that contain at least one non-empty cell, scanning
        // from the last row down (used as the selectable-row count / wrap bound).
        // OWNED here.
        // ------------------------------------------------------------------
        s32 GetNumNonEmptyItems() const;

        // ------------------------------------------------------------------
        // Move the selection up one row, wrapping to the last non-empty row at the
        // top. OWNED here.
        // ------------------------------------------------------------------
        void MoveUp();

        // ------------------------------------------------------------------
        // Move the selection down one row, wrapping to the first row at the bottom.
        // OWNED here.
        // ------------------------------------------------------------------
        void MoveDown();

        // ------------------------------------------------------------------
        // Draw the menu: background box, selection highlight + scrollbar, then each
        // visible cell as on-screen text (or an icon for the "<ICON>" marker).
        // OWNED here.
        // ------------------------------------------------------------------
        void Render(s32 liRenderContext);

        // --- layout (offsets proven by the seven bodies) ------------------
        s32  miStyle;            // +0x00  style index (key into the per-style tables)
        s32  miNumRows;          // +0x04  number of rows
        s32  miNumColumns;       // +0x08  columns per row
        s32  miMaxVisibleRows;   // +0x0C  rows shown at once (scroll window)
        s32  miScrollTop;        // +0x10  first visible row index
        s32  miSelectedRow;      // +0x14  selected row, or -1 when not selectable
        u8   maReserved18[8];    // +0x18  unmodelled padding (no body reads it)
        f32  maSelectBox[4];     // +0x20  selectable-menu highlight rect (init only)
        ICEWidgetMenuItem* mpRows; // +0x30  row buffer (miNumRows * 164 bytes)
        f32  mfX;                // +0x34  screen-space left edge
        f32  mfY;                // +0x38  screen-space top edge
    };

    // Pin the offsets the bodies touch. The base is empty (EBO), so miStyle lands
    // at offset 0. Offsets through maSelectBox are pointer-width-invariant and are
    // asserted directly. mpRows / mfX / mfY are documented at their 32-bit offsets
    // (+0x30 / +0x34 / +0x38) in the comments above but NOT static_asserted: the
    // single pointer mpRows widens from 4 to 8 bytes on the 64-bit compile host, so
    // the two trailing floats shift +4 there (the same pointer-widening caveat the
    // committed ICEData.hpp records for its post-pointer members). The 32-bit
    // layout the bodies encode is unchanged.
    static_assert(offsetof(ICEWidgetMenu, miStyle)          == 0x00, "ICEWidgetMenu::miStyle offset");
    static_assert(offsetof(ICEWidgetMenu, miNumRows)        == 0x04, "ICEWidgetMenu::miNumRows offset");
    static_assert(offsetof(ICEWidgetMenu, miNumColumns)     == 0x08, "ICEWidgetMenu::miNumColumns offset");
    static_assert(offsetof(ICEWidgetMenu, miMaxVisibleRows) == 0x0C, "ICEWidgetMenu::miMaxVisibleRows offset");
    static_assert(offsetof(ICEWidgetMenu, miScrollTop)      == 0x10, "ICEWidgetMenu::miScrollTop offset");
    static_assert(offsetof(ICEWidgetMenu, miSelectedRow)    == 0x14, "ICEWidgetMenu::miSelectedRow offset");
    static_assert(offsetof(ICEWidgetMenu, maSelectBox)      == 0x20, "ICEWidgetMenu::maSelectBox offset");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETMENU_HPP
