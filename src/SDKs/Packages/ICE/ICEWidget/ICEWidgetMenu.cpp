// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.cpp
//
// ICE::ICEWidgetMenu -- the dev-only ICE camera-take editor's scrollable menu
// widget. Seven functions:
//
//   * Construct        -- seed the row/column/visible counts + selectable flag
//                         from the per-style tables, store the position, allocate
//                         the row buffer from the ICE edit heap, Prepare each row.
//   * ClearMenuContent -- reset every cell to the empty sentinel.
//   * GetWidth         -- background-box width = margins + per-style column widths.
//   * GetNumNonEmptyItems -- count of rows with at least one non-empty cell.
//   * MoveUp / MoveDown   -- step the selection with wrap, using the count above.
//   * Render           -- draw the background box, the selection highlight, the
//                         scrollbar, and each visible cell (text, or an icon for
//                         the "<ICON>" marker).
//
// The geometry is data-driven by a per-style table family keyed on miStyle: row
// count, visible-line count, column count, a selectable flag, and a per-column
// width array. Those tables live in the ICE data segment reached only through the
// style index; they are modelled here as file-local statics keyed by style.
//
// FLAG: there is no source and no layout reference for this widget. The struct
//       layout, the table family, the box colours/sizes, and the cell-draw wiring
//       are all DERIVED from the function bodies. The per-style table CONTENTS
//       (counts, widths, selectable flags) are NOT recoverable -- they are modelled
//       as a single placeholder style row so the bodies compile and exercise the
//       same arithmetic; see KA*_ICE_MENU_*.
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.hpp"   // ICE::ICEWidgetIcon (the "<ICON>" cell)
#include "SDKs/Packages/ICE/ICEMemory.hpp"                 // ICE::ICEMemory::GetMemory (row buffer)
#include "SDKs/Packages/ICE/ICEMath.hpp"                   // ICE::Matrix4 (RenderPoly context slot)
#include "SDKs/Packages/ICE/ICERender.hpp"                 // ICE::ICERender::RenderPoly / ScrPrintf
#include "rw/core/stdc/stdc.h"                             // rw::core::stdc string wrappers

namespace ICE
{
    // The empty-cell sentinel (extern-declared in the header). PLACEHOLDER: the
    // exact characters are not recoverable; its only observable role is "compares
    // equal to a freshly cleared cell". Modelled as the empty string.
    const char* const KPC_ICE_MENU_EMPTY_CELL = "";   // FLAG: placeholder text

    namespace
    {
        // --- per-cell / per-row geometry (shared with ICEWidgetMenuItem) --------
        // Each row is an ICEWidgetMenuItem: KI_ICE_MENU_MAX_ITEMS cells of
        // KI_ICE_MENU_ITEM_TEXT_LENGTH bytes, then the cell count at +0xA0. The cell
        // text address is rowBase + col*40 and the cell count is at rowBase+0xA0.
        const f32 KF_ICE_MENU_ROW_HEIGHT = 20.0f;   // vertical pitch between rows
        const f32 KF_ICE_MENU_MARGIN     = 10.0f;   // left/right + top/bottom margin
        const f32 KF_ICE_MENU_TEXT_PAD   = 5.0f;    // text inset below the box top
        const f32 KF_ICE_MENU_TEXT_SIZE  = 18.0f;   // drawn cell-text size

        // --- box depth / colour lanes (the RenderPoly Z + colour args) ----------
        const f32 KF_ICE_MENU_BG_Z       = 8.0f;    // background-box depth lane
        const f32 KF_ICE_MENU_HILITE_Z   = 7.0f;    // selection / scrollbar-track depth
        const f32 KF_ICE_MENU_THUMB_Z    = 6.0f;    // scrollbar-thumb depth
        const f32 KF_ICE_MENU_BG_ALPHA   = 200.0f;  // background-box alpha
        const f32 KF_ICE_MENU_HILITE_A   = 220.0f;  // selection-highlight alpha
        const f32 KF_ICE_MENU_THUMB_RGB  = 255.0f;  // scrollbar-thumb colour (white)
        const f32 KF_ICE_MENU_THUMB_A    = 100.0f;  // scrollbar-thumb alpha

        // The icon cell marker: a cell whose text is exactly "<ICON>" is drawn as a
        // grey icon box instead of as text.
        const char* const KPC_ICE_MENU_ICON_MARKER = "<ICON>";

