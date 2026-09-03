// cLionChunkManager / cLionChunk -- effect-chunk hierarchy teardown.
//
// Vendor code (eauk_lion), reconstructed from the X360 asm/pseudocode at
//   0x829086E8 cLionChunk::UnLink
//   0x8290ECE0 cLionChunkManager::AppDeInit
//   0x8290ABA0 cLionChunkManager::ChunkGroupDeAlloc
// against the DWARF layouts in LionChunkManager.h. Store-for-store faithful; members by
// name. These three functions are this TU's full recon ledger.

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionChunkManager.h"

void cLionChunk::UnLink()
{
    cLionChunk* lpParent = mpParent;
    if (!lpParent)
    {
        return;
    }

    if (lpParent->mpChild)
    {
        // Walk the parent's child/sibling list looking for this node, tracking the slot
        // (parent->mpChild for the first child, or the predecessor's mpNext) that points
        // at it.
        cLionChunk** lppSlot = &lpParent->mpChild;
        cLionChunk*  lpCur   = *lppSlot;
        while (lpCur != this)
        {
            lppSlot = &lpCur->mpNext;
            lpCur   = lpCur->mpNext;
            if (!lpCur)
            {
                mpParent = nullptr;
                return;
            }
        }
        // Found: splice it out.
        if (lpCur)
        {
            *lppSlot     = lpCur->mpNext;
            lpCur->mpNext = nullptr;
        }
    }

    mpParent = nullptr;
}

void cLionChunkManager::ChunkGroupDeAlloc(cLionChunk* apChunk)
{
    cLionChunk* lpChunk = apChunk;
    while (lpChunk)
    {
        cLionChunk* lpNext = lpChunk->mpNext;

        // Recurse into children first, then detach and free this node.
        ChunkGroupDeAlloc(lpChunk->mpChild);
        lpChunk->UnLink();

        lpChunk->mType    = 0;
        lpChunk->mpData   = nullptr;
        lpChunk->mpParent = nullptr;
        lpChunk->mpChild  = nullptr;
        lpChunk->mpNext   = nullptr;
        mpAllocator->Free(lpChunk, 0);

        lpChunk = lpNext;
    }
}

void cLionChunkManager::AppDeInit()
{
    cLionChunk* lpChunk = mpChunks;
    while (lpChunk)
    {
        cLionChunk* lpNext = lpChunk->mpNext;

        // Free the chunk's child group, then detach the chunk from its parent (inlined
        // UnLink) and free it.
        ChunkGroupDeAlloc(lpChunk->mpChild);

        cLionChunk* lpParent = lpChunk->mpParent;
        if (lpParent)
        {
            if (lpParent->mpChild)
            {
                cLionChunk** lppSlot = &lpParent->mpChild;
                cLionChunk*  lpCur   = *lppSlot;
                while (lpCur && lpCur != lpChunk)
                {
                    lppSlot = &lpCur->mpNext;
                    lpCur   = lpCur->mpNext;
                }
                if (lpCur == lpChunk && lpChunk)
                {
                    *lppSlot       = lpChunk->mpNext;
                    lpChunk->mpNext = nullptr;
                }
            }
            lpChunk->mpParent = nullptr;
        }

        lpChunk->mType    = 0;
        lpChunk->mpData   = nullptr;
        lpChunk->mpParent = nullptr;
        lpChunk->mpChild  = nullptr;
        lpChunk->mpNext   = nullptr;
        mpAllocator->Free(lpChunk, 0);

        lpChunk = lpNext;
    }
}

// ------------------------------------------------------------------------------------------------
// cLionChunkManager::GetMe / ::AppInit  (DWARF LionChunkManager.h:123 / h:15)
//
// NEITHER HAS A STANDALONE X360 BODY. GetMe is inlined at every call site (which is why
// cParticleSystem::AppDeInit @0x82911DF0 passes the literal `&off_83121DC8` as `this`), and
// AppInit is inlined into cParticleSystem::AppInit @0x82913810, where it is two stores:
//     off_83121DC8   = apAllocator   ; mpAllocator
//     dword_83121DCC = 0             ; mpChunks
//
// A file-scope object, NOT a function-local static: the console has no magic-static guard word
// beside it (the next .bss datum is 8 bytes later).
// ------------------------------------------------------------------------------------------------
namespace
{
    cLionChunkManager gChunkManagerSingleton;   // X360 mSingleton @0x83121DC8
}

cLionChunkManager* cLionChunkManager::GetMe()
{
    return &gChunkManagerSingleton;
}

void cLionChunkManager::AppInit(EA::Allocator::ITaggedAllocator* apAllocator)
{
    mpAllocator = apAllocator;
    mpChunks    = nullptr;
}
