#pragma once

#include "types.hpp"

// =============================================================================
// CgsTextureScopeTable.h  (GameShared/GameClasses/Graphics/Dispatch)
//
// The render dispatcher's named-texture-purpose registry. The engine binds a
// small fixed set of "scoped" textures (ambient-occlusion / shadow / environment
// / glass-fracture maps) that the dispatch path resolves by purpose enum. Each
// slot records the purpose's name, its index, and a per-purpose scope count, plus
// a pair of currently-bound resource pointers that get cleared on (re)assignment.
//
// Layout is reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly
// (CgsGraphics::TextureScopeTable::AddTexturePurpose @ 0x827EC180, ctor
// @ 0x827EC260; authoritative for the per-entry store offsets) and the assert
// strings baked into those bodies (CgsTextureScopeTable.h, "lePurpose <
// E_MAX_TEXTURE_PURPOSES" / ">= E_TEXTURE_PURPOSE_NONE").
//
// X360-verified per-entry layout (the ctor indexes maEntries with stride 0x1C,
// `mulli r,idx,0x1C`, so each TexturePurposeEntry is 28 bytes / 7 words):
//   0x00  mpcName            char*   duplicated purpose name (sub_82C08C00 strdup)
//   0x04  meTexturePurpose   s32     the purpose index (a2 / lePurpose)
//   0x08  miScope            s32     per-purpose scope count (a4: none/AO=1, rest=2)
//   0x0C  mpBoundResourceA   void*   bound resource; cleared (with mClearState) on set
//   0x10  mpBoundResourceB   void*   bound resource; cleared (with mClearState) on set
//   0x14  mClearState        s32     reset-to-0 on (re)assignment
//   0x18  muReserved         u32     7th word (not touched by AddTexturePurpose)
// =============================================================================

namespace CgsGraphics
{
    // E_TEXTURE_PURPOSE_NONE..E_MAX_TEXTURE_PURPOSES (the ctor registers indices
    // 0..6; AddTexturePurpose asserts lePurpose in [E_TEXTURE_PURPOSE_NONE,
    // E_MAX_TEXTURE_PURPOSES) i.e. 0..6).
    enum E_TexturePurpose
    {
        E_TEXTURE_PURPOSE_NONE = 0,
        E_MAX_TEXTURE_PURPOSES = 7
    };

    struct TexturePurposeEntry
    {
        char* mpcName;            // +0x00
        s32   meTexturePurpose;   // +0x04
        s32   miScope;            // +0x08
        void* mpBoundResourceA;   // +0x0C
        void* mpBoundResourceB;   // +0x10
        s32   mClearState;        // +0x14
        u32   muReserved;         // +0x18
    };

    struct TextureScopeTable
    {
        // X360 0x827EC260: the ctor registers the seven texture purposes.
        TextureScopeTable();

        // X360 0x827EC180: duplicate lpcName onto the heap, store the purpose's
        // name/index/scope into maEntries[lePurpose], and clear the bound-resource
        // pointers + clear-state.
        char* AddTexturePurpose(u32 leTexturePurpose, const char* lpcName, s32 liScope);

        TexturePurposeEntry maEntries[E_MAX_TEXTURE_PURPOSES];
    };
}
