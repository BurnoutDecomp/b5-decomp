#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"

#include "GameShared/GameClasses/Fonts/CgsFont.h"                 // CgsResource::Font (typeface name field)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"           // CGSSTRNLOWER

#include <cstring>   // strncpy, strstr

// =============================================================================
// CgsGui::FontCollection::FindFont -- BURNOUT_X360_ARTIST.XEX @ 0x82853168.
//
// Faithful decompile of the X360 lookup. The collection is an array of typed Font
// handles (CgsResource::SafeResourceHandle<Font>); the X360 walks consecutive
// two-word slots and dereferences each non-empty handle to its CgsResource::Font.
// On x64 the slots are wider (two 8-byte pointers) so the walk is expressed over the
// typed handle array maFonts[] by NAME rather than over raw word offsets, preserving
// the semantics (skip empty slots, deref to Font, match by typeface name).
//
// The X360's per-slot "is this the empty/default handle?" test compares both handle
// words against a module-static default-handle sentinel (dword_8305F174/_8305F178).
// An empty SafeResourceHandle has mpResourceMemory == nullptr; testing IsNull() is the
// x64-native equivalent of that sentinel compare and is what skips unused slots.
//
// The matched name lives on the resolved CgsResource::Font: the X360 reads it at the
// handle's deref + 0x150 (sub_827EF870 == SafeResourceHandle::operator-> inlined; the
// + 0x150 is the typeface family-name string). The committed Font models that string as
// macTypefaceFamilyName (DWARF/wiki) at [c:+0x14C]; this ARTIST build reads [c:+0x150]
// (a 4-byte skew vs the DWARF layout). We access the named member for x64 parity.
// FLAG: [c:+0x150] vs the committed Font's macTypefaceFamilyName @ [c:+0x14C] -- a
// console-build offset skew; the NAMED field is the correct, portable target.
// =============================================================================

namespace CgsGui
{
    namespace
    {
        // The fallback table the X360 consults when no registered font's typeface name is
        // a direct substring of the requested name. Interleaved {pattern, fallbackName}
        // pairs read from rodata @ 0x820E08DC / 0x820E08E0 in the ARTIST build:
        //   pair 0: if the request contains "machinestd-bold" -> a "machinestd-bold" font
        //   pair 1: if the request contains ""  (always true)  -> a "dfheic" font
        // The empty pattern makes pair 1 the universal last-resort fallback. The X360 walks
        // two pairs (v13 steps by 2 up to 4 over a flat pointer array).
        struct FontFallback { const char* mpcPattern; const char* mpcFallbackName; };
        const FontFallback gskaFontFallbacks[] =
        {
            { "machinestd-bold", "machinestd-bold" },   // 0x820E08DC / 0x820E08E0
            { "",                "dfheic"          },    // 0x820046A7 (empty) / 0x820E057C
        };
        const int KI_NUM_FALLBACKS = sizeof(gskaFontFallbacks) / sizeof(gskaFontFallbacks[0]);
    }

    CgsResource::SafeResourceHandle<CgsResource::Font>* FontCollection::FindFont(const char* lpcFontName)
    {
        // FindFont asserts (CgsGuiFontCollection.cpp:100).
        CGS_ASSERT(lpcFontName != nullptr, "lpcFontName");

        // String-too-long guard. The X360 walks to the NUL and asserts the length is
        // < 0x80 (CgsStringUtils.h:55), then strncpy's 128 bytes into a local buffer.
        {
            const char* lpcScan = lpcFontName;
            while (*lpcScan)
                ++lpcScan;
            CGS_ASSERT((lpcScan - lpcFontName) < 0x80, "String too long");
        }

        // Lower-case a bounded copy of the requested name; matching is case-insensitive.
        char acNameLower[0xF0];   // [c:char v23[240]]
        std::strncpy(acNameLower, lpcFontName, 128);
        acNameLower[128] = '\0';   // strncpy does not NUL-terminate on truncation
        CGSSTRNLOWER(acNameLower, 128);

        // ---- Pass 1: direct match (font's typeface family name is a substring of the
        // requested name). Scan up to KI_MAX_FONTS registered slots, skipping empties.
        for (int i = 0; i < KI_MAX_FONTS; ++i)
        {
            if (maFonts[i].IsNull())   // [c:slot != {dword_8305F174,_8305F178}]
                continue;

            const CgsResource::Font* lpFont = maFonts[i].Get();   // [c:sub_827EF870 -> Font*]
            if (std::strstr(acNameLower, lpFont->macTypefaceFamilyName))   // [c:font + 0x150]
                return &maFonts[i];
        }

        // ---- Pass 2: fallback table. For each {pattern, fallbackName} pair whose pattern
        // is a substring of the requested name, scan the registered slots again for a font
        // whose typeface name is a substring of the fallback name; return the first hit.
        for (int f = 0; f < KI_NUM_FALLBACKS; ++f)
        {
            if (!std::strstr(acNameLower, gskaFontFallbacks[f].mpcPattern))
                continue;   // [c:v13 += 2; retry next pattern]

            for (int i = 0; i < KI_MAX_FONTS; ++i)
            {
                if (maFonts[i].IsNull())
                    continue;

                const CgsResource::Font* lpFont = maFonts[i].Get();
                if (std::strstr(gskaFontFallbacks[f].mpcFallbackName, lpFont->macTypefaceFamilyName))
                    return &maFonts[i];
            }
            // No slot matched this triggered fallback -> fall through to the next pair
            // (X360 LABEL_26: v13 += 2).
        }

        // ---- Nothing matched. The X360 asserts NO FONTS REGISTERED when even the first
        // slot is the empty/default handle (CgsGuiFontCollection.cpp:144), then returns the
        // collection's first slot regardless (a degenerate, never-null result).
        CGS_ASSERT(!maFonts[0].IsNull(), "NO FONTS REGISTERED");
        return &maFonts[0];
    }
}
