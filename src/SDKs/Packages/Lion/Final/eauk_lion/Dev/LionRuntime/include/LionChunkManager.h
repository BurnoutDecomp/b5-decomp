#pragma once

// cLionChunkManager / cLionChunk -- Lion runtime effect-chunk hierarchy
// (SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionChunkManager.h).
//
// A cLionChunk is a node in a parent/child/sibling tree describing a Lion effect
// definition. The manager owns the root chunk list (mpChunks) and a tagged allocator
// (mpAllocator) used to free chunk nodes during teardown.
//
// Layouts recovered from the X360 DWARF (LionChunkManager.h / .cpp) and the
// UnLink/AppDeInit/ChunkGroupDeAlloc asm:
//   cLionChunk:        +0x00 mType  +0x04 mpData  +0x08 mpParent  +0x0C mpChild  +0x10 mpNext
//   cLionChunkManager: +0x00 mpAllocator  +0x04 mpChunks  (mSingleton is a static, not a member)
//
// Vendor code (eauk_lion), reconstructed in its canonical Lion home. Only the teardown
// path (UnLink / AppDeInit / ChunkGroupDeAlloc) is in this TU's recon ledger; the large
// query/build surface declared on the manager is owned by other LionChunkManager TUs and
// is intentionally not declared here.

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

struct cLionChunk
{
    u32         mType;     // +0x00
    const void* mpData;    // +0x04
    cLionChunk* mpParent;  // +0x08
    cLionChunk* mpChild;   // +0x0C first child
    cLionChunk* mpNext;    // +0x10 next sibling

    // Detach this chunk from its parent's child/sibling list.
    void UnLink();
};

struct cLionChunkManager
{
    void AppDeInit();

private:
    void ChunkGroupDeAlloc(cLionChunk* apChunk);

    EA::Allocator::ITaggedAllocator* mpAllocator;  // +0x00
    cLionChunk*                      mpChunks;      // +0x04 root chunk list
};
