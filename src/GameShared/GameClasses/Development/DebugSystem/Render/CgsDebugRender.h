#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                              // VariableEventQueue<16384,16> (the event buffer)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // Debug2DImmediateRender, RGBA, Vector2

// CgsDev::DebugRender - the BUFFERED debug renderer (the DebugManager's mBufferedRenderer). Debug draws
// are QUEUED here as byte-image events in a VariableEventQueue and replayed once per frame by Dispatch2D
// into the immediate-mode Debug2DImmediateRender. DebugManager::RenderHUD flushes it (Dispatch2D) before
// the per-component HUD pass. Recovered from the X360 ARTIST build (Draw2DText 0x8282B1D0 / Draw2DBox
// 0x8282B2D8 / Dispatch2D 0x8282A4B8): each Draw2DX queues a CInEventDrawX2D under a type ID; Dispatch2D
// walks the queue (GetFirstEvent/GetNextEvent) and replays each into the renderer, then clears.
//
// INCREMENTAL: the 2D path (text/line/box - what the frame-stats overlay needs) is reconstructed; the
// X360 also owns a second (3D) queue + Dispatch3D + richer 2D prims (frame/circle/poly/text-with-bg),
// which are the follow-on. Modelled with the single 2D queue (semantic parity, not the byte-exact 2x
// queue layout).

namespace CgsDev
{
    namespace Internal
    {
        // 2D event type IDs (the queue type tag = the Dispatch2D switch case). A text event (TEXT) is
        // preceded by a STRING event carrying the characters.
        enum InEvent2DType
        {
            E_INEVENT_2D_STRING = 0,
            E_INEVENT_2D_TEXT   = 1,
            E_INEVENT_2D_LINE   = 2,
            E_INEVENT_2D_BOX    = 3,
        };

        // Queued 2D events, stored by byte image (POD; cast to CgsModule::Event* for the queue API).
        // Field order mirrors the X360 (Dispatch2D reads them as a float array).
        struct CInEventDrawText2D   // ID 1 (+ a preceding STRING event with the text)
        {
            f32  mfX;
            f32  mfY;
            f32  mfScale;
            RGBA mColour;
        };
        struct CInEventDrawLine2D   // ID 2
        {
            f32  mfX0;
            f32  mfY0;
            f32  mfX1;
            f32  mfY1;
            RGBA mColour;
        };
        struct CInEventDrawBox2D    // ID 3
        {
            f32  mfMinX;
            f32  mfMinY;
            f32  mfMaxX;
            f32  mfMaxY;
            RGBA mColour;
        };
    }

    class DebugRender
    {
    public:
        void Construct();
        void Clear();

        // Queue a 2D primitive (replayed by Dispatch2D), screen-space in the Im2d logical coords.
        // X360-attested Vector2 overloads (DecFIGS DWARF CgsDebugRender.h:93/96): the debug overlay
        // code (ICERender) passes the screen rect/position as Vector2 values and a packed RGBA.
        void Draw2DText(const char* lpcText, Vector2 lv2Position, f32 lfScale, RGBA lColour);
        void Draw2DBox(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);

        void Draw2DText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour);
        void Draw2DLine(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour);
        void Draw2DBox(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour);

        // Queue a 3D (WORLD-space) box: an axis-aligned box at `lpv3Centre` extending
        // from `lv4MinCorner` to `lv4MaxCorner` (corner offsets) in the space of the
        // passed world transform.
        // X360-attested (CgsDev::DebugRender::DrawBox, progress/tu_index.json):
        // ICEWidgetTargetBox::Render @0x8252D2B8 passes the widget's world transform base
        // as the `float*` (r4 = this+0x10, the caller's 4-row matrix), the packed colour
        // (r5), and the two corner vectors in the SIMD arg registers (v1 = min corner
        // {-0.05,-0.05,-0.05,0}, v2 = max corner {+0.05,+0.05,+0.05,0}). DECLARATION-ONLY:
        // the body queues a CInEventDrawBox into the 3D event queue and is the Debug3D
        // render follow-on (this render currently models only the 2D queue, so no 3D body
        // is defined here).
        // FLAG (header grow): DrawBox (3D) added here for ICEWidgetTargetBox::Render; the
        //       symbol is X360-attested but has no DWARF here, so the arg shape (transform
        //       float* + RGBA + two Vector4 corners) is asm-derived from the call.
        void DrawBox(const f32* lpTransform, RGBA lColour, Vector4 lv4MinCorner, Vector4 lv4MaxCorner);

        // X360 Dispatch2D: replay the queued 2D events into lpRenderer; clear the queue if lbClear.
        void Dispatch2D(Debug2DImmediateRender* lpRenderer, bool lbClear);

    private:
        // X360 owns a 2D + a 3D queue (2D at +16400); only the 2D queue is modelled (3D path deferred).
        CgsModule::VariableEventQueue<16384, 16> m2DQueue;
    };
}
