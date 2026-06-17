#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 (rw::math::vpu::Vector2)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"               // CgsGraphics::Im2d, Basic2dColouredTexturedVertex
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"         // renderengine::PrimitiveType
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"          // mVectorFont (DrawText)

// CgsDev::Debug2DImmediateRender - the screen-space immediate-mode debug renderer. This is what
// actually draws the on-screen "debug squares" (DrawBox) + lines/text the debug HUD emits each
// frame. It batches CgsGraphics::Basic2dColouredTexturedVertex into a fixed array and flushes them
// through the real immediate-mode renderer (CgsGraphics::Im2d, which the loading screen already
// renders through) via ImRenderer<V>::Render. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Render/CgsDebug2DImmediateRender.h).
//
// X360 box path: Begin (assert mpRenderBuffer + reset the batch + mpRenderBuffer->BeginRendering +
// SetDebugRenderStates) -> DrawBox/DrawLine (append a quad/segment of verts to maIm2dVertsArray) ->
// End (DispatchVertices flushes the batch via mpRenderBuffer->Render, then EndRendering). RGBA is the
// packed u32 colour; Vector2 is the screen-space point.
//
// INCREMENTAL: the X360 layout also carries the text path's members (SafeResourceHandle<Font> mpFont,
// VectorFont mVectorFont, TextRenderer mTextRenderer) ahead of mpRenderBuffer. Those + the DrawText/
// font API are the text follow-on (the squares need only the box path); they are declared but the
// members are deferred, so this models the box-path layout, not the byte-exact X360 offsets.

namespace rw { class IResourceAllocator; }

namespace CgsDev
{
    typedef u32 RGBA;   // packed RGBA8 colour (matches the debug render's RGBA param)

    struct Debug2DImmediateRender
    {
        enum DrawingMode
        {
            E_DRAWING_LINES = 0,
            E_DRAWING_TRIANGLES = 1,
            E_DRAWING_QUADS = 2,
            E_DRAWING_TRISTRIP_SOLID = 3,
            E_DRAWING_TRISTRIP_LINES = 4,
            E_DRAWING_FONT = 5,
            E_DRAWING_COUNT = 6,
        };

        static const s32 KI_VERTEX_BUFFER_SIZE = 1000;

        void Construct(rw::IResourceAllocator* lpAllocator, f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight);
        void Destruct();

        void Begin();
        void End();

        Vector2 GetVirtualScreenSize() const;

        void DrawBox(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);
        void DrawBox(f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight, RGBA lColour);
        void DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour);
        void DrawFrame(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);
        void DrawWirePolygon(const rw::math::vpu::Vector2* lpaPoints, u32 luCount, RGBA lColour);
        void DrawSolidConvexPolygon(const rw::math::vpu::Vector2* lpaPoints, u32 luCount, RGBA lColour);
        void DrawCircle(Vector2 lv2Centre, f32 lfRadius, s32 liSegments, RGBA lColour);

        // Text + bar/value helpers: declared for the full API; bodies are the text/font follow-on.
        void DrawText(const char* lpcText, Vector2 lv2Position, f32 lfScale, RGBA lColour, bool lbCentred);
        void DrawText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour);
        void DrawHorizontalBar(Vector2 lv2Min, Vector2 lv2Max, f32 lfValue, f32 lfMax, RGBA lBackColour, RGBA lBarColour);

        void SetRenderBuffer(CgsGraphics::Im2d* lpRenderBuffer);
        bool HasRenderBuffer() const { return mpRenderBuffer != nullptr; }   // safe to Begin() only once set

    private:
        void DispatchVertices();
        void SetDebugRenderStates();
        void EmitQuad(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour);
        void AddVertex(f32 lfX, f32 lfY, RGBA lColour);

        DrawingMode                              meDrawingMode;
        // The debug VECTOR font - DrawText renders through it (the X360 carries it here alongside the
        // deferred mpFont/mTextRenderer resource-font path; the vector font needs no font resource).
        VectorFont                               mVectorFont;
        CgsGraphics::Im2d*                       mpRenderBuffer;
        f32                                      mfVirtualScreenWidth;
        f32                                      mfVirtualScreenHeight;
        CgsGraphics::Basic2dColouredTexturedVertex maIm2dVertsArray[KI_VERTEX_BUFFER_SIZE];
        s16                                      miIm2dVertsHead;
        // -- deferred text member (see header note): TextRenderer mTextRenderer; --
        renderengine::PrimitiveType              mePrimitiveType;
    };
}
