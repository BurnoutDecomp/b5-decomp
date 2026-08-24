#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 (rw::math::vpu::Vector2)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"               // CgsGraphics::Im2d, Basic2dColouredTexturedVertex
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"         // renderengine::PrimitiveType
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"          // mVectorFont (DrawText)
#include "GameShared/GameClasses/Fonts/CgsFont.h"                                 // SafeResourceHandle<Font> (SetDebugFont)
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"                 // CgsGraphics::TextRenderer (resource-font DrawText)

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

namespace rw { struct IResourceAllocator; }   // struct -- must match rwcore_structs.h's class-key (MSVC mangling)

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

        // True if the screen-space point lies within the virtual-screen rect
        // [0,mfVirtualScreenWidth] x [0,mfVirtualScreenHeight] (inclusive). Used by the
        // line/text cull (Is2DLineOnScreen / DrawText). X360 @ 0x82818838.
        bool Is2DPointOnScreen(Vector2 lv2Point) const;

        void DrawBox(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);
        void DrawBox(f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight, RGBA lColour);
        void DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour);
        void DrawFrame(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour);
        // 6-arg frame overload (X360 @0x8281C960): draw a bordered frame around the rect
        // (x0,y0)-(x1,y1) with the given border thickness (four DrawBox strips; bodied in
        // this TU). Used by ErrorWindow::Render + the GPU perfmon graph frame.
        void DrawFrame(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour, f32 lfBorderSize);
        // Wrapped, size + aligned text drawn into the rect (x0,y0)-(x1,y1). Declared-only;
        // body is the text follow-on (used by ErrorWindow::Render). lfAlign in [0,1] centres.
        void DrawTextInBox(const char* lpcText, f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1,
                           f32 lfSize, RGBA lColour, f32 lfAlign);
        void DrawWirePolygon(const rw::math::vpu::Vector2* lpaPoints, u32 luCount, RGBA lColour);
        void DrawSolidConvexPolygon(const rw::math::vpu::Vector2* lpaPoints, u32 luCount, RGBA lColour);
        void DrawCircle(Vector2 lv2Centre, f32 lfRadius, s32 liSegments, RGBA lColour);

        // Text + bar/value helpers: declared for the full API; bodies are the text/font follow-on.
        void DrawText(const char* lpcText, Vector2 lv2Position, f32 lfScale, RGBA lColour, bool lbCentred);
        void DrawText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour);
        void DrawHorizontalBar(Vector2 lv2Min, Vector2 lv2Max, f32 lfValue, f32 lfMax, RGBA lBackColour, RGBA lBarColour);

        // X360 @0x82824098: sprintf the integer value as "%d" and draw it as text at (x,y). Used by the
        // debug HUDs to print a labelled numeric read-out next to a name. Body lives in this TU.
        void DrawValue(u32 luValue, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour);

        // Width (in virtual-screen units) the given text would occupy at the given scale; used by
        // label/value layout code to position a value just after its label. Body is the text/font
        // follow-on (lives in this TU).
        f32 CalcTextWidth(const char* lpcText, f32 lfScale) const;

        void SetRenderBuffer(CgsGraphics::Im2d* lpRenderBuffer);
        bool HasRenderBuffer() const { return mpRenderBuffer != nullptr; }   // safe to Begin() only once set

        // The debug-font handoff (X360 0x823B13A8): store the loaded bitmap Font's handle so DrawText
        // switches off the vector-font fallback onto the resource-font path. Driven by
        // DebugManager::SetDebugFont, which the game calls once the "Default.font" bundle resolves
        // (see GamePrepare). HasResourceFont mirrors DrawText's "mpFont != NULLResourceHandle" test.
        void SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont);
        bool HasResourceFont() const { return !mpFont.IsNull(); }

    private:
        void DispatchVertices();
        void SetDebugRenderStates();
        void EmitQuad(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour);
        void AddVertex(f32 lfX, f32 lfY, RGBA lColour);

        DrawingMode                              meDrawingMode;
        // The loaded bitmap-font handle (X360 +0x04/+0x08). Null = no bundle loaded -> DrawText uses
        // the vector font; non-null (set via SetDebugFont) -> the resource-font TextRenderer path.
        CgsResource::SafeResourceHandle<CgsResource::Font> mpFont;
        // The debug VECTOR font - DrawText's fallback when mpFont is null (the vector font needs no
        // font resource). The X360 carries both the resource-font handle (above) and this here.
        VectorFont                               mVectorFont;
        CgsGraphics::Im2d*                       mpRenderBuffer;
        f32                                      mfVirtualScreenWidth;
        f32                                      mfVirtualScreenHeight;
        CgsGraphics::Basic2dColouredTexturedVertex maIm2dVertsArray[KI_VERTEX_BUFFER_SIZE];
        s16                                      miIm2dVertsHead;
        // The bitmap-font text renderer (X360 carries this inline). DrawText drives it when a font
        // bundle is loaded (mpFont set); it batches glyph quads into mpRenderBuffer.
        CgsGraphics::TextRenderer                mTextRenderer;
        renderengine::PrimitiveType              mePrimitiveType;
    };
}
