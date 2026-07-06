#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"  // MenuItem base + InputEvent

// CgsDev::DebugUI::MenuItemVariableLineGraph - a MenuItem row that plots up to KI_MAX_FLOATS bound
// f32 values as scrolling history lines. Each frame Update() samples the (up to five) bound floats
// into a per-float [KI_HISTORY_SIZE] ring, tracks a running min/max, and Render() normalises the
// rings into a scratch grid and hands them to a stack WidgetLineGraph. Recovered from the DecFIGS
// DWARF (CgsMenuItemVariableLineGraph.h) + the X360 asm. Member offsets (base MenuItem is 0x10:
// vptr@+0x00, mfWidth@+0x04, mfHeight@+0x08, list-link@+0x0C):
//   mapfFloats[5]       @+0x10   (f32*, the bound source floats; null == unused slot)
//   maafHistory[5][360] @+0x24   (f32, per-float scrolling history ring, 0x5A0 bytes/row)
//   miHistoryHead       @+0x1C44 (s32, ring write cursor, wraps mod KI_HISTORY_SIZE)
//   mfMinVal            @+0x1C48 (f32, running minimum across all rings)
//   mfMaxVal            @+0x1C4C (f32, running maximum across all rings)
//
// This slice reconstructs AddFloat / Update / RecalculateMinMax; the remaining virtuals
// (Prepare/Render/ComputeSize) are declared for class shape but bodied by their own reconstructions.

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct MenuItemVariableLineGraph : public MenuItem
        {
            static const s32 KI_MAX_FLOATS   = 5;
            static const s32 KI_HISTORY_SIZE = 360;

            // X360 0x828167C0 - reset all rings, min/max and the ring head.
            virtual void Prepare();
            // X360 0x82818008 - sample the bound floats into the rings, advance the ring head.
            virtual void Update(f32 lfTimeStep, InputEvent leEvent);
            // X360 0x82829BD0 - normalise the rings and plot them through a stack WidgetLineGraph.
            virtual void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha);
            // X360 0x82816818 - size the row from the metrics (fixed 360-wide plot + captions).
            virtual void ComputeSize();

            // X360 0x82823610 - bind a source float into the next free slot and seed its ring/min-max.
            void AddFloat(f32* lpfValue);

        private:
            // X360 0x82816860 - recompute mfMinVal/mfMaxVal by sweeping every ring.
            void RecalculateMinMax();

            f32* mapfFloats[KI_MAX_FLOATS];                     // +0x10
            f32  maafHistory[KI_MAX_FLOATS][KI_HISTORY_SIZE];   // +0x24
            s32  miHistoryHead;                                 // +0x1C44
            f32  mfMinVal;                                      // +0x1C48
            f32  mfMaxVal;                                      // +0x1C4C
        };
    }
}