        // Diagnostic strings the bodies substitute for out-of-range cell access.
        const char* const KPC_ICE_MENU_BAD_LINE = "<Invalid lineID>";
        const char* const KPC_ICE_MENU_BAD_COL  = "<Invalid colume ID>";
        const char* const KPC_ICE_MENU_TOO_LONG = "<Menu String is too long!>";

        // ====================================================================
        // The per-style table family, keyed on miStyle. These five tables drive
        // every menu's geometry. PLACEHOLDER: the real table CONTENTS are not
        // recoverable, so a single style (index 0) is modelled with self-consistent
        // values. The accessors clamp the style to this one row, preserving the
        // table-lookup arithmetic the bodies perform.
        // FLAG: KI_ICE_MENU_NUM_STYLES and every value below are placeholders.
        // ====================================================================
        const s32 KI_ICE_MENU_NUM_STYLES = 1;

        const s32 KAI_ICE_MENU_NUM_ROWS[KI_ICE_MENU_NUM_STYLES]     = { 4 };
        const s32 KAI_ICE_MENU_NUM_VISIBLE[KI_ICE_MENU_NUM_STYLES]  = { 4 };
        const s32 KAI_ICE_MENU_NUM_COLUMNS[KI_ICE_MENU_NUM_STYLES]  = { 1 };
        const u8  KAB_ICE_MENU_SELECTABLE[KI_ICE_MENU_NUM_STYLES]   = { 1 };

        // Per-style column widths: KI_ICE_MENU_MAX_ITEMS entries per style. GetWidth
        // sums the first miNumColumns of the style's row.
        const f32 KAF_ICE_MENU_COLUMN_WIDTH[KI_ICE_MENU_NUM_STYLES][KI_ICE_MENU_MAX_ITEMS] =
        {
            { 200.0f, 0.0f, 0.0f, 0.0f }
        };

        // Clamp a style index into the modelled table range.
        inline s32 ClampStyle(s32 liStyle)
        {
            if (liStyle < 0)                     { return 0; }
            if (liStyle >= KI_ICE_MENU_NUM_STYLES) { return KI_ICE_MENU_NUM_STYLES - 1; }
            return liStyle;
        }

