#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"

namespace CgsDev
{
namespace DebugUI
{
// Default plot state: no value grid yet, the full KI_HISTORY_SIZE window, no colour table and a
// zero (transparent) background until the caller wires them through the setters. The X360 inlines
// this construct at each use site (it only ever stores the vptr + miHistoryCount==360 there; the
// remaining fields are immediately overwritten by the setters).
WidgetLineGraph::WidgetLineGraph()
    : mpaaiValuesArray(nullptr)
    , miFloatCount(0)
    , miHistoryCount(KI_HISTORY_SIZE)
    , miFirstValueIndex(0)
    , mpColoursArray(nullptr)
    , miColoursArraySize(0)
    , mColourBackground(0)
{
}

void WidgetLineGraph::SetValues(f32* lpafValues, s32 liFloatCount)
{
    mpaaiValuesArray = lpafValues;
    miFloatCount     = liFloatCount;
}

void WidgetLineGraph::SetFirstValue(s32 liFirstValueIndex)
{
    miFirstValueIndex = liFirstValueIndex;
}

void WidgetLineGraph::SetColours(RGBA* lpaColours, s32 liColoursArraySize)
{
    mpColoursArray     = lpaColours;
    miColoursArraySize = liColoursArraySize;
}

void WidgetLineGraph::SetCustomBackgroundColour(RGBA lColour)
{
    mColourBackground = lColour;
}

// X360 0x82824558. Plots up to miFloatCount polylines over a miHistoryCount-wide window into
// the background box (lfX,lfY,lfWidth,lfHeight). The value buffer is a flat [KI_MAX_FLOATS]
// [KI_HISTORY_SIZE] float grid; each line reads its history ring backwards from miFirstValueIndex.
void WidgetLineGraph::Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight)
{
    if (mpaaiValuesArray == nullptr || miFloatCount == 0 || miHistoryCount == 0)
        return;

    // Background rect: the asm builds the max corner itself (x+w, y+h) and calls the
    // Vector2-min/Vector2-max DrawBox (colour arrives in r8 after the four float corners).
    const Vector2 lv2BoxMin = { lfX,           lfY,            0.0f, 0.0f };
    const Vector2 lv2BoxMax = { lfX + lfWidth, lfY + lfHeight, 0.0f, 0.0f };
    lpRender->DrawBox(lv2BoxMin, lv2BoxMax, mColourBackground);

    const f32 lfBottom = lfY + lfHeight;
    const f32 lfXStep  = lfWidth / static_cast<f32>(miHistoryCount);

    // Default line colour when the widget has no per-line colour table (palette "bar" colour).
    const RGBA lDefaultColour = GetPalette().mColourBar;

    for (s32 liFloatIndex = 0; liFloatIndex < miFloatCount; ++liFloatIndex)
    {
        const s32 liFloatBase = liFloatIndex * KI_HISTORY_SIZE;

        RGBA lColour = lDefaultColour;
        if (mpColoursArray != nullptr)
            lColour = mpColoursArray[liFloatIndex % miColoursArraySize];

        f32 lfPrevY = -1.0f;   // sentinel: no previous point plotted yet

        s32 liRemaining   = miHistoryCount;
        s32 liHistoryIndex = (miHistoryCount + miFirstValueIndex - 1) % miHistoryCount;

        do
        {
            --liRemaining;

            CGS_ASSERT(liFloatIndex >= 0, "liFloatIndex >= 0");
            CGS_ASSERT(liFloatIndex < WidgetLineGraph::KI_MAX_FLOATS, "liFloatIndex < WidgetLineGraph::KI_MAX_FLOATS");
            CGS_ASSERT(liHistoryIndex >= 0, "liHistoryIndex >= 0");
            CGS_ASSERT(liHistoryIndex < WidgetLineGraph::KI_HISTORY_SIZE, "liHistoryIndex < WidgetLineGraph::KI_HISTORY_SIZE");

            const f32 lfValue = mpaaiValuesArray[liFloatBase + liHistoryIndex];
            if (lfValue > 0.0f)
            {
                const f32 lfScaledY = lfValue * lfHeight;
                if (lfPrevY > 0.0f && liRemaining > 0)
                {
                    const Vector2 lv2Start = { static_cast<f32>(liRemaining - 1) * lfXStep + lfX, lfBottom - lfPrevY,   0.0f, 0.0f };
                    const Vector2 lv2End   = { static_cast<f32>(liRemaining)     * lfXStep + lfX, lfBottom - lfScaledY, 0.0f, 0.0f };
                    lpRender->DrawLine(lv2Start, lv2End, lColour);
                }
                lfPrevY = lfScaledY;
            }

            liHistoryIndex = (miHistoryCount + liHistoryIndex - 1) % miHistoryCount;
        }
        while (liRemaining != 0);
    }
}
}
}
