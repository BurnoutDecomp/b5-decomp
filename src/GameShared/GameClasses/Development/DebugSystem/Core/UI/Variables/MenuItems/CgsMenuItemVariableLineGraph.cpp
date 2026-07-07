#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/MenuItems/CgsMenuItemVariableLineGraph.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                             // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.h"  // WidgetLineGraph (stack plot)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"                     // Metrics / Palette / RGBA

// CgsDev::DebugUI::MenuItemVariableLineGraph - the line-graph menu-item bodies. AddFloat binds a
// live float into the next free plot slot; Update samples every bound float into its history ring
// and tracks the running min/max, fully recomputing it on ring wrap; RecalculateMinMax rebuilds
// the min/max from scratch. See the header for the (X360-pinned) member layout.

namespace CgsDev
{
    namespace DebugUI
    {
        // X360 CgsMenuItemVariableLineGraph.cpp:30-31 (rodata @0x82F32E64). The per-line colour cycle
        // Render hands to the stack WidgetLineGraph via SetColours. KN_NUM_COLOURS == 6. The exact
        // packed-RGBA values live in X360 rodata that is not carried in the dossier, so a distinct
        // six-entry debug cycle is reconstructed here (semantics: each of the up-to-five plotted lines
        // gets its own colour, wrapping modulo the table length).
        const u32 KN_NUM_COLOURS = 6;
        RGBA      KA_COLOURS_ARRAY[KN_NUM_COLOURS] =
        {
            0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFF00u, 0xFF00FFFFu, 0xFFFF00FFu,
        };

        // X360 0x82823610. Bind a live float into the first free plot slot: clear that line's
        // history ring, reset the ring head, and fold the new value into the running min/max.
        void MenuItemVariableLineGraph::AddFloat(f32* lpfValue)
        {
            s32 liFloatIndex = 0;
            while (mapfFloats[liFloatIndex])
            {
                ++liFloatIndex;
                if (liFloatIndex >= KI_MAX_FLOATS)
                {
                    CGS_ASSERT(liFloatIndex < KI_MAX_FLOATS, "liFloatIndex < WidgetLineGraph::KI_MAX_FLOATS");
                    break;
                }
            }

            mapfFloats[liFloatIndex] = lpfValue;

            for (s32 liSample = 0; liSample < KI_HISTORY_SIZE; ++liSample)
                maafHistory[liFloatIndex][liSample] = 0.0f;

            miHistoryHead = 0;

            const f32 lfValue = *lpfValue;
            mfMinVal = (mfMinVal - lfValue >= 0.0f) ? lfValue : mfMinVal;
            mfMaxVal = (mfMaxVal - lfValue >= 0.0f) ? mfMaxVal : lfValue;
        }

        // X360 0x82816860. Rebuild the running min/max from scratch by scanning every bound line's
        // full history ring (called once each time the ring wraps).
        void MenuItemVariableLineGraph::RecalculateMinMax()
        {
            mfMinVal = 0.0f;
            mfMaxVal = 0.0f;

            for (s32 liFloat = 0; liFloat < KI_MAX_FLOATS; ++liFloat)
            {
                if (!mapfFloats[liFloat])
                    continue;

                for (s32 liSample = 0; liSample < KI_HISTORY_SIZE; ++liSample)
                {
                    const f32 lfSample = maafHistory[liFloat][liSample];
                    mfMinVal = (mfMinVal - lfSample >= 0.0f) ? lfSample : mfMinVal;
                    mfMaxVal = (mfMaxVal - lfSample >= 0.0f) ? mfMaxVal : lfSample;
                }
            }
        }

        // X360 0x82818008. Sample each bound float into its history ring at the current head, updating
        // the running min/max as it goes; on wrap (head == 0) fully recompute min/max, then advance the
        // ring head modulo KI_HISTORY_SIZE. The lfTimeStep / input event are unused by the graph row.
        void MenuItemVariableLineGraph::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/)
        {
            for (s32 liFloatIndex = 0; liFloatIndex < KI_MAX_FLOATS; ++liFloatIndex)
            {
                const f32* lpfValue = mapfFloats[liFloatIndex];
                if (lpfValue != nullptr)
                {
                    const f32 lfValue = *lpfValue;
                    if (lfValue < mfMinVal)
                        mfMinVal = lfValue;
                    if (lfValue > mfMaxVal)
                        mfMaxVal = lfValue;
                    maafHistory[liFloatIndex][miHistoryHead] = lfValue;
                }
            }

            if (miHistoryHead == 0)
                RecalculateMinMax();

            miHistoryHead = (miHistoryHead + 1) % KI_HISTORY_SIZE;
        }

