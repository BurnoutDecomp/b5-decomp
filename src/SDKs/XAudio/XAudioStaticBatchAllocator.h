#pragma once

// ===========================================================================
// XAUDIO::CStaticBatchAllocator -- a CBatchAllocator specialisation whose bump
// heap is CO-ALLOCATED INLINE with the object in a single XMem block (rather than
// bound separately through CreateHeap, and unlike CDynamicBatchAllocator which
// leaves the heap empty). This header is the canonical OWNING home for:
//
//     XAUDIO::CStaticBatchAllocator::CreateObject               @ 0x82C31620
//     XAUDIO::CStaticBatchAllocator::AbsoluteRelease            @ 0x82C313D8
//     XAUDIO::CStaticBatchAllocator::`vector deleting destructor'          @ 0x82C31520
//     XAUDIO::CStaticBatchAllocator::`vector deleting destructor'`adjustor{4}' @ 0x82C313D0
//
// The `vector deleting destructor' @ 0x82C31520 carries the real
// ~CStaticBatchAllocator() body (it nulls the inline heap pointer before chaining
// the base dtor); it is reconstructed below as the virtual destructor. Its
// `adjustor{4}' thunk @ 0x82C313D0 (`this -= 4` then forward, recovering the full
// object from the CObjectRefCount secondary subobject) is a compiler-synthesised
// MI thunk and -- exactly as the parent XAudioBatchAllocator.h / sibling
// XAudioDynamicBatchAllocator.h treat their own deleting-destructor thunks -- is
// NOT reproduced as source.
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed purely from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// LAYOUT: CStaticBatchAllocator adds NO data members; its X360 layout is identical
// to CBatchAllocator (base object 0x1C = 28 bytes). The distinguishing trait is the
// allocation strategy, not the layout: CreateObject XMemAllocs `heapSize + sizeof`
// bytes in ONE block and points mpHeapBase (+0x18) at the bytes immediately after
// the object (this + sizeof), so the object and its bump heap share a single
// XMem allocation.
//
// TEARDOWN (the two entry points below cooperate around the single co-allocated
// block, which must NOT be freed by the base's per-object XMemFree path):
//   - ~CStaticBatchAllocator (@0x82C31520) nulls mpHeapBase first, so the chained
//     ~CBatchAllocator sees mpHeapBase==0 and skips its XMemFree of the (inline) heap.
//   - AbsoluteRelease (@0x82C313D8) then frees the WHOLE block in one XMemFree with
//     the recorded attribute word. In the X360 it is a virtual on the CObjectRefCount
//     secondary subobject, so its `this` arrives as the +4 subobject and it reaches
//     the full object (the allocation base) via `this - 4`; modelled here as an
//     ordinary CStaticBatchAllocator method whose `this` is already the full object.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioBatchAllocator.h"   // XAUDIO::CBatchAllocator (base)

namespace XAUDIO
{

class CStaticBatchAllocator : public CBatchAllocator
{
public:
    // @ 0x82C31620 -- factory: XMem-allocate ONE block of `sizeof + uHeapSize` bytes,
    // construct the object with its bump heap seated inline (mpHeapBase = this+sizeof,
    // capacity = uHeapSize, cursor = 0, refcount = 1), publish it through the
    // out-parameter, and return an HRESULT-style status (0 == S_OK, 0x8007000E on OOM).
    static s32 CreateObject(u32 uXMemAttributes, u32 uHeapSize,
                            CStaticBatchAllocator** appObject);

    // Constructor building the co-allocated inline heap. Seats the CObjectRefCount
    // refcount to 1 and the CBatchAllocator heap words for a heap that begins at
    // `this + sizeof(CStaticBatchAllocator)`. (In the X360 this is inlined into
    // CreateObject; recovered here as the constructor CreateObject placement-news.)
    CStaticBatchAllocator(u32 uXMemAttributes, u32 uHeapSize);

    // @ 0x82C31520 -- virtual destructor. Nulls mpHeapBase so the chained
    // ~CBatchAllocator does not XMemFree the inline heap (it is part of this object's
    // single co-allocation and is released by AbsoluteRelease as one block).
    virtual ~CStaticBatchAllocator();

    // @ 0x82C313D8 -- destruct then free the WHOLE co-allocated block. Captures the
    // XMem attribute word, virtual-destructs in place (which for this class frees
    // nothing -- the heap is inline), then XMemFree's the single block with the
    // recorded attributes. Returns the freed object pointer (the X360 tail-returns
    // XMemFree's result).
    void* AbsoluteRelease();
};

} // namespace XAUDIO
