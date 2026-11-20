#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETINFOLIST_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETINFOLIST_HPP

#include "types.hpp"
#include <cstddef>                                       // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base
#include "SDKs/Packages/ICE/ICERender.hpp"               // ICE::ICERender (Render's parameter)

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp
//
// ICE::ICEWidgetInfoList -- the dev-only ICE camera-take editor's scrolling info
// list widget. It holds a fixed pool of pre-formatted text rows and draws them as
// a vertical column of shadowed on-screen text. ICEController::RefreshInfoList
// fills the rows (one AddFormattedItem call per line); ICEController::RenderBars
// draws them top-down. Built like its siblings as a flat, NON-polymorphic struct
// (see ICEWidgetIcon for the widget pattern), so its first field sits at offset 0
// and the empty ICEWidget base contributes no layout (EBO).
//
// LAYOUT (all offsets PINNED below with static_assert):
//   macItemText[15][40] @0x00   -- the row text pool: 15 slots, 40-byte stride.
//                                  AddFormattedItem formats into `this + count*40`
//                                  (+0x00 step 40); Render walks the same buffers
//                                  (+40 per row). Capacity is 15 because the count
//                                  field begins at +0x258 == 15*40, immediately
//                                  after slot 14 (+0x00..+0x257).
//   miItemCount        @0x258    -- number of filled rows (read/compared and
//                                  incremented in AddFormattedItem; loop bound in
//                                  Render).
//   mfX                @0x25C    -- the list's screen X, converted to int and passed
//                                  to ScrShadowPrintf as X for every row
//                                  (ConstructMenus seeds it, 0x82532120).
//   mfFirstLineY       @0x260    -- the first row's vertical position; converted to
//                                  int and stepped -18 per row (the per-line drop).
//
// FLAG: there is no layout reference for this widget. The member names/types and
//       offsets are DERIVED from the field accesses + draw wiring: ScrShadowPrintf
//       takes the render context in r3, so mfX feeds r4 (X) and the stepped
//       mfFirstLineY feeds r5 (Y) (0x825346A4..0x825346B4). Names follow the project
//       Hungarian convention + the ICEWidgetIcon mfX/mfY precedent.
//
// FLAG: the row capacity is 15 slots (indices 0..14). AddFormattedItem's guard
//       admits index 15 (it skips only when miItemCount > 15), and a write at
//       index 15 would land at +0x258 -- overlapping miItemCount -- a latent
//       off-by-one. The guard is kept faithful (<= 15); see the .cpp note. The
//       list is never filled past 15 rows in practice, so the overlap is never
//       reached.
// ============================================================================

namespace ICE
{
    // Row text pool geometry.
    static const s32 KI_ICE_INFOLIST_MAX_ITEMS        = 15;   // slots @ +0x00..+0x257
    static const s32 KI_ICE_INFOLIST_ITEM_TEXT_LENGTH = 40;   // 40-byte stride / format limit

    struct ICEWidgetInfoList : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Format one row into the next free text slot. `lpcFormat` + the
        // trailing varargs are vsnprintf'd (bounded to the 40-byte slot) and
        // the row count is advanced. OWNED here (body in the .cpp).
        // ------------------------------------------------------------------
        void AddFormattedItem(const char* lpcFormat, ...);

        // ------------------------------------------------------------------
        // Draw every filled row as shadowed on-screen text, top-down, stepping
        // the vertical position down each row, through the editor's render context
        // (ICEController::RenderBars passes controller + 0xF6C, 0x82539EAC). OWNED
        // here (body in the .cpp).
        // ------------------------------------------------------------------
        void Render(ICERender* lpRender);

        // --- layout (offsets proven by the function bodies) ----------------
        char macItemText[KI_ICE_INFOLIST_MAX_ITEMS][KI_ICE_INFOLIST_ITEM_TEXT_LENGTH]; // @0x00
        s32  miItemCount;     // @0x258  number of filled rows
        f32  mfX;             // @0x25C  screen X of every row (seeded by ConstructMenus)
        f32  mfFirstLineY;    // @0x260  first row's vertical position (stepped -18/row)
    };

    // Pin the offsets the bodies touch. The base is empty (EBO), so macItemText
    // starts at offset 0; the count lands exactly after the 15 row slots. Total
    // sizeof is NOT asserted: only fields up to +0x260 are proven, so trailing
    // members (if any) are left unmodelled.
    static_assert(offsetof(ICEWidgetInfoList, macItemText)  == 0x000, "ICEWidgetInfoList::macItemText offset");
    static_assert(offsetof(ICEWidgetInfoList, miItemCount)  == 0x258, "ICEWidgetInfoList::miItemCount offset");
    static_assert(offsetof(ICEWidgetInfoList, mfX)          == 0x25C, "ICEWidgetInfoList::mfX offset");
    static_assert(offsetof(ICEWidgetInfoList, mfFirstLineY) == 0x260, "ICEWidgetInfoList::mfFirstLineY offset");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETINFOLIST_HPP