        // Draw a flat 2D box (left,top)-(right,bottom) at depth lfZ in colour
        // (r,g,b,a), per-channel in [0,255]. Mirrors the ICEWidgetIcon quad-build /
        // RenderPoly convention: the four corners are wound top-left -> top-right ->
        // bottom-right -> bottom-left, RenderPoly reads corner + opposite-corner.
        // The take->screen context the asm leaves to the debug renderer is modelled
        // as an identity Matrix4 (RenderPoly's static box path ignores it).
        void DrawBox(f32 lfLeft, f32 lfTop, f32 lfRight, f32 lfBottom, f32 lfZ,
                     f32 lfR, f32 lfG, f32 lfB, f32 lfA)
        {
            Poly lQuad;
            lQuad.maVertices[0] = { lfLeft,  lfTop,    lfZ, 0.0f };
            lQuad.maVertices[1] = { lfRight, lfTop,    lfZ, 0.0f };
            lQuad.maVertices[2] = { lfRight, lfBottom, lfZ, 0.0f };
            lQuad.maVertices[3] = { lfLeft,  lfBottom, lfZ, 0.0f };

            Vector4 lColour;
            lColour.x = lfR;
            lColour.y = lfG;
            lColour.z = lfB;
            lColour.w = lfA;

            Matrix4 lLocalWorld;
            ICEMath::Identity(&lLocalWorld);

            ICERender::RenderPoly(&lQuad, &lColour, &lColour, &lLocalWorld);
        }
    } // anonymous namespace

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::Construct
    // ------------------------------------------------------------------------
    void ICEWidgetMenu::Construct(ICEMemory* lpMemory, s32 liStyle, f32 lfX, f32 lfY)
    {
        const s32 liClampedStyle = ClampStyle(liStyle);

        // Position + style key.
        mfX     = lfX;
        mfY     = lfY;
        miStyle = liStyle;

        // Seed the counts from the per-style tables (the asm reads the same style
        // row from three parallel tables).
        miNumRows        = KAI_ICE_MENU_NUM_ROWS[liClampedStyle];
        miMaxVisibleRows = KAI_ICE_MENU_NUM_VISIBLE[liClampedStyle];
        miNumColumns     = KAI_ICE_MENU_NUM_COLUMNS[liClampedStyle];

        // Selectable menus start at row 0; non-selectable menus carry -1.
        if (KAB_ICE_MENU_SELECTABLE[liClampedStyle])
        {
            miSelectedRow = 0;
        }
        else
        {
            miSelectedRow = -1;
        }

        // Scroll always starts at the top.
        miScrollTop = 0;

        // Selectable menus seed the highlight rect with the initial box constants
        // (a single 16-byte store in the asm). Left unset for non-selectable menus.
        if (miSelectedRow != -1)
        {
            maSelectBox[0] = 20.0f;
            maSelectBox[1] = 100.0f;
            maSelectBox[2] = 20.0f;
            maSelectBox[3] = 128.0f;
        }

        // Allocate the row buffer from the ICE edit heap (miNumRows entries, one
        // ICEWidgetMenuItem == 164 bytes each), then Prepare each row.
        mpRows = (ICEWidgetMenuItem*)lpMemory->GetMemory(
            (u32)(sizeof(ICEWidgetMenuItem) * miNumRows));
        if (mpRows)
        {
            for (s32 liRow = 0; liRow < miNumRows; ++liRow)
            {
                mpRows[liRow].Prepare(miNumColumns);
            }
        }
    }

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::ClearMenuContent
    // ------------------------------------------------------------------------
    void ICEWidgetMenu::ClearMenuContent()
    {
        // The empty-sentinel text, clamped to a diagnostic when it would not fit a
        // cell (over the 40-byte cell stride). The sentinel is empty in practice,
        // so the clamp never fires; kept faithful to the body.
        const char* lpcFill = KPC_ICE_MENU_EMPTY_CELL;
        if (rw::core::stdc::StringLength(KPC_ICE_MENU_EMPTY_CELL) > KI_ICE_MENU_ITEM_TEXT_LENGTH)
        {
            lpcFill = KPC_ICE_MENU_TOO_LONG;
        }

        for (s32 liRow = 0; liRow < miNumRows; ++liRow)
        {
            ICEWidgetMenuItem& lRow = mpRows[liRow];
            for (s32 liCol = 0; liCol < miNumColumns; ++liCol)
            {
                // The body re-guards row < miNumRows and col < this row's cell count
                // each iteration before writing the cell.
                if (liRow < miNumRows && liCol < lRow.miItemCount)
                {
                    rw::core::stdc::StringCopy(lRow.macItemText[liCol], lpcFill);
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::GetWidth
    // ------------------------------------------------------------------------
    f32 ICEWidgetMenu::GetWidth() const
    {
        const s32 liClampedStyle = ClampStyle(miStyle);

        // Left margin + the sum of the per-style column widths + right margin. The
        // asm unrolls the sum by four; de-strength-reduced to the plain loop.
        f32 lfWidth = KF_ICE_MENU_MARGIN;
        for (s32 liCol = 0; liCol < miNumColumns; ++liCol)
        {
            lfWidth += KAF_ICE_MENU_COLUMN_WIDTH[liClampedStyle][liCol];
        }
        return lfWidth + KF_ICE_MENU_MARGIN;
    }

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::GetNumNonEmptyItems
    // ------------------------------------------------------------------------
    s32 ICEWidgetMenu::GetNumNonEmptyItems() const
    {
        // Scan rows from the last down to the first; the first row (highest index)
        // that has any cell whose text differs from the empty sentinel fixes the
        // count as (rowIndex + 1). Returns 0 if every row is entirely empty.
        for (s32 liRow = miNumRows - 1; liRow >= 0; --liRow)
        {
            const ICEWidgetMenuItem& lRow = mpRows[liRow];
            for (s32 liCol = 0; liCol < miNumColumns; ++liCol)
            {
                // Resolve the cell text, substituting a diagnostic for out-of-range
                // line / column (kept faithful to the body's range guards).
                const char* lpcCell;
                if (liRow >= miNumRows)
                {
                    lpcCell = KPC_ICE_MENU_BAD_LINE;
                }
                else if (liCol >= lRow.miItemCount || liCol < 0)
                {
                    lpcCell = KPC_ICE_MENU_BAD_COL;
                }
                else
                {
                    lpcCell = lRow.macItemText[liCol];
                }

                // A cell differing from the empty sentinel makes this row the last
                // non-empty one.
                if (rw::core::stdc::StringCompare(lpcCell, KPC_ICE_MENU_EMPTY_CELL) != 0)
                {
                    return liRow + 1;
                }
            }
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::MoveUp
    // ------------------------------------------------------------------------
    void ICEWidgetMenu::MoveUp()
    {
        const s32 liSelected = miSelectedRow;
        if (liSelected <= 0)
        {
            // At (or above) the top: wrap to the last non-empty row.
            miSelectedRow = GetNumNonEmptyItems() - 1;
        }
        else
        {
            miSelectedRow = liSelected - 1;
        }
    }

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::MoveDown
    // ------------------------------------------------------------------------
    void ICEWidgetMenu::MoveDown()
    {
        const s32 liSelected   = miSelectedRow;
        const s32 liNumNonEmpty = GetNumNonEmptyItems();

        // Step down, wrapping to the top once past the last non-empty row.
        s32 liNext = liSelected + 1;
        if (liSelected >= liNumNonEmpty - 1)
        {
            liNext = 0;
        }
        miSelectedRow = liNext;
    }

    // ------------------------------------------------------------------------
    // ICEWidgetMenu::Render
    //
    // The box geometry is built with collapsed vector-lane packing (the corner
    // cross product assembled in packed float lanes); de-collapsed here to the same
    // scalar arithmetic via the DrawBox helper. The cell-text draw mirrors the
    // ICEWidgetLogo / ICEWidgetInfoList convention: the cell string is the displayed
    // text and the screen coordinates ride as inert trailing varargs.
    // ------------------------------------------------------------------------
    void ICEWidgetMenu::Render(s32 liRenderContext)
    {
        (void)liRenderContext;   // editor-render context; the static draw path ignores it

        const s32 liNumNonEmpty = GetNumNonEmptyItems();
        if (liNumNonEmpty == 0)
        {
            return;   // nothing to draw
        }

        // The number of rows actually shown: the lesser of the non-empty count and
        // the scroll window height.
        s32 liVisible = liNumNonEmpty;
        if (miMaxVisibleRows <= liNumNonEmpty)
        {
            liVisible = miMaxVisibleRows;
        }

        const f32 lfWidth = GetWidth();

        // --- background box ---------------------------------------------------
        // From (mfX, mfY), spanning the menu width and (liVisible rows * pitch) plus
        // the top/bottom margin. Semi-transparent fill (RGB 0, alpha 200).
        {
            const f32 lfLeft   = mfX;
            const f32 lfTop    = mfY;
            const f32 lfRight  = mfX + lfWidth;
            const f32 lfBottom = mfY + ((f32)liVisible * KF_ICE_MENU_ROW_HEIGHT) + KF_ICE_MENU_MARGIN;
            DrawBox(lfLeft, lfTop, lfRight, lfBottom, KF_ICE_MENU_BG_Z,
                    0.0f, 0.0f, 0.0f, KF_ICE_MENU_BG_ALPHA);
        }

        // --- selection highlight + scrollbar ----------------------------------
        const s32 liSelected = miSelectedRow;
        if (liSelected >= 0)
        {
            // Keep the selected row inside the scroll window.
            const s32 liTop = miScrollTop;
            if (liSelected < liTop + liVisible)
            {
                if (liSelected < liTop)
                {
                    miScrollTop = liSelected;
                }
            }
            else
            {
                miScrollTop = liSelected - liVisible + 1;
            }

            // Highlight box behind the selected row: one row tall, inset below the
            // box top by KF_ICE_MENU_TEXT_PAD. Uses the selectable-menu highlight
            // alpha (220) over the menu's column rect.
            {
                const s32 liRowOnScreen = liSelected - miScrollTop;
                const f32 lfRowTop    = mfY + KF_ICE_MENU_TEXT_PAD
                                      + ((f32)liRowOnScreen * KF_ICE_MENU_ROW_HEIGHT);
                const f32 lfRowBottom = lfRowTop + KF_ICE_MENU_ROW_HEIGHT;
                const f32 lfLeft      = mfX;
                const f32 lfRight     = mfX + lfWidth;
                DrawBox(lfLeft, lfRowTop, lfRight, lfRowBottom, KF_ICE_MENU_HILITE_Z,
                        0.0f, 0.0f, 0.0f, KF_ICE_MENU_HILITE_A);
            }

            // Scrollbar: drawn only when there are more non-empty rows than fit in
            // the window. A thumb whose vertical span is proportional to the scroll
            // window over the total non-empty rows, drawn white at alpha 100.
            if (liVisible < liNumNonEmpty)
            {
                const f32 lfTrackTop    = mfY + KF_ICE_MENU_TEXT_PAD;
                const f32 lfTrackHeight = (f32)liVisible * KF_ICE_MENU_ROW_HEIGHT;
                const f32 lfThumbHeight = ((f32)miScrollTop / (f32)liNumNonEmpty) * lfTrackHeight;
                const f32 lfThumbTop    = lfThumbHeight + lfTrackTop;
                const f32 lfBarRight    = mfX + lfWidth + 4.0f;
                const f32 lfBarLeft     = lfBarRight - 2.0f;
                DrawBox(lfBarLeft, lfThumbTop, lfBarRight, lfTrackTop + lfTrackHeight,
                        KF_ICE_MENU_THUMB_Z,
                        KF_ICE_MENU_THUMB_RGB, KF_ICE_MENU_THUMB_RGB, KF_ICE_MENU_THUMB_RGB,
                        KF_ICE_MENU_THUMB_A);
            }
        }

        // --- cell grid --------------------------------------------------------
        // Walk columns left-to-right; for each column walk the visible rows. The
        // running X starts at the menu's left + margin and advances by the style's
        // column width per column. Each visible row's Y is its on-screen offset
        // times the row pitch, below the top text pad.
        const s32 liClampedStyle = ClampStyle(miStyle);
        f32 lfColumnX = mfX + KF_ICE_MENU_MARGIN;
        for (s32 liCol = 0; liCol < miNumColumns; ++liCol)
        {
            const s32 liWindowEnd = (miScrollTop + liVisible >= liNumNonEmpty)
                                  ? liNumNonEmpty
                                  : (miScrollTop + liVisible);

            for (s32 liRow = miScrollTop; liRow < liWindowEnd; ++liRow)
            {
                const s32 liRowOnScreen = liRow - miScrollTop;
                const f32 lfRowY = ((f32)liRowOnScreen * KF_ICE_MENU_ROW_HEIGHT)
                                 + mfY + KF_ICE_MENU_TEXT_PAD;

                // Resolve the cell text, substituting a diagnostic for out-of-range
                // line / column (the body's range guards).
                const char* lpcCell;
                if (liRow >= miNumRows || liRow < 0)
                {
                    lpcCell = KPC_ICE_MENU_BAD_LINE;
                }
                else
                {
                    const ICEWidgetMenuItem& lRow = mpRows[liRow];
                    if (liCol >= lRow.miItemCount || liCol < 0)
                    {
                        lpcCell = KPC_ICE_MENU_BAD_COL;
                    }
                    else
                    {
                        lpcCell = lRow.macItemText[liCol];
                    }
                }

                if (rw::core::stdc::StringCompare(lpcCell, KPC_ICE_MENU_ICON_MARKER) != 0)
                {
                    // A normal cell: draw the cell text. The cell string is the
                    // displayed text; the screen coordinates ride as inert trailing
                    // varargs (the ICEWidgetLogo / ICEWidgetInfoList convention).
                    ICERender::ScrPrintf((s32)lfColumnX, (s32)lfRowY, KF_ICE_MENU_TEXT_SIZE,
                                         lpcCell, (s32)lfColumnX, (s32)lfRowY);
                }
                else
                {
                    // An "<ICON>" cell: draw a small icon box at the cell position.
                    ICEWidgetIcon lIcon;
                    lIcon.mfX      = lfColumnX;
                    lIcon.mfY      = lfRowY;
                    lIcon.mfWidth  = KF_ICE_MENU_TEXT_SIZE;
                    lIcon.mfHeight = KF_ICE_MENU_TEXT_SIZE;
                    lIcon.Render((ICERender*)0);
                }
            }

            // Advance the running X by this column's width.
            lfColumnX += KAF_ICE_MENU_COLUMN_WIDTH[liClampedStyle][liCol];
        }
    }
}
