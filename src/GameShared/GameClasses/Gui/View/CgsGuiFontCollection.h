#pragma once

// CgsGui::FontCollection -- the apt/Flapt text-layout font set. A small fixed array
// of typed CgsResource::Font handles (the registered typefaces), looked up by name.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   * AddFont @ 0x82853030 -- insert a font handle into the first free (empty) slot;
//     asserts ("No room to add font <name>") if all KI_MAX_FONTS slots are occupied.
//   * FindFont @ 0x82853168 walks the collection's handle array, dereferences each
//     non-empty handle to its CgsResource::Font, and matches the requested name
//     against the font's typeface family name (via strstr), with a small fallback
//     table for two well-known typeface families.
// Source path baked into both methods' asserts:
//   ..\..\..\GameShared\GameClasses\Gui/View/CgsGuiFontCollection.cpp  (lines 86/100/144).
//
// DWARF (CgsGuiFontCollection.h:50) also declares Construct() and CountLoadedFonts().
// Construct() is X360-attested INLINED into ViewModule::Construct @0x828605A0 (the
// three-slot sentinel seeding) and is declared below; CountLoadedFonts() has no X360
// attestation and stays undeclared -- per AGENTS.md "DWARF supplies names/types; the
// X360 ledger decides what exists".
//
// DWARF also attests FindFont as a CONST method returning
// `const SafeResourceHandle<CgsResource::Font>&` (CgsGuiFontCollection.h:65); the X360
// body never mutates the collection or the handle it returns (a2 walk aside), so the
// const method / const-ref return is semantic-parity-safe and is what the header
// declares below (previously mis-declared as non-const returning a mutable pointer).
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

        // DWARF CgsGuiFontCollection.h:50; X360-attested INLINED into
        // CgsGui::ViewModule::Construct @0x828605A0, which seeds each of the three
        // slot pairs from the module-static default-handle sentinel
        // (dword_8305F174/_8305F178). The x64-native empty/default handle is the
        // null handle -- the same emptiness AddFont/FindFont test via IsNull().
        void Construct()
        {
            for (int i = 0; i < KI_MAX_FONTS; ++i)
            {
                maFonts[i].mpResourceMemory = 0;
                maFonts[i].mpSourceEntry = 0;
            }
        }

        // @ 0x82853030 -- register a font handle in the first free (empty/default) slot.
        // Asserts ("No room to add font <name>", CgsGuiFontCollection.cpp:86) if every
        // slot is already occupied; on assert failure (dev builds continue past the
        // assert) the handle is silently dropped -- no slot is written. DWARF
        // (CgsGuiFontCollection.h:60) declares the parameter as a non-const reference.
        void AddFont(CgsResource::SafeResourceHandle<CgsResource::Font>& lTypeface);

        // @ 0x82853168 -- look a font up by name. Copies lpcFontName (asserted non-null and
        // < 0x80 chars) into a local buffer, lowercases it, then:
        //   1. returns the first registered font whose typeface family name is a substring
        //      of the (lowercased) requested name;
        //   2. otherwise consults a small fallback table (two {pattern, fallbackName}
        //      pairs) and returns the first registered font matching a triggered fallback;
        //   3. otherwise (NO FONTS REGISTERED) returns the collection's first slot.
        // The return is a reference to a registered handle slot (DWARF: const method,
        // const SafeResourceHandle<Font>& return -- CgsGuiFontCollection.h:65).
        const CgsResource::SafeResourceHandle<CgsResource::Font>& FindFont(const char* lpcFontName) const;
    };
}
