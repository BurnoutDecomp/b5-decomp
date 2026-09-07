#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                              // VariableEventQueue<16384,16> (the event buffer)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderCommon.h"        // CgsDev::Internal::CInEventDraw* records + RGBA (via CgsTypes.h)

// CgsDev::DebugRender - the BUFFERED debug renderer (the DebugManager's mBufferedRenderer, X360
// DebugManager+0x14C). Debug draws are QUEUED here as byte-image events in a VariableEventQueue and
// replayed once per frame by Dispatch2D/Dispatch3D into the immediate renderers. Recovered from
// the X360 ARTIST publishers and complete replay switches at 0x8282A4B8/0x8282A6F0.
//
// LAYOUT (X360 DebugManager::Construct @0x828332C0 + the ctor @0x82822370): the object is exactly
// TWO VariableEventQueue<16384,16> back to back - the 3D (world-space) queue at +0, the 2D queue at
// +0x4010 (DebugManager reaches them at +0x14C and +0x415C). Both queues are real members now; the
// 3D queue at +0 is replayed during DebugManager::RenderWorld; 2D at +0x4010 during RenderHUD.
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
            E_INEVENT_2D_FRAME  = 4,
        };

        enum InEvent3DType
        {
            E_INEVENT_3D_STRING       = 0,
            E_INEVENT_3D_TEXT         = 5,
            E_INEVENT_3D_LINE         = 6,
            E_INEVENT_3D_POINT        = 7,
            E_INEVENT_3D_QUAD         = 8,
            E_INEVENT_3D_SOLID_QUAD   = 9,
            E_INEVENT_3D_ANGLE        = 10,
            E_INEVENT_3D_AXIS         = 11,
            E_INEVENT_3D_SPHERE       = 12,
            E_INEVENT_3D_SOLID_SPHERE = 13,
            E_INEVENT_3D_HOLLOW_SPHERE = 14,
            E_INEVENT_3D_CIRCLE       = 15,
            E_INEVENT_3D_BOX          = 16,
            E_INEVENT_3D_BOX_AA       = 17,
            E_INEVENT_3D_SOLID_BOX    = 18,
            E_INEVENT_3D_SOLID_BOX_AA = 19,
            E_INEVENT_3D_ARROW        = 20,
            E_INEVENT_3D_SOLID_ARROW  = 21,
            E_INEVENT_3D_TRIANGLE     = 22,
            E_INEVENT_3D_CYLINDER     = 23,
            E_INEVENT_3D_CAPSULE      = 24,
        };

        // The queued 2D/3D event records (CInEventDrawText2D / CInEventDrawLine2D / CInEventDrawBox2D
        // + the world-space CInEventDraw* family) are homed canonically in CgsDebugRenderCommon.h
        // (DWARF-authoritative layout, sizeof cross-checked against each AddEventSafe instance).
        // #include'd above; consumed here by the DebugRender bodies (CgsDebugRender.cpp).
    }

    class DebugRender
    {
    public:
        // Text justification for Draw2DTextJustified (DecFIGS DWARF CgsDebugRender.h:112).
        enum Justification
        {
            E_JUSTIFY_LEFT   = 0,
            E_JUSTIFY_CENTRE = 1,
            E_JUSTIFY_RIGHT  = 2,
        };

        void Construct();
        void Destruct();
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
        void Draw2DLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour);
        void Draw2DBox(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);
        void Draw2DFrame(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);

        void Draw2DText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour);
        void Draw2DLine(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour);
        void Draw2DBox(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour);
        void Draw2DFrame(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour);

        // Canonical world-space API from the DecFIGS declaration shape. Each method publishes the
        // byte-image record consumed by the ARTIST Dispatch3D switch.
        void DrawText(Vector3 lv3Position, const char* lpcText, f32 lfScale, RGBA lColour);
        void DrawLine(Vector3 lv3From, Vector3 lv3To, RGBA lColour);
        void DrawPoint(Vector3 lv3Position, RGBA lColour);
        void DrawQuad(Vector3 lv3A, Vector3 lv3B, Vector3 lv3C, Vector3 lv3D, RGBA lColour);
        void DrawSolidQuad(Vector3 lv3A, Vector3 lv3B, Vector3 lv3C, Vector3 lv3D, RGBA lColour);
        void DrawAngleDeg(Vector3 lv3Position, f32 lfAngle, RGBA lColour);
        void DrawAngleRad(Vector3 lv3Position, f32 lfAngle, RGBA lColour);
        void DrawAxis(Matrix44Affine lTransform);
        void DrawSphere(Vector3 lv3Centre, f32 lfRadius, RGBA lColour);
        void DrawSolidSphere(Vector3 lv3Centre, f32 lfRadius, RGBA lColour);
        void DrawHollowSphere(Vector3 lv3Centre, f32 lfRadius, RGBA lColour);
        void DrawCircle(Matrix44Affine lTransform, f32 lfRadius, RGBA lColour);
        void DrawCircle(Vector3 lv3Centre, Vector3 lv3Normal, f32 lfRadius, RGBA lColour);
        void DrawBox(Vector3 lv3Min, Vector3 lv3Max, Matrix44Affine lTransform, RGBA lColour);
        void DrawBox(Vector3 lv3Min, Vector3 lv3Max, RGBA lColour);
        void DrawSolidBox(Vector3 lv3Min, Vector3 lv3Max, Matrix44Affine lTransform, RGBA lColour);
        void DrawSolidBox(Vector3 lv3Min, Vector3 lv3Max, RGBA lColour);
        void DrawArrow(Vector3 lv3From, Vector3 lv3To, RGBA lColour);
        void DrawSolidArrow(Vector3 lv3From, Vector3 lv3To, RGBA lColour);
        void DrawCapsule(Vector3 lv3Start, Vector3 lv3End, f32 lfRadius, RGBA lColour);
        void DrawCylinder(Vector3 lv3Start, Vector3 lv3End, f32 lfRadius, RGBA lColour);
        void DrawTriangle(Vector3 lv3A, Vector3 lv3B, Vector3 lv3C, RGBA lColour);
        void DrawWireTriangle(Vector3 lv3A, Vector3 lv3B, Vector3 lv3C, RGBA lColour);
        void DrawCross(Vector3 lv3Position, f32 lfSize, RGBA lColour);

        // Compatibility spellings used by already-reconstructed call sites whose matrix/vector ABI
        // was recovered before the DecFIGS declaration was mounted. They forward to the canonical
        // methods and do not create a second render path.
        void DrawBox(const f32* lpTransform, RGBA lColour, Vector4 lv4MinCorner, Vector4 lv4MaxCorner);
        void DrawLine(RGBA lColour, Vector3 lv3From, Vector3 lv3To);
        void DrawAxis(const f32* lpTransform);
        void DrawSolidQuad(RGBA lColour, Vector3 lv3A, Vector3 lv3B, Vector3 lv3C, Vector3 lv3D);

        // X360 Dispatch2D: replay the queued 2D events into lpRenderer; clear the queue if lbClear.
        void Dispatch2D(Debug2DImmediateRender* lpRenderer, bool lbClear);

        // X360 Dispatch3D: replay the queued WORLD-space events into the 3D renderer; clear the
        // queue if lbClear (DebugManager::RenderWorld brackets this with Begin/End).
        void Dispatch3D(Debug3DImmediateRender* lpRenderer, bool lbClear);

    private:
        // X360 queue pair (Construct @0x828332C0 constructs +0x4010 [2D] then +0 [3D]; the ctor
        // zeroes each queue's flag byte). The 3D queue's dispatch path is the Debug3D follow-on.
        CgsModule::VariableEventQueue<16384, 16> m3DQueue;   // +0x0000 - world-space (3D) events
        CgsModule::VariableEventQueue<16384, 16> m2DQueue;   // +0x4010 - screen-space (2D) events
    };
}
