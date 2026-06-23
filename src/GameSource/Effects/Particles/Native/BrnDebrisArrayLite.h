#ifndef BRN_DEBRIS_ARRAY_LITE_H
#define BRN_DEBRIS_ARRAY_LITE_H

// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h
//
// BrnParticle::Native::BrnDebrisArrayLite -- the lightweight ("Lite") platform debris
// array. Its live debris buckets hang off an intrusive singly-linked list. Update walks
// every live bucket and integrates it for one frame via UpdateBucket (caller:
// DebrisUpdateJob::Execute).
//
// LAYOUT AUTHORITY (X360 ARTIST asm, Update @0x82C08B58):
//   this[0]    @ +0x00 -- mpBucketHead : head of the live-bucket singly-linked list
//                         (the walk follows bucket[1] @ +0x04 == mpNext).
//   this+0x10  @ +0x10 -- a 4-float vector loaded into the per-bucket update (the
//                         physics/gravity step vector).
//   this+0x20  @ +0x20 -- a float splatted across all lanes for the per-bucket update.
//   this[9]    @ +0x24 -- a byte read into r6 and forwarded to UpdateBucket (a flag).
//   Each bucket node: bucket[1] @ +0x04 == mpNext.
//
// HONEST STUB -- VMX KEYSTONE (Update NOT reconstructed in this pass):
//   Update @0x82C08B58 is the per-bucket integration shim. Although its outer control
//   flow is a scalar singly-linked-list walk, every iteration marshals HAND-VECTORISED
//   VMX/AltiVec register state into BrnDebrisArrayLite::UpdateBucket @0x82C08410:
//     - lvlx + vspltw128 splat the frame-time float (a9) into v127 (-> v1),
//     - lvlx + vspltw128 splat this+0x20 into v126 (-> v3),
//     - lvx128 loads the this+0x10 vector into v2,
//   and UpdateBucket is then invoked with those three vectors passed BY VMX REGISTER.
//   UpdateBucket itself (0x82C08410) is NOT present in the X360 export set, so its
//   signature/body are unrecovered. There is no faithful scalar lowering of a
//   vector-register call ABI into an undecompiled VMX callee without inventing the
//   parameter layout. Per project policy this is FLAGGED as a VMX keystone and given an
//   honest non-fabricated stub rather than a paraphrase. Reconstructing it requires
//   recovering UpdateBucket's body + register contract (a dedicated VMX pass).
//
// Members are pinned BY NAME and order. GROW additively. X360 pointers are 32-bit, so
// the absolute byte offsets above do NOT hold on the 64-bit host.
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
namespace Native
{
    // One debris bucket node in the live-bucket list. Only the next link the walk
    // touches is modelled; the bucket payload that follows is opaque here.
    struct DebrisArrayLiteBucket
    {
        void*                  mpReserved0;   // bucket[0] @ +0x00 -- opaque, not touched by the walk
        DebrisArrayLiteBucket* mpNext;        // bucket[1] @ +0x04
    };

    class BrnDebrisArrayLite
    {
    public:
        // BrnParticle::Native::BrnDebrisArrayLite::Update @0x82C08B58.
        // Integrate every live debris bucket for one frame. lfFrameTime is the splatted
        // time step; the remaining job parameters are forwarded to UpdateBucket.
        // HONEST STUB: not implemented -- see file header (VMX keystone).
        void Update(int liJobParam2, int liJobParam3, int liJobParam5, int liJobParam6,
                    int liJobParam7, float lfFrameTime);

    private:
        DebrisArrayLiteBucket* mpBucketHead;   // this[0] @ +0x00
    };
}
}

#endif
