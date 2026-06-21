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
//   * Render -- draw every filled row as shadowed on-screen text, top-down. The
//     two screen-coordinate floats are converted to int once; mfBaseY is held
//     constant as the liY-style coordinate while the per-row vertical position
//     starts at mfFirstLineY and drops 18 each row. The row text pointer advances
//     40 bytes per row. Called by ICEController::RenderBars.
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
    void ICEWidgetInfoList::Render(s32 liScreenX)
    {
        // Both screen coordinates are converted to int once up front: mfBaseY is
        // the constant liY-style coordinate, mfFirstLineY is the first row's
        // vertical position (stepped down per row below).
        const s32 liBaseY = (s32)mfBaseY;
        s32       liLineY = (s32)mfFirstLineY;

        // Draw each filled row top-down. The pre-formatted row text is passed as
        // the format string and the per-row vertical position rides along as a
        // trailing vararg -- the rows carry no `%` specifiers, so it is inert in
        // the formatted output and only satisfies the variadic call shape (the
        // same marshaling ICEWidgetLogo::Render uses). The row pointer advances
        // one 40-byte slot per iteration; the line position drops 18 each row.
        for (s32 liRow = 0; liRow < miItemCount; ++liRow)
        {
            ICERender::ScrShadowPrintf(liScreenX, liBaseY, KF_ICE_INFOLIST_LINE_SIZE,
                                       macItemText[liRow], liLineY);

            liLineY -= KI_ICE_INFOLIST_LINE_STEP;
        }
    }
}
