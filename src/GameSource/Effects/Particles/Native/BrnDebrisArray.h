#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisArray.h
//
// BrnParticle::Native::BrnDebrisArray -- the platform ("Native") debris-bucket array
// owned by the particle module. Live debris buckets hang off an intrusive doubly-linked
// "used" list; a separate bucket pool keeps the free list. Only the slice exercised by
// ClearAllBuckets is modelled here.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, ClearAllBuckets @ 0x8227E3D0):
//   this[1] @ +0x04 -- mpUsedHead   : head of the used-bucket doubly-linked list
//   this[2] @ +0x08 -- mpBucketPool : the bucket-pool node holding the free list head
//                                     (its [0] == free head, its [2] == free count)
//   this[3] @ +0x0C -- muUsedCount  : live bucket count, zeroed once the list is drained
//   this[0] @ +0x00 -- not touched by this function (kept as an opaque leading word)
//
//   Each bucket node: node[0] @ +0x00 == mpPrev, node[1] @ +0x04 == mpNext.
//
// X360 pointers are 32-bit; on the 64-bit host they widen so the absolute byte offsets
// above do NOT hold. The load-bearing facts reproduced are the intrusive-list SHAPE
// (prev/next first two words of each node) and the unlink-then-push SEQUENCE. Members
// are pinned BY NAME and order, not by an absolute-offset assert. GROW additively.
//
// HONEST PLACEHOLDER: the full BrnDebrisArray / debris-bucket / bucket-pool layout is not
// reconstructed in this pass; only the list-management fields are modelled.
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
namespace Native
{
    // One debris bucket: an intrusive doubly-linked-list node. Only the two link words
    // the list management touches are modelled; the bucket payload that follows is
    // opaque here (grow additively when the bucket layout lands).
    struct DebrisBucket
    {
        DebrisBucket* mpPrev;   // node[0] @ +0x00
        DebrisBucket* mpNext;   // node[1] @ +0x04
    };

    // The free-list pool node. Its first word doubles as the free-list head (so that
    // pushing a freed bucket and fixing up the head's prev pointer share the layout of a
    // bucket node), and the third word is the free count.
    struct DebrisBucketPool
    {
        DebrisBucket* mpFreeHead;   // pool[0] @ +0x00 -- head of the free list
        DebrisBucket* mpPad1;       // pool[1] @ +0x04 -- not touched here
        u32           muFreeCount;  // pool[2] @ +0x08 -- free bucket count
    };

    class BrnDebrisArray
    {
    public:
        // BrnParticle::Native::BrnDebrisArray::ClearAllBuckets @ 0x8227E3D0. Drain every
        // live bucket off the used list and push it back onto the bucket pool's free
        // list, then zero the live count. Caller (X360 xref):
        // BrnParticle::ParticleModule::ProcessEventQueue.
        void ClearAllBuckets();

    private:
        void*             mpReserved0;   // this[0] @ +0x00 -- opaque, not touched here
        DebrisBucket*     mpUsedHead;    // this[1] @ +0x04
        DebrisBucketPool* mpBucketPool;  // this[2] @ +0x08
        u32               muUsedCount;   // this[3] @ +0x0C
    };
}
}
