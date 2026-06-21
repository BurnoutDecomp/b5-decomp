// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetMenuItem.cpp
//
// ICE::ICEWidgetMenuItem::Prepare -- the dev-only ICE camera-take editor menu
// widget's setup. It fixes the number of active entries (clamped into [0,4]) and
// seeds each active entry's text buffer with a default label, then leaves the
// widget ready for ICEWidgetMenu to populate the real entry strings.
//
// RECONSTRUCTION:
//   * CLAMP -- the requested count is clamped to [0,4]: a negative request yields
//     0 active entries, a request above 4 (the buffer capacity) is capped at 4.
//     The clamped value is stored as miItemCount (+0xA0).
//   * SEED  -- for each of the first miItemCount entries, the default label is
//     copied into that entry's text buffer (macItemText[i]); the write cursor
//     advances one 40-byte entry per iteration. The loop re-reads miItemCount as
//     its bound (it was just stored).
//   * StringCopy returns its destination pointer (strcpy semantics); that leftover
//     return is an incidental artifact of the wrapper, not a logical result, so
//     Prepare returns void (a lifecycle setter).
//
// FLAG: the default label text is an unrecoverable read-only constant -- its exact
//       characters are not recoverable, so it is modelled as a file-local
//       placeholder (KPC_ICE_MENU_DEFAULT_ITEM_TEXT) and seeded into each active
//       entry. Replace with the real label if it is recovered.
// FLAG: Prepare's void return type is DERIVED -- a leftover pointer arises only
//       because the string-copy wrapper returns its destination; no caller is
//       known to consume it.
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenuItem.hpp"
#include "rw/core/stdc/stdc.h"   // rw::core::stdc::StringCopy (strcpy wrapper)

namespace ICE
{
    // The default entry label seeded into each active menu entry. PLACEHOLDER: the
    // default string's exact text is not recoverable.
    static const char* const KPC_ICE_MENU_DEFAULT_ITEM_TEXT = "";   // FLAG: placeholder text

    // ------------------------------------------------------------------------
    // ICEWidgetMenuItem::Prepare
    // ------------------------------------------------------------------------
    void ICEWidgetMenuItem::Prepare(s32 liItemCount)
    {
        // Clamp the requested count into the buffer capacity [0,4].
        s32 liClampedCount = liItemCount;
        if (liClampedCount < 0)
        {
            liClampedCount = 0;
        }
        else if (liClampedCount > KI_ICE_MENU_MAX_ITEMS)
        {
            liClampedCount = KI_ICE_MENU_MAX_ITEMS;
        }

        miItemCount = liClampedCount;

        // Seed each active entry's text buffer with the default label.
        for (s32 liIndex = 0; liIndex < miItemCount; ++liIndex)
        {
            rw::core::stdc::StringCopy(macItemText[liIndex], KPC_ICE_MENU_DEFAULT_ITEM_TEXT);
        }
    }
}
