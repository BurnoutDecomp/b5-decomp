#pragma once

// CgsGui::FontCollection -- the apt/Flapt text-layout font set. A small fixed array
// of typed CgsResource::Font handles (the registered typefaces), looked up by name.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   * FindFont @ 0x82853168 (the only method dumped) walks the collection's handle
//     array, dereferences each non-empty handle to its CgsResource::Font, and matches
//     the requested name against the font's typeface family name (via strstr), with a
//     small fallback table for two well-known typeface families.
// Source path baked into FindFont's asserts:
//   ..\..\..\GameShared\GameClasses\Gui/View/CgsGuiFontCollection.cpp  (lines 100/144).
//
// LAYOUT (x64-native): the X360 walks the collection as an array of two-word slots
// (each slot is a CgsResource::SafeResourceHandle<Font> = {mpResourceMemory, mpSourceEntry})
// and caps every scan at 3 slots (the `v9 >= 3` guards). So the collection holds up to
// KI_MAX_FONTS == 3 typeface handles inline. Members are accessed BY NAME; the X360 slot
// is 8 bytes (two 32-bit words) while the x64 handle is two 8-byte pointers, so no
// byte-size static_assert is asserted (the project's x64 semantic-parity rule -- the
// console offsets are recorded in [c:0xNN] comments).

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // CgsResource::SafeResourceHandle

namespace CgsResource { struct Font; }

namespace CgsGui
{
    struct FontCollection
    {
        // The X360 FindFont scans at most 3 handle slots (v9 >= 3 / `cmpwi r30, 3`).
        enum { KI_MAX_FONTS = 3 };

        // The registered typeface handles. Each slot is a typed Font handle; an unused
        // slot is the empty/default handle (mpResourceMemory == nullptr). The X360 stores
        // these as consecutive two-word slots starting at the collection base [c:+0x00];
        // FindFont returns a pointer to the matching slot (or the collection base itself
        // as a degenerate fallback when nothing matches).
        CgsResource::SafeResourceHandle<CgsResource::Font> maFonts[KI_MAX_FONTS];   // [c:+0x00 .. +0x18]

        // @ 0x82853168 -- look a font up by name. Copies lpcFontName (asserted non-null and
        // < 0x80 chars) into a local buffer, lowercases it, then:
        //   1. returns the first registered font whose typeface family name is a substring
        //      of the (lowercased) requested name;
        //   2. otherwise consults a small fallback table (two {pattern, fallbackName}
        //      pairs) and returns the first registered font matching a triggered fallback;
        //   3. otherwise (NO FONTS REGISTERED) returns the collection's first slot.
        // The return is a pointer to a registered handle slot, never null.
        CgsResource::SafeResourceHandle<CgsResource::Font>* FindFont(const char* lpcFontName);
    };
}
