#pragma once

// ===========================================================================
// EATech Apt -- AptPseudoDisplayList (the interpreter's pseudo display list).
//
// AptPseudoDisplayList is the doubly-linked display list APT builds while
// interpreting a movie's temporary-frame place/remove commands (AptMovie::
// DoTemporaryFrameControls, AptCIH::jumpToFrame). Each entry is an
// AptPseudoCIH_t node (the 20-byte pseudo character-instance header already
// homed in AptPseudoCIH.h); the list keeps them ordered by the node's context
// key and pool-allocates/frees them through the shared Apt DOGMA pool.
//
// NO leak/DWARF exists. SHAPE is reconstructed STRICTLY from the X360
// ARTIST.XEX pseudocode/asm of the five owned methods plus the FindInst callee:
//     AptPseudoDisplayList::AptPseudoDisplayList @ 0x82AE5B10  (ctor)
//     AptPseudoDisplayList::ClearList            @ 0x82AEAFC8
//     AptPseudoDisplayList::Insert               @ 0x82AEE4D0
//     AptPseudoDisplayList::Remove               @ 0x82AEB018
//     AptPseudoDisplayList::~AptPseudoDisplayList @ 0x82AECBF0
//
// LAYOUT (8 bytes; ctor stores *this = head node, this[1] = owner):
//   +0x00  mpHead   AptPseudoCIH_t*  a zeroed 20-byte sentinel/head node, pool-
//                                     allocated by the ctor (all 5 dwords zeroed)
//   +0x04  mpOwner  void*            the owning context (ctor arg a2)
//
// The list links live INSIDE each AptPseudoCIH_t node (AptPseudoCIH.h):
//   node +0x0C  mpNext     forward link
//   node +0x10  mpPrev     back link
// ClearList/dtor walk mpHead->mpNext; Insert/Remove splice on mpNext/mpPrev.
//
// This is vendor/SDK code reconstructed in its canonical Apt home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptPseudoDisplayList,
// ClearList/Insert/Remove/FindInst) are kept verbatim.
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"   // AptPseudoCIH_t (the node), gpAptPseudoDataPool
#include "SDKs/EATech/Apt/DogmaAllocator.h"          // DOGMA_PoolManager

// ---------------------------------------------------------------------------
// FLAG (un-homed, owned by the Apt allocator boot TU): the shared DOGMA pool the
// 20-byte display-list nodes are allocated from. The X360 loads it from
// off_8324D808 (the same slot AptPseudoCIH.h / AptValueVector / AptSingleListPolicy
// reference). gpAptPseudoDataPool (declared in AptPseudoCIH.h) IS that slot;
// reused here by name so the same underlying pool object backs every Apt family.
// ---------------------------------------------------------------------------

class AptPseudoDisplayList
{
public:

    // ctor @ 0x82AE5B10 -- pool-allocate a zeroed 20-byte head node and store the
    // owner. X360: mpHead = Allocate(pool, 20) (all 5 dwords zeroed on success,
    // or null), this[1] = a2. Returns *this in the fastcall convention.
    AptPseudoDisplayList(void* lpOwner);

    // dtor @ 0x82AECBF0 -- ClearList() the chained nodes, then destroy + free the
    // head node itself (its three leading dwords are cleared by ~AptPseudoCIH_t
    // and the 20-byte block is returned to the pool).
    ~AptPseudoDisplayList();

    // ClearList @ 0x82AEAFC8 -- walk mpHead->mpNext, destroying (~AptPseudoCIH_t)
    // and freeing each node (20 bytes) until the chain ends. Does not touch the
    // head node (only its successors are real entries).
    void ClearList();

    // Insert @ 0x82AEE4D0 -- find the insertion slot for lpNode by its context
    // key (lpNode->mpContext, read at node+0x08), evicting any existing entry
    // with the same key (Remove), then splice lpNode after the predecessor that
    // FindInst returns.
    void Insert(AptPseudoCIH_t* lpNode);

    // Remove @ 0x82AEB018 -- unlink lpNode from the doubly-linked list (fix up its
    // neighbours' mpPrev/mpNext), destroy it (~AptPseudoCIH_t) and free the 20-byte
    // block back to the pool.
    void Remove(AptPseudoCIH_t* lpNode);

    // FindInst @ 0x82AD99A0 -- the ordered-position finder (owned by another TU;
    // declared so Insert links). Given a context key, fills *ppPred with the node
    // to insert after and *ppExisting with any node already holding that key.
    void FindInst(void* lpKey, AptPseudoCIH_t** ppExisting, AptPseudoCIH_t** ppPred);

    AptPseudoCIH_t* mpHead;    // +0x00 zeroed sentinel/head node
    void*           mpOwner;   // +0x04 owning context (ctor arg)
};
