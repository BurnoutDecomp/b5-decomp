// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.cpp
//
// ICE::ICEWidgetInfoList -- the dev-only ICE camera-take editor's scrolling info
// list widget. Two functions:
//
//   * AddFormattedItem -- append one pre-formatted text row. The next free slot is
//     `this + miItemCount*40` (the 40-byte row stride); the row is vsnprintf'd
//     there bounded to 40 bytes, then the count is advanced. Called by
//     ICEController::RefreshInfoList.
//
//   * Render -- draw every filled row as shadowed on-screen text, top-down,
//     through the editor's render context. The two screen-coordinate floats are
//     converted to int once: every row is drawn at X = mfX, starting at Y =
//     mfFirstLineY and dropping 18 each row. The row text pointer advances 40 bytes
//     per row. Called by ICEController::RenderBars.
//
// FLAG: no layout reference for this widget; the layout/constants (40-byte stride,
//       15-slot capacity, the 16.0 text size, the -18 per-line step, the two float
//       coordinate roles) are DERIVED from the field accesses and the draw wiring.
//       See the header for the field-by-field rationale.
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetInfoList.hpp"
#include "SDKs/Packages/ICE/ICERender.hpp"      // ICE::ICERender::ScrShadowPrintf
#include "rw/core/stdc/stdc.h"                  // rw::core::stdc::Vsnprintf

#include <cstdarg>                              // va_list / va_start / va_end

namespace ICE
{
    // Drawn text size for every row (the per-row ScrShadowPrintf scale).
    static const f32 KF_ICE_INFOLIST_LINE_SIZE = 16.0f;

    // Per-row vertical drop: each row sits 18 units below the previous one.
    static const s32 KI_ICE_INFOLIST_LINE_STEP = 18;

    // ------------------------------------------------------------------------
    // ICEWidgetInfoList::AddFormattedItem
    // ------------------------------------------------------------------------
    void ICEWidgetInfoList::AddFormattedItem(const char* lpcFormat, ...)
    {
        // Guard kept faithful: proceed while the count is <= 15 (skip only when
        // > 15). NOTE the latent off-by-one -- at miItemCount == 15 the write
        // target is `this + 15*40` == +0x258, overlapping miItemCount. The slot
        // address is computed as `this + count*40` (the row stride); the list is
        // never filled past 15 rows in practice, so the overlap is never reached.
        if (miItemCount <= KI_ICE_INFOLIST_MAX_ITEMS)
        {
            // The va_list begins right after lpcFormat (the trailing varargs the
            // caller supplies for the format string). The size-bounded formatter
            // writes into the next free row slot, limited to the 40-byte stride.
            va_list lvaArgs;
            va_start(lvaArgs, lpcFormat);
            rw::core::stdc::Vsnprintf(macItemText[miItemCount],
                                      KI_ICE_INFOLIST_ITEM_TEXT_LENGTH,
                                      lpcFormat, lvaArgs);
            va_end(lvaArgs);

            ++miItemCount;
        }
    }

    // ------------------------------------------------------------------------
    // ICEWidgetInfoList::Render
    // ------------------------------------------------------------------------
    void ICEWidgetInfoList::Render(ICERender* lpRender)
    {
        // Both screen coordinates are converted to int once up front: mfX is the X
        // of every row, mfFirstLineY the first row's Y (stepped down per row below).
        const s32 liX     = (s32)mfX;
        s32       liLineY = (s32)mfFirstLineY;

        // Draw each filled row top-down, its pre-formatted text as the format string
        // (0x825346B8: r3 = the render context, r4 = X, r5 = the row's Y, f1 = the
        // size, r7 = the row text, and nothing more). The row pointer advances one
        // 40-byte slot per iteration; the line position drops 18 each row.
        for (s32 liRow = 0; liRow < miItemCount; ++liRow)
        {
            lpRender->ScrShadowPrintf(liX, liLineY, KF_ICE_INFOLIST_LINE_SIZE, macItemText[liRow]);

            liLineY -= KI_ICE_INFOLIST_LINE_STEP;
        }
    }
}
