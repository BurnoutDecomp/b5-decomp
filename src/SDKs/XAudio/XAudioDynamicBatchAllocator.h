#pragma once

// ===========================================================================
// XAUDIO::CDynamicBatchAllocator -- a CBatchAllocator specialisation that is
// heap-created through an XMem-backed factory (rather than embedded by value).
// This header is the canonical OWNING home for:
//
//     XAUDIO::CDynamicBatchAllocator::CreateObject              @ 0x82C31570
//     XAUDIO::CDynamicBatchAllocator::~CDynamicBatchAllocator   (virtual dtor)
//
// The X360 also emits a `vector deleting destructor' @0x82C314A8 and its
// `adjustor{4}' thunk @0x82C313C8; both are compiler-synthesised MI thunks over
// the virtual dtor + the base operator delete, so -- exactly as the direct parent
// XAudioBatchAllocator.h treats its own deleting-destructor entry point + thunk --
// they are NOT reproduced as source.
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed purely from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// LAYOUT: CDynamicBatchAllocator adds NO data members; its 28-byte (0x1C) layout
// is identical to CBatchAllocator (the factory XMemAllocs 0x1C bytes and zeroes the
// bump-heap words +0x0C..+0x18, leaving the base in its CreateHeap-ready state).
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioBatchAllocator.h"   // XAUDIO::CBatchAllocator (base)

namespace XAUDIO
{

class CDynamicBatchAllocator : public CBatchAllocator
{
public:
    // @ 0x82C31570 -- factory: XMem-allocate a 28-byte CDynamicBatchAllocator, run its
    // two-vptr MI construction (mRefCount=1, empty bump heap), publish it through the
    // out-parameter, and return an HRESULT-style status (0 == S_OK, 0x8007000E on OOM).
    static s32 CreateObject(CDynamicBatchAllocator** appObject);

    // The virtual destructor. Adds no owned resources: ~CBatchAllocator() (heap release)
    // chains automatically, and the flagged XMemFree tail becomes the class operator
    // delete at the call site. The X360 vector deleting destructor + its adjustor{4}'
    // thunk are compiler-synthesised over this dtor and are not emitted as source.
    virtual ~CDynamicBatchAllocator();
};

} // namespace XAUDIO
