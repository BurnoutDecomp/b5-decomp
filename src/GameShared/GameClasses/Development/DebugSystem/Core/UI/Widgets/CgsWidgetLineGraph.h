#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h" // Internal::DebugInternal (Widget base)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"                // Palette / RGBA

// CgsDev::DebugUI::WidgetLineGraph -- a debug-UI widget that plots up to miFloatCount rolling
// history lines (a flat [KI_MAX_FLOATS][KI_HISTORY_SIZE] float grid) over a miHistoryCount-wide
// window into a background box. Recovered from the DecFIGS DWARF (CgsWidgetLineGraph.h) + the
// X360 Render asm (0x82824558). Members / offsets:
//   vptr                @+0x00
//   mpaaiValuesArray    @+0x04  (f32*, flat [KI_MAX_FLOATS][KI_HISTORY_SIZE] grid)
//   miFloatCount        @+0x08  (s32, number of active lines)
//   miHistoryCount      @+0x0C  (s32, window width in samples)
//   miFirstValueIndex   @+0x10  (s32, ring head)
//   mpColoursArray      @+0x14  (RGBA*, optional per-line colour table)
//   miColoursArraySize  @+0x18  (s32, colour table length)
//   mColourBackground   @+0x1C  (RGBA, background box fill)
//
// The Widget base mirrors the committed MenuItem / Window pattern: an empty (vptr-only)
// CgsDev::Internal::DebugInternal-derived base that exposes the debug-UI palette/metrics/
// renderer accessors (declaration-only; bodies live in their own TU).

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Metrics;

        // The debug-widget base (empty, vptr-only) -- mirrors MenuItem/Window: derives from
        // DebugInternal and exposes the palette/metrics/renderer to derived widgets.
        struct Widget : public Internal::DebugInternal
        {
            virtual ~Widget();

        protected:
            const Palette&          GetPalette() const;
            const Metrics&          GetMetrics() const;
            Debug2DImmediateRender* Get2DRenderer() const;
        };

        struct WidgetLineGraph : public Widget
        {
            // X360 CgsWidgetLineGraph.h:55-77. floatBase = liFloatIndex*KI_HISTORY_SIZE, asserted
            // against KI_MAX_FLOATS (1800/360==5) and KI_HISTORY_SIZE (360).
            static const s32 KI_MAX_FLOATS   = 5;
            static const s32 KI_HISTORY_SIZE = 360;

            // Default-constructs an empty plot (miHistoryCount seeded to KI_HISTORY_SIZE, everything
            // else cleared); the caller populates it through the setters below. DWARF-attested API
            // (CgsWidgetLineGraph.h:38-40/87-108) driven by MenuItemVariableLineGraph::Render.
            WidgetLineGraph();

            void SetValues(f32* lpafValues, s32 liFloatCount);
            void SetFirstValue(s32 liFirstValueIndex);
            void SetColours(RGBA* lpaColours, s32 liColoursArraySize);
            void SetCustomBackgroundColour(RGBA lColour);

            // @0x82824558 -- plot the widget into the background box (lfX,lfY,lfWidth,lfHeight).
            void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight);

        private:
            f32*  mpaaiValuesArray;    // +0x04
            s32   miFloatCount;        // +0x08
            s32   miHistoryCount;      // +0x0C
            s32   miFirstValueIndex;   // +0x10
            RGBA* mpColoursArray;      // +0x14
            s32   miColoursArraySize;  // +0x18
            RGBA  mColourBackground;   // +0x1C
        };
    }
}
