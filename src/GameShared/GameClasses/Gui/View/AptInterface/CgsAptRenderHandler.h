#pragma once

#include "types.hpp"

// CgsGui::AptRenderHandler - the GUI's Apt (Adobe-Flash-player) rendering bridge: it owns the
// Im2d renderer set, a white fallback texture, and two large per-frame string/rendering-unit
// caches the Apt callback renderer fills while drawing a movie. Recovered from the X360 ARTIST
// binary:
//   AptRenderHandler()      @ 0x827DFBD8  (constructor; EXECUTED in the boot trace)
//   SetWhiteTexture(tex)    @ 0x828466E0  (EXECUTED in the boot trace)
//   GetIm2dRendererType()   @ 0x82846780  (debug-asserted Im2d renderer-type accessor)
//
// The real guest object is very large (~108 KB) and only PARTIALLY recovered here: these three
// functions are the entire in-scope slice, so this class models ONLY the fields they touch.
// Every other DWARF member is uncommitted and intentionally OMITTED.  FLAG: minimal-slice class.
//
// CrashNavIcon/BoostBarRenderer-style reconstruction: standalone class (NO base class, NO raw
// offset casts), members declared BY NAME, repeated stride writes grouped into arrays cleared by
// loops, data-segment pointers held as named slots. Exact byte offsets are NOT reproduced because
// the gate compiles for a 64-bit host (pointers widen 4->8 bytes), so the X360 pseudocode offsets
// are not load-bearing here -- the constructor reproduces the SAME stores (zeroed dword runs,
// 0x7FFFFFFF-seeded slot arrays, cleared flag bytes, -1 sentinels) in the same order.
namespace CgsGui
{
    class AptRenderHandler
    {
    public:
        // X360 0x827DFBD8. Zeroes two leading dword runs, seeds two large slot arrays with the
        // 0x7FFFFFFF "unset" marker, clears two flag bytes, and stores the -1 "no current entry"
        // sentinel into two trailing index words.
        AptRenderHandler();

        // X360 0x828466E0. Cache the white fallback texture pointer (guest +0x80). Asserts the
        // incoming pointer is non-null ("Invalid texture pointer sent to AptRenderHandler::
        // SetWhiteTexture").
        void SetWhiteTexture(void* lpWhiteTexture);

        // X360 0x82846780. Return the active Im2d renderer's leading type word: asserts the
        // renderer pointer (mpImRenderers, guest +108588) is non-null ("mpImRenderers"), then
        // returns *mpImRenderers (the first dword of the pointed-to renderer). The X360 does a
        // member load (lwz r11,0(r31)) then a SINGLE deref of that pointer (lwz r3,0(r11)).
        int GetIm2dRendererType() const;

    private:
        // One slot of a 0x7FFFFFFF-seeded cache (the ctor writes only the leading marker word of
        // each 3-dword slot; the other two dwords are left untouched). FLAG: only the leading
        // marker word is attested -- the slot's other two dwords are opaque uninitialised state.
        struct MarkerSlot
        {
            s32 miMarker;     // ctor stores 0x7FFFFFFF (KI_UNSET)
            s32 maOpaque[2];  // untouched by the ctor
        };

        static const s32 KI_UNSET     = 0x7FFFFFFF;  // the "unset" slot marker the ctor seeds
        static const s32 KI_NO_ENTRY  = -1;          // the "no current entry" index sentinel
        static const u32 KU_NUM_SLOTS = 25;          // each slot array is 25 entries (loop r10=24..0)

        // ---- leading zeroed dword runs (guest +0x88..+0x98 and +0xA0..+0xB0) ----
        u32 maZeroRunA[5];  // guest +0x88..+0x98 (the ctor's first 5-store run, dup-first-write)
        u32 maZeroRunB[5];  // guest +0xA0..+0xB0 (the ctor's second 5-store run, dup-first-write)

        // ---- white fallback texture (guest +0x80, written by SetWhiteTexture) ----
        void* mpWhiteTexture;

        // ---- first 0x7FFFFFFF-seeded slot array (guest +99780) + its trailing flag/sentinel ----
        MarkerSlot maSlotsA[KU_NUM_SLOTS];  // guest +99780, 12-byte stride, marker-only writes
        u8         mbFlagA;                 // guest +100072 (cleared to 0)
        s32        miEntryIndexA;           // guest +104172 (set to -1)

        // ---- second 0x7FFFFFFF-seeded slot array (guest +104184) + its trailing flag/sentinel --
        MarkerSlot maSlotsB[KU_NUM_SLOTS];  // guest +104184, 12-byte stride, marker-only writes
        u8         mbFlagB;                 // guest +104476 (cleared to 0)
        s32        miEntryIndexB;           // guest +108576 (set to -1)

        // ---- Im2d renderer set (guest +108588; read by GetIm2dRendererType) ----
        // Pointer to the active Im2d renderer; GetIm2dRendererType returns its leading dword
        // (one deref of this held pointer).
        int* mpImRenderers;                 // guest +108588 (mpImRenderers)
    };
}
