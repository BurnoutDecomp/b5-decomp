#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsTextureScopeTable.cpp - the render dispatcher's texture-purpose registry.
//
//   AddTexturePurpose  @ 0x827EC180
//   TextureScopeTable  @ 0x827EC260  (ctor)
//
// Reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly (authoritative for the
// per-entry store order/offsets). The TexturePurposeEntry / TextureScopeTable layout
// lives in the owning header CgsTextureScopeTable.h.

namespace CgsGraphics
{
    // -----------------------------------------------------------------------------
    // Honest reconstruction note for the name store:
    //
    // The X360 computes strlen(lpcName) (the `while (*v8++) ;` scan), allocates a
    // buffer of that length+1 (the call to the out-of-scope string allocator
    // sub_82C08C00 == `new char[len+1]`, same helper the shader-constant table uses),
    // then byte-copies the name (the do/while stb loop @ 0x827EC1FC). r3 is left
    // pointing just past the copied terminator and returned; the ctor discards it. We
    // reproduce the allocate + copy + that return pointer faithfully.
    // -----------------------------------------------------------------------------
    char* TextureScopeTable::AddTexturePurpose(u32 leTexturePurpose, const char* lpcName, s32 liScope)
    {
        CGS_ASSERT(leTexturePurpose < static_cast<u32>(E_MAX_TEXTURE_PURPOSES),
                   "( lePurpose < E_MAX_TEXTURE_PURPOSES ) && ( lePurpose >= E_TEXTURE_PURPOSE_NONE )");

        // strlen scan (the asm walks lpcName to its terminator to size the buffer).
        const char* lpcScan = lpcName;
        while (*lpcScan++)
        {
        }
        const s32 liLength = static_cast<s32>(lpcScan - lpcName);   // includes the terminator

        // Allocate the duplicate buffer and byte-copy the name (incl. terminator). r3
        // walks past the last written byte; that post-copy pointer is the return value.
        char* lpcDst = new char[liLength];
        const char* lpcSrc = lpcName;
        char lcByte;
        do
        {
            lcByte = *lpcSrc++;
            *lpcDst++ = lcByte;
        }
        while (lcByte);

        TexturePurposeEntry& lrEntry = maEntries[leTexturePurpose];
        lrEntry.mpcName          = const_cast<char*>(lpcName);
        lrEntry.meTexturePurpose = static_cast<s32>(leTexturePurpose);
        lrEntry.miScope          = liScope;
        if (lrEntry.mpBoundResourceA != nullptr)
        {
            lrEntry.mpBoundResourceA = nullptr;
            lrEntry.mClearState      = 0;
        }
        if (lrEntry.mpBoundResourceB != nullptr)
        {
            lrEntry.mpBoundResourceB = nullptr;
            lrEntry.mClearState      = 0;
        }
        lrEntry.mClearState = 0;

        return lpcDst;
    }

    // X360 0x827EC260: register the seven texture purposes with their scope counts.
    TextureScopeTable::TextureScopeTable()
    {
        AddTexturePurpose(0, "none", 1);
        AddTexturePurpose(1, "AmbientOcclusionMap", 1);
        AddTexturePurpose(2, "shadowMap", 2);
        AddTexturePurpose(3, "glassFractureTexturePersistent", 2);
        AddTexturePurpose(4, "environmentMap", 2);
        AddTexturePurpose(5, "glassFractureTexture", 2);
        AddTexturePurpose(6, "shadowMapAniso", 2);
    }
}
