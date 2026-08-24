#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                              // VariableEventQueue<16384,16> (the event buffer)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderCommon.h"        // CgsDev::Internal::CInEventDraw* records + RGBA (via CgsTypes.h)

// CgsDev::DebugRender - the BUFFERED debug renderer (the DebugManager's mBufferedRenderer, X360
// DebugManager+0x14C). Debug draws are QUEUED here as byte-image events in a VariableEventQueue and
// replayed once per frame by Dispatch2D into the immediate-mode Debug2DImmediateRender.
// DebugManager::RenderHUD flushes it (Dispatch2D) before the per-component HUD pass. Recovered from
// the X360 ARTIST build (Draw2DText 0x8282B1D0 / Draw2DBox 0x8282B2D8 / Dispatch2D 0x8282A4B8): each
// Draw2DX queues a CInEventDrawX2D under a type ID; Dispatch2D walks the queue
// (GetFirstEvent/GetNextEvent) and replays each into the renderer, then clears.
//
// LAYOUT (X360 DebugManager::Construct @0x828332C0 + the ctor @0x82822370): the object is exactly
// TWO VariableEventQueue<16384,16> back to back - the 3D (world-space) queue at +0, the 2D queue at
// +0x4010 (DebugManager reaches them at +0x14C and +0x415C). Both queues are real members now; the
// 3D DISPATCH path (Dispatch3D + the world-space replay) is still the Debug3D render follow-on -
// only the 2D replay is bodied.
//
// This header is pulled by value into CgsDebugManager.h, so it stays light: the immediate renderer
// is forward-declared (Dispatch2D takes it by pointer); CgsDebugRender.cpp includes the real type.

namespace CgsDev
{
    struct Debug2DImmediateRender;   // the immediate 2D renderer Dispatch2D replays into
    struct Debug3DImmediateRender;   // the immediate 3D renderer Dispatch3D replays into

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

        // The queued 2D/3D event records (CInEventDrawText2D / CInEventDrawLine2D / CInEventDrawBox2D
        // + the world-space CInEventDraw* family) are homed canonically in CgsDebugRenderCommon.h
        // (DWARF-authoritative layout, sizeof cross-checked against each AddEventSafe instance).
        // #include'd above; consumed here by the DebugRender bodies (CgsDebugRender.cpp).
    }

    class DebugRender
    {
    public:
        // WorldEntityModule::RenderInstance debug path (@0x822D5AB0 tail): draw a
    // world-space circle (centre, facing, radius, packed colour). Declaration only;
    // the body lands with the DebugRender TU (per-TU compile gate).
    void DrawCircle( Vector3 lCentre, Vector3 lNormal, f32 lfRadius, u32 luColour );
        // Text justification for Draw2DTextJustified (DecFIGS DWARF CgsDebugRender.h:112).
        enum Justification
        {
            E_JUSTIFY_LEFT   = 0,
            E_JUSTIFY_CENTRE = 1,
            E_JUSTIFY_RIGHT  = 2,
        };

        void Construct();
        void Clear();

        // Draw a justified 2D text string: measure the text width at lfSize, shift lv2Position left
        // by 0 / width*0.5 / width for LEFT / CENTRE / RIGHT, then queue it like Draw2DText. X360-
        // attested (CgsDev::DebugRender::Draw2DTextJustified); the DebugPrinter overlay draws each
        // line through this. DECLARATION-ONLY here: the body lives in the DebugRender TU
        // (CgsDebugRender.cpp) and the per-TU /c gate does not link. Signature/justification enum
        // from the DecFIGS DWARF (CgsDebugRender.cpp:672); the DebugPrinter call site (ActualPrint
        // @0x821F71D8) passes text/position/justification/size/colour in exactly this order.
        void Draw2DTextJustified(const char* lpcText, Vector2 lv2Position, Justification leJustification,
                                 f32 lfSize, RGBA lColour);

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

        // FLAG (header grow 2026-08-02): DrawLine (3D) added for
        // BehaviourGameplayExternal::Update's debug-render arm (.cpp:370, X360 @0x82241524).
        // ARGUMENT SHAPE IS ASM-DERIVED and matches DrawSolidQuad's above: the packed colour
        // arrives in the GPR slot (r4) and the two world-space endpoints in v1/v2. No DWARF
        // here; DECLARATION-ONLY, the body is the Debug3D render follow-on.
        void DrawLine(RGBA lColour, Vector3 lv3From, Vector3 lv3To);

        // FLAG (header grow): DrawAxis + DrawSolidQuad added for BehaviourRig::Update.
        // DrawAxis: draws the 3 coordinate axes of a world-space transform (asm @BehaviourRig::Update).
        void DrawAxis(const f32* lpTransform);
        // DrawSolidQuad: draws a world-space solid quad defined by 4 corner points + colour.
        void DrawSolidQuad(RGBA lColour, Vector3 lv3A, Vector3 lv3B, Vector3 lv3C, Vector3 lv3D);

        // X360 Dispatch2D: replay the queued 2D events into lpRenderer; clear the queue if lbClear.
        void Dispatch2D(Debug2DImmediateRender* lpRenderer, bool lbClear);

        // X360 Dispatch3D: replay the queued WORLD-space events into the 3D renderer; clear the
        // queue if lbClear (DebugManager::RenderWorld @0x8282E030 calls it inside the 3D Begin/End
        // bracket). BOUNDED: the replay switch is the Debug3D render follow-on - no 3D events are
        // queued on this build (the 3D Draw* publishers are declaration-only), so the body only
        // honours the clear.
        void Dispatch3D(Debug3DImmediateRender* lpRenderer, bool lbClear);

    private:
        // X360 queue pair (Construct @0x828332C0 constructs +0x4010 [2D] then +0 [3D]; the ctor
        // zeroes each queue's flag byte). The 3D queue's dispatch path is the Debug3D follow-on.
        CgsModule::VariableEventQueue<16384, 16> m3DQueue;   // +0x0000 - world-space (3D) events
        CgsModule::VariableEventQueue<16384, 16> m2DQueue;   // +0x4010 - screen-space (2D) events
    };
}
