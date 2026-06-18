#ifndef CGS_FONT_RENDERER_H
#define CGS_FONT_RENDERER_H

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsFont.h"   // CgsResource::Font, CgsUtf8
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"  // Im2dVertex
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // Im2dRenderBuffer (RenderStart/SetState/RenderEnd)

namespace renderengine { class TextureState; }   // RenderBufferSetTextureState binds it

// CgsGraphics::TextObject + TextRenderer -- the resource-font (bitmap) text path. A TextObject
// describes one piece of text (which Font, colour, screen rect, alignment, the multiline / italic /
// background / border / gradient / drop-shadow / emboss options); TextRenderer turns it into Im2d
// quads. This is the path CgsDev::Debug2DImmediateRender::DrawText takes when a font BUNDLE is
// loaded (the no-bundle fallback is the built-in CgsVectorFont). Layout + bodies recovered from the
// DecFIGS PS3 header (Graphics/Font/CgsFontRenderer.h) and the X360 ARTIST bodies. X360 field offsets
// noted; pointers widen on the x64 target.

// CgsResource::SafeResourceHandle<T> is defined in CgsFont.h (included above).

namespace CgsGraphics
{
    typedef u32 RGBA;   // packed RGBA8 colour

    // BasicColouredTexturedVertex::Vector2U_32 -- a 2-component screen-space vector.
    struct TextVector2
    {
        f32 mX;
        f32 mY;
    };

    // Im2dRenderBuffer / Im3dRenderBuffer come from CgsImRenderBuffer.h (the canonical render buffer).

    // The four corner colours for a text background quad.
    struct TextBackground
    {
        RGBA mTopLeft;
        RGBA mTopRight;
        RGBA mBottomLeft;
        RGBA mBottomRight;
    };

    // CgsGraphics::TextObject -- field order + offsets from X360 TextObject::Construct (0x827EEE70).
    struct TextObject
    {
        enum ETextAlignment
        {
            E_ALIGNMENT_START  = 0,
            E_ALIGNMENT_LEFT   = 1,
            E_ALIGNMENT_CENTER = 2,
            E_ALIGNMENT_RIGHT  = 3,
            E_ALIGNMENT_COUNT  = 4
        };

        CgsResource::SafeResourceHandle<CgsResource::Font> mpFont;  // X360 +0x00 (8 bytes)
        f32           mfFontHeight;             // +0x08
        f32           mfAutosizedFontHeight;    // +0x0C
        f32*          mpfCurrentFontHeight;     // +0x10 (-> mfFontHeight by default)
        RGBA          mTextColour;              // +0x14
        TextVector2   mv2TopLeft;               // +0x18
        TextVector2   mv2BottomRight;           // +0x20
        f32           mfCharSpacingMultiplier;  // +0x28
        s32           meAlignment;              // +0x2C (ETextAlignment)
        s32           mbMultiLine;              // +0x30
        s32           mbWordWrap;               // +0x34
        s32           mbItalic;                 // +0x38
        s32           mbBackground;             // +0x3C
        s32           mbBorder;                 // +0x40
        bool          mbGradient;               // +0x44
        bool          mbDropShadow;             // +0x45
        bool          mbEmbossed;               // +0x46
        TextBackground mBackgroundColours;      // +0x48 (16 bytes)
        RGBA          mDropShadowColour;        // +0x58
        RGBA          mBorderColour;            // +0x5C
        RGBA          mGradientColour1;         // +0x60
        RGBA          mGradientColour2;         // +0x64
        const RGBA*   mpAlternateTextColours;   // +0x68 (Construct arg)
        s32           miNumAlternateColours;    // +0x6C (Construct arg)
        f32           mfStringWidth;            // +0x70 (-1 = not yet measured)
        const CgsResource::CgsUtf8* mpUtf8String; // +0x74
        bool          mbAutosize;               // +0x78

        // X360 TextObject::Construct (0x827EEE70): set the defaults (alignment LEFT, colours white,
        // char-spacing 1.0, string-width -1, font = the default handle); alt-colour args stored.
        void Construct(const RGBA* lpAlternateColours, s32 liNumAlternateColours);

        // Autosize the font height to fit mv2TopLeft..mv2BottomRight (X360 0x827EEF58). Body deferred.
        void CalculateAutosizing();
    };

    // CgsGraphics::TextRenderer -- batches a TextObject's glyphs into Im2d vertices and submits them.
    // Layout from the DecFIGS PS3 header (CgsFontRenderer.h); the temp-vertex scratch is sized to
    // KU_MAX_VERTICES. One slot ([1]) per EImRenderingType.
    struct TextRenderer
    {
        enum EImRenderingType
        {
            EImRenderingType_Buffered = 0,
            EImRenderingType_Num      = 1
        };

        static const u32 KU_MAX_VERTICES = 1536;

        typedef CgsGraphics::Basic2dColouredTexturedVertex Im2dVertex;

        // Render one text object's string into the given Im2d buffer. X360 0x82801998: stashes the
        // buffer, runs RenderStringInternal (Buffered), clears the buffer pointer.
        void RenderString(Im2dRenderBuffer* lpRenderBuffer, const TextObject& lrTextObject);

    private:
        // The glyph-quad builder (X360 0x827FF670). Translated from the PPC asm (the Hex-Rays output
        // is VPU-garbled); the body is written in passes.
        void RenderStringInternal(const TextObject& lrTextObject, EImRenderingType leType);

        // Vertex-buffer helpers the glyph emit drives (X360 TextRenderer privates). RenderBufferRenderStart
        // reserves luVertexCount verts (returns the write pointer), RenderBufferRenderEnd submits them as
        // a primitive, RenderBufferSetTextureState binds the font atlas. Bodies are a follow-on pass.
        Im2dVertex* RenderBufferRenderStart(u32 luVertexCount, EImRenderingType leType);
        void        RenderBufferRenderEnd(u32 luPrimitiveType, const Im2dVertex* lpVertices, u32 luVertexCount, EImRenderingType leType);
        void        RenderBufferSetTextureState(const renderengine::TextureState* lpTextureState, EImRenderingType leType);

        Im2dRenderBuffer* mpIm2dRenderBuffer;                    // X360 +0x00
        Im3dRenderBuffer* mpIm3dRenderBuffer;                    // X360 +0x04
        u32               mauVertexCount[1];                     // X360 +0x08
        Im2dVertex*       mapaVertices[1];                       // X360 +0x0C
        bool              mabTempVerticesInUse[1];               // X360 +0x10 (+pad)
        Im2dVertex        maaTempVertices[1][KU_MAX_VERTICES];   // X360 +0x14
    };
}

#endif
