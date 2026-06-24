#include "GameShared/GameClasses/Fonts/Resources/CgsFontResourceType.h"
#include "GameShared/GameClasses/Fonts/CgsFont.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::BaseResourceDescriptors<5>

#include <cstdint>   // uintptr_t

// CgsResource::FontResourceType -- the font handler (id 0x21). A faithful port of the X360 ARTIST
// bodies. The font's glyph table + atlas-page-pointer array are main-memory self-references
// rebased by FixUp/FixDown (ordinary serialised-pointer fixup); the atlas pages themselves are
// imported RwRasters reported through GetImportPointer (one per texture page).

namespace CgsResource
{
    namespace
    {
        // CGSSTRNLOWER: lowercase up to luCount chars in place (ASCII), stopping at NUL. The X360
        // font FixUp lowercases the typeface family name for case-insensitive matching.
        void FontStrNLower(char* lpcString, u32 luCount)
        {
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                const char lcCh = lpcString[luIndex];
                if (lcCh == '\0')
                    break;
                if (lcCh >= 'A' && lcCh <= 'Z')
                    lpcString[luIndex] = static_cast<char>(lcCh - 'A' + 'a');
            }
        }
    }

    FontResourceType::FontResourceType()
        : Type()
    {
    }

    uint32_t FontResourceType::GetTypeID() const
    {
        return E_RESOURCETYPE_FONT;  // 0x21 = 33 (X360 0x82834E50: "return 33")
    }

    ResourceDescriptor FontResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // The X360 GetSerialisedResourceDescriptor (CgsFontResourceType.cpp:73) is folded in the
        // ARTIST image. The font records its own total allocation size (mSizeOfFont), and its
        // glyph table lives inside that one main block (the atlas pages are imports, not
        // sub-allocations), so the main slot is {mSizeOfFont, 16}. NOTE: build-time sizing -- the
        // load path uses each bundle entry's stored descriptor, not this.
        const Font* lpFont = static_cast<const Font*>(lpResource);
        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = lpFont->mSizeOfFont;  // slot0 (main) size
        lpData[1] = 16u;                  // slot0 align
        lpData[2] = 0u;  lpData[3] = 1u;
        lpData[4] = 0u;  lpData[5] = 1u;
        lpData[6] = 0u;  lpData[7] = 1u;
        lpData[8] = 0u;  lpData[9] = 1u;
        return lDescriptor;
    }

    void FontResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // Faithful port of X360 0x82835AF0 (the private FixUp(font, base), base = main resource):
        //   assert(font->muVersionId == 10);            // "Mismatched font versions"
        //   font->mpapTextures  += base;  font->mpaFontCharIds += base;  font->mpaFontChars += base;
        //   font->mpTextureState = 0;
        //   CGSSTRNLOWER(font->macTypefaceFamilyName, 128);
        // The glyph arrays + the page-pointer array are main-memory offsets rebased to absolute;
        // the atlas pages themselves are resolved as imports (see GetImportPointer). The X360
        // ByteSwapFontChar pass is endian conversion and does not apply to little-endian PC data.
        Font* lpFont = static_cast<Font*>(lpResource);
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);

        // (X360 asserts muVersionId == KU_FONT_VERSION_ID here; CgsFontResourceType.cpp:124.)

        lpFont->mpaFontChars   = reinterpret_cast<FontChar*>(reinterpret_cast<uintptr_t>(lpFont->mpaFontChars) + luBase);
        lpFont->mpaFontCharIds = reinterpret_cast<CgsUtf16*>(reinterpret_cast<uintptr_t>(lpFont->mpaFontCharIds) + luBase);
        lpFont->mpapTextures   = reinterpret_cast<renderengine::Texture**>(reinterpret_cast<uintptr_t>(lpFont->mpapTextures) + luBase);
        lpFont->mpTextureState = 0;

        FontStrNLower(lpFont->macTypefaceFamilyName, 128u);
    }

    void FontResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        // Faithful port of X360 0x82835BF8: inverse of FixUp. Un-rebase each resolved atlas-page
        // pointer first (while mpapTextures is still absolute), then the glyph arrays and the
        // page-pointer array itself.
        Font* lpFont = static_cast<Font*>(lpResource);
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);

        for (u32 luPage = 0; luPage < lpFont->muNumTexturePages; ++luPage)
        {
            lpFont->mpapTextures[luPage] =
                reinterpret_cast<renderengine::Texture*>(reinterpret_cast<uintptr_t>(lpFont->mpapTextures[luPage]) - luBase);
        }
        lpFont->mpaFontChars   = reinterpret_cast<FontChar*>(reinterpret_cast<uintptr_t>(lpFont->mpaFontChars) - luBase);
        lpFont->mpaFontCharIds = reinterpret_cast<CgsUtf16*>(reinterpret_cast<uintptr_t>(lpFont->mpaFontCharIds) - luBase);
        lpFont->mpapTextures   = reinterpret_cast<renderengine::Texture**>(reinterpret_cast<uintptr_t>(lpFont->mpapTextures) - luBase);
    }

    uint32_t FontResourceType::GetImportCount(const void* lpResource) const
    {
        // One import per atlas page (faithful to GetImportPointer / FixDown, which both iterate
        // muNumTexturePages). The X360 GetImportCount body (cpp:267) is folded.
        return static_cast<const Font*>(lpResource)->muNumTexturePages;
    }

    void FontResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const
    {
        // Faithful to X360 0x82834E60: import i = the i-th atlas page pointer (mpapTextures[i]),
        // reported with its byte offset within the font.
        const Font* lpFont = static_cast<const Font*>(lpResource);
        if (luIndex >= lpFont->muNumTexturePages)
        {
            if (lpuOffset != 0) *lpuOffset = 0;
            if (lppValue != 0)  *lppValue = 0;
            return;
        }
        if (lppValue != 0)
            *lppValue = lpFont->mpapTextures[luIndex];
        if (lpuOffset != 0)
            *lpuOffset = static_cast<u32>(reinterpret_cast<const u8*>(&lpFont->mpapTextures[luIndex]) - reinterpret_cast<const u8*>(lpFont));
    }
}
