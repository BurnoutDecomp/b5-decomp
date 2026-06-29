#pragma once

// ===========================================================================
// XAUDIO::CBatchAllocator -- a bump-pointer heap allocator backing the XAudio
// batch-allocation path in BURNOUT_X360_ARTIST.XEX. This header is the
// canonical OWNING home for:
//
//     XAUDIO::CBatchAllocator::CreateHeap                       @ 0x82C311F8
//     XAUDIO::CBatchAllocator::Alloc                            @ 0x82C31270
//     XAUDIO::CBatchAllocator::GrowHeap                         @ 0x82C311E0
//     XAUDIO::CBatchAllocator::GetFreeHeapSize                  @ 0x82C313B8
//     XAUDIO::CBatchAllocator::~CBatchAllocator                 @ 0x82C312E0
//     XAUDIO::CBatchAllocator::`vector deleting destructor'     @ 0x82C31448
//     XAUDIO::CBatchAllocator::`vector deleting destructor'`adjustor{4}' @ 0x82C31350
//     XAUDIO::CBatchAllocator::AddRef`adjustor{4}'              @ 0x82963068
//     XAUDIO::CBatchAllocator::Release`adjustor{4}'             @ 0x82C31440
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed purely from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// INHERITANCE (two-vptr MI, attested by the destructor's two staged vptr stores
// and the two `adjustor{4}' thunks that recover `this` from the secondary
// subobject at +4):
//   - Primary base   (vptr @+0x00, off_821B5694) : CBatchAllocatedObject -- the
//     XMem-batch-allocated, held-object-owning polymorphic base. The vector
//     deleting destructor + its operator-delete free route through this base's
//     XMem path (XMemFree(&unk_83222C40, this, 0x61820000)).
//   - Secondary base (vptr @+0x04, off_821B5680 / off_82108FF4) : CObjectRefCount
//     -- the intrusive ref-counted base. The `AddRef'/`Release' virtual slots
//     live on this secondary subobject, so their X360 entry points are
//     `adjustor{4}' thunks (`this -= 4` then forward); the compiler synthesises
//     those thunks, so they are not reproduced as source. (The X360 AddRef thunk
//     COMDAT-folds onto CXMASourceEffect::AddRef and the Release thunk onto
//     CBatchAllocator::Release -- identical ref-count bodies the linker merged;
//     in portable C++ the inherited CObjectRefCount::AddRef/Release supply that
//     behaviour.)
//
// X360 LAYOUT (32-bit, grounded in the asm load/store displacements; the host
// build's pointer-width will shift the member offsets -- semantic parity, the
// staged vptr writes and the exact byte offsets are X360 compiler artifacts):
//   +0x00  primary vptr   (CBatchAllocatedObject subobject)
//   +0x04  secondary vptr (CObjectRefCount subobject)
//   +0x08  (unused by this TU)
//   +0x0C  muXMemAttributes -- the XMem allocate/free attribute word; CreateHeap
//                              stores its `a2` here (stw r5,0xC) and the dtor
//                              passes it as XMemFree's attribute arg (lwz 0xC).
//   +0x10  muCapacity       -- heap capacity in bytes; CreateHeap guards on it
//                              (non-zero), GrowHeap adds to it, GetFreeHeapSize
//                              and Alloc's free-size check read it.
//   +0x14  muCursor         -- bump cursor (bytes consumed); Alloc advances it.
//   +0x18  mpHeapBase       -- base address returned by XMemAlloc; Alloc adds the
//                              cursor to it, the dtor frees + nulls it.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioBatchAllocatedObject.h" // XAUDIO::CBatchAllocatedObject
#include "SDKs/XAudio/XAudioObjectRefCount.h"        // XAUDIO::CObjectRefCount

namespace XAUDIO
{

class CBatchAllocator : public CBatchAllocatedObject, public CObjectRefCount
{
public:
    // @ 0x82C312E0 -- free the backing heap (XMemFree(mpHeapBase,
    // muXMemAttributes)) through the XAudio XMem manager singleton and null the
    // base pointer; the two base subobject dtors chain afterwards (the staged
    // vptr re-seats off_821B5694 / off_82108FF4 the asm shows). The X360 scalar /
    // vector deleting destructors @ 0x82C31448 / 0x82C31350 are the
    // compiler-synthesised thunks over this dtor + the base operator delete.
    virtual ~CBatchAllocator();

    // @ 0x82C311F8 -- bind the heap: require a non-zero capacity (else return 0),
    // record `uXMemAttributes`, allocate `muCapacity` bytes through the XAudio
    // XMem manager, and store the result as mpHeapBase. Returns 0 (S_OK) when the
    // allocation succeeds, else 0x8007000E (E_OUTOFMEMORY). When capacity is 0 it
    // returns 0 without allocating.
    s32 CreateHeap(u32 uXMemAttributes);

    // @ 0x82C31270 -- bump-allocate `uSize` bytes (rounded up to a 4-byte
    // multiple) from the heap. Returns 0 when the rounded size exceeds the
    // (virtually-dispatched) free heap size; otherwise returns mpHeapBase +
    // muCursor and advances the cursor by the rounded size.
    void* Alloc(u32 uSize);

    // @ 0x82C311E0 -- grow the recorded capacity by `uSize` rounded up to a
    // 4-byte multiple (does not touch the cursor or the backing allocation).
    void GrowHeap(u32 uSize);

    // @ 0x82C313B8 -- virtual: bytes still available = muCapacity - muCursor.
    // (Alloc dispatches this through the primary vtable slot @+0x10 to gate the
    // bump.)
    virtual u32 GetFreeHeapSize();

private:
    u32   muUnused08;        // +0x08 -- not read by this TU
    u32   muXMemAttributes;  // +0x0C
    u32   muCapacity;        // +0x10
    u32   muCursor;          // +0x14
    void* mpHeapBase;        // +0x18
};

} // namespace XAUDIO