        // X360 0x828167C0. Reset the row: the base MenuItem reset (width/height + list-link detach),
        // then clear the running min/max, blank every bound line's history ring, drop every bound
        // float and rewind the ring head.
        void MenuItemVariableLineGraph::Prepare()
        {
            MenuItem::Prepare();   // X360 stfs 0->+4/+8 (mfWidth/mfHeight) + stw 0->+0xC (list link)

            mfMinVal = 0.0f;
            mfMaxVal = 0.0f;

            for (s32 liFloat = 0; liFloat < KI_MAX_FLOATS; ++liFloat)
                for (s32 liSample = 0; liSample < KI_HISTORY_SIZE; ++liSample)
                    maafHistory[liFloat][liSample] = 0.0f;

            for (s32 liFloat = 0; liFloat < KI_MAX_FLOATS; ++liFloat)
                mapfFloats[liFloat] = nullptr;

            miHistoryHead = 0;
        }

        // X360 0x82816818. Size the row off the shared debug-UI metrics: a fixed 360-wide plot padded
        // by the sub-item indent on both sides, and a caption-height row scaled from the big-bar factor
        // and the text size plus the menu gap.
        void MenuItemVariableLineGraph::ComputeSize()
        {
            const Metrics& lrMetrics = GetMetrics();

            mfWidth  = lrMetrics.mfSubItemIndent * 2.0f + 360.0f;
            mfHeight = lrMetrics.mfBarHeightScaleBig * lrMetrics.mfTextSize
                     + lrMetrics.mfMenuGap
                     + lrMetrics.mfTextSize;
        }

        // X360 0x82829BD0. Normalise every ring sample into a stack [KI_MAX_FLOATS][KI_HISTORY_SIZE]
        // scratch grid ((value - (min-0.5)) / ((max+0.5) - (min-0.5))), then plot it through a stack
        // WidgetLineGraph inset by the sub-item indent, at a fixed 450px plot height. The lbSelected
        // flag is unused by the graph row; the last float carries the row width.
        void MenuItemVariableLineGraph::Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY,
                                               bool /*lbSelected*/, f32 lfAlpha)
        {
            const Metrics& lrMetrics = GetMetrics();
            const f32 lfIndent    = lrMetrics.mfSubItemIndent;
            const f32 lfPlotX     = lfIndent + lfX;
            const f32 lfPlotWidth = lfAlpha - lfIndent * 2.0f;

            const f32 lfMin   = mfMinVal - 0.5f;
            const f32 lfMax   = mfMaxVal + 0.5f;
            const f32 lfRange = lfMax - lfMin;

            f32 laafNormalised[KI_MAX_FLOATS][KI_HISTORY_SIZE];
            for (s32 liFloatIndex = 0; liFloatIndex < KI_MAX_FLOATS; ++liFloatIndex)
            {
                for (s32 liHistoryIndex = 0; liHistoryIndex < KI_HISTORY_SIZE; ++liHistoryIndex)
                {
                    CGS_ASSERT(liFloatIndex >= 0, "liFloatIndex >= 0");
                    CGS_ASSERT(liFloatIndex < KI_MAX_FLOATS, "liFloatIndex < WidgetLineGraph::KI_MAX_FLOATS");
                    CGS_ASSERT(liHistoryIndex >= 0, "liHistoryIndex >= 0");
                    CGS_ASSERT(liHistoryIndex < KI_HISTORY_SIZE, "liHistoryIndex < WidgetLineGraph::KI_HISTORY_SIZE");

                    laafNormalised[liFloatIndex][liHistoryIndex] =
                        (maafHistory[liFloatIndex][liHistoryIndex] - lfMin) / lfRange;
                }
            }

            WidgetLineGraph lWidget;
            lWidget.SetValues(&laafNormalised[0][0], KI_MAX_FLOATS);
            lWidget.SetFirstValue(miHistoryHead);
            lWidget.SetColours(KA_COLOURS_ARRAY, static_cast<s32>(KN_NUM_COLOURS));
            lWidget.SetCustomBackgroundColour(GetPalette().mColourBarBackground);
            lWidget.Render(lpRender, lfPlotX, lfY, lfPlotWidth, 450.0f);
        }
    }
}
