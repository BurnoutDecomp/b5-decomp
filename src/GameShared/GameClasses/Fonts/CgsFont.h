#ifndef CGS_FONT_H
#define CGS_FONT_H

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource (mTextureStateResource)

namespace renderengine { class Texture; class TextureState; }
namespace rw { struct IResourceAllocator; }   // Font::CreateTextureState allocator

namespace CgsResource
{
    typedef u16 CgsUtf16;
    typedef u8  CgsUtf8;   // UnicodeBuffer::CgsUtf8 (one byte of a UTF-8 sequence)

    // SafeResourceHandle<T> -- a resource pointer plus a safety/generation id (the X360 handle is 8
    // bytes; the null handle is CgsResource::NULLResourceHandle, which the debug-font handoff asserts
    // against). operator-> yields the resource. Full safe-handle validation is deferred; this models
    // the access the font + debug-text paths need.
    template <class T>
    struct SafeResourceHandle
    {
        T*  mpResource;   // X360 +0
        u32 muSafetyId;   // X360 +4

        T* operator->() const { return mpResource; }
        T* Get() const { return mpResource; }
        bool IsNull() const { return mpResource == 0; }
    };

    // The default glyph used when a character is missing ('-', CgsFont.h:28).
    enum { KUTF16_DEFAULT_CHARACTER = 45 };

    // Basic2dColouredVertex::Vector2 (Vector2Template<float>) -- a 2-float UV / position.
    struct FontVector2
    {
        float mX;
        float mY;
    };

    // CgsResource::FontChar -- one glyph's atlas placement + metrics (DecFIGS CgsFont.h, wiki
    // "Font" page). No pointers, so the layout is identical on X360 and the x64 target (32 bytes).
    struct FontChar
    {
        FontVector2 mTopLeftUV;         // +0x00  glyph top-left in atlas UV
        FontVector2 mDimensionsUV;      // +0x08  glyph size in atlas UV
        FontVector2 mStart;             // +0x10  pen start offset
        float       mfAdvance;          // +0x18  horizontal advance
        u16         mu16TexturePageId;  // +0x1C  which atlas page (index into Font::mpapTextures)
        u8          mbIsLowerCaseScale; // +0x1E
        u8          mbIsRenderable;     // +0x1F  glyphs like space are non-renderable

        bool IsRenderable() const { return mbIsRenderable != 0; }
    };

    // CgsResource::Font -- a loaded bitmap font (DecFIGS CgsFont.h + wiki "Font" page; X360
    // offsets in comments, pointers widen on x64). The glyph table (mpaFontChars/mpaFontCharIds)
    // and page-pointer array (mpapTextures) live in the resource's main allocation and are
    // rebased by Rw...FontResourceType::FixUp; the atlas pages themselves are imported RwRasters
    // (renderengine::Texture); the texture state is created at runtime (mpTextureState/FixUp nulls
    // it). Glyph lookup goes through a 128-bucket hash (mauHashOffsets / mpaFontCharIds).
    struct Font
    {
        enum
        {
            KI_TYPEFACE_MAX_CHARS = 128,
            KU_FONT_VERSION_ID    = 10,   // FixUp asserts muVersionId == this
            KU_HASH_MASK          = 127,
            KU_HASH_TABLE_SIZE    = 129
        };

        u32          muVersionId;                         // X360 +0x00
        u32          mSizeOfFont;                         // +0x04  (size_t in source; 32-bit size)
        FontVector2  mScaleUV;                            // +0x08
        float        mfLowerCaseScale;                    // +0x10
        float        mfBaseLine;                          // +0x14
        float        mfXHeight;                           // +0x18
        u32          muNumChars;                          // +0x1C
        FontChar*    mpaFontChars;                        // +0x20  main-memory; FixUp-rebased
        CgsUtf16*    mpaFontCharIds;                      // +0x24  main-memory; FixUp-rebased
        u16          mauHashOffsets[KU_HASH_TABLE_SIZE];  // +0x28  129 hash-bucket boundaries
        u32          muNumTexturePages;                   // X360 +0x12C  import count
        renderengine::Texture**     mpapTextures;         // +0x130 imported atlas pages (FixUp-rebased)
        renderengine::TextureState* mpTextureState;       // +0x134 runtime; FixUp nulls it
        rw::Resource mTextureStateResource;               // +0x138 backing resource for the texture state
        u32          muFontHeightInPixels;                // +0x148
        char         macTypefaceFamilyName[128];          // +0x14C FixUp lowercases this
        char         macTypefaceStyleName[128];           // +0x1CC

        u32 GetHeightInPixels() const { return muFontHeightInPixels; }

        // Glyph lookup (CgsFont.cpp). FindFontChar binary-searches the hash bucket and may return
        // null; the GetFontChar overloads add the missing-glyph fallback ('-' then the first glyph).
        const FontChar* FindFontChar(CgsUtf16 lu16Char) const;
        const FontChar* GetFontChar(CgsUtf16 lu16Char) const;
        const FontChar* GetFontChar(const CgsUtf8* lpUtf8Char) const;

        // Width of one line of a UTF-8 string in this font (stops at the first newline). Skips
        // Burnout '^N' colour codes; scaled by mScaleUV.x. CgsFont.cpp / X360 0x82834F20.
        float GetStringWidth(const CgsUtf8* lpUtf8String) const;

        // Build the font's runtime texture state from atlas page 0 (mpapTextures[0]) so the text
        // renderer can bind it. Called once after the font bundle loads (see GamePrepare handoff).
        // CgsFont.cpp / X360 0x82835658. [PC: the allocator is unused -- the texture state is backed
        // by mTextureStateResource -- so it is a nullable pointer the bring-up can omit.]
        void CreateTextureState(rw::IResourceAllocator* lpAllocator = 0);

        // Find the [start,end) of the next line of lpString that fits lfMaxWidthInEm (word-wrapping
        // when lbWordWrap), returning the line's width (in em). Used by TextRenderer::RenderStringInternal
        // for per-line layout. CgsFont.cpp / X360. Body is a follow-on (a line-measure like GetStringWidth).
        float GetStringStartAndEnd(const CgsUtf8* lpString, float lfMaxWidthInEm,
                                   const CgsUtf8** lppLineStart, const CgsUtf8** lppLineEnd, bool lbWordWrap) const;
    };
}

#endif
