#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/MenuItems/CgsMenuItemVariableLineGraph.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsDev::DebugUI::MenuItemVariableLineGraph - the line-graph menu-item bodies. AddFloat binds a
// live float into the next free plot slot; Update samples every bound float into its history ring
// and tracks the running min/max, fully recomputing it on ring wrap; RecalculateMinMax rebuilds
// the min/max from scratch. See the header for the (X360-pinned) member layout.

namespace CgsDev
{
    namespace DebugUI
    {
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
    }
}
