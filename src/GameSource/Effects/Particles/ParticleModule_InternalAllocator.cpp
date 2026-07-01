#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc

// ============================================================================
// GameSource/Effects/Particles/ParticleModule_InternalAllocator.cpp
//
// BrnParticle::IInternalAllocator member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnParticle::IInternalAllocator)
// bodies the three X360-emitted functions:
//
//   Free`adjustor{4}'                    @ 0x82289898
//   `scalar deleting destructor'         @ 0x822898A0
//   `vector deleting destructor'`adjustor{4}' @ 0x82289890
//
// Free`adjustor{4}' (0x82289898) is a plain adjustor thunk: `addi r3,r3,-4`
// then a tail branch into the scalar body -- i.e. it is called through the
// secondary (IAllocator-shaped) base pointer at this+4 and re-bases to the
// primary `this` before falling through to the real Free implementation. Its
// pseudocode calls `BrnParticle::IInternalAllocator::Free(a1 - 4)`, but that
// resolved symbol IS this class's own Free override -- the X360 export table
// has no separate non-adjustor Free entry point for this TU (Free itself was
// never independently exported/ledger-attested), so the thunk's landing body
// is IInternalAllocator::Free(void*, size_t) declared in the header. There is
// nothing further to body here beyond the thunk itself; the thunk is a pure
// compiler artifact (this-pointer adjustment), not hand-written source, so it
// has no separate C++ source expression -- it is implicit in `override`ing
// Free while inheriting IAllocator's secondary vtable slot (see the header's
// two-vtable-dtor note).
//
// The scalar deleting destructor (0x822898A0) and the vector-deleting-
// destructor's adjustor{4} thunk (0x82289890, `addi r3,r3,-4` then tail-branch
// into the scalar body) are entirely compiler-generated from the virtual
// ~IInternalAllocator() declared in the header: they store IInternalAllocator's
// own vtable at this+0, the inherited IAllocator secondary-vtable slot at
// this+4 (see the header's two-vtable-dtor note for why there are two stores),
// then conditionally operator-delete the object when the low bit of the flags
// argument is set. Defining ~IInternalAllocator() out-of-line (empty -- no
// owned resources; mpHeapMalloc is owned elsewhere) reproduces exactly that
// compiler-emitted sequence, matching the sibling precedent (CgsGraphics::
// MoviePlayerCoreAllocator, CgsGui::MemcardAllocator): no hand-written body is
// needed or possible for compiler-synthesised deleting-destructor thunks.
// ============================================================================

namespace BrnParticle
{
    // Out-of-line destructor: anchors the vtable (see the two-vtable-dtor note
    // in ParticleModule.h -- the X360 scalar/vector deleting-destructor thunk
    // block is synthesised by the host compiler from this virtual ~dtor). No
    // owned resources to release (mpHeapMalloc is owned elsewhere).
    IInternalAllocator::~IInternalAllocator()
    {
    }

    // X360 0x82289898 (Free`adjustor{4}') / the Free override it thunks into.
    // Forward the block to the backing heap. HeapMalloc::Free takes no size
    // argument (the caller-supplied anSize is only needed by allocators that
    // don't track block size internally); mpHeapMalloc does, so it is dropped
    // here exactly as the sibling CgsGui::MemcardAllocator::Free does.
    void IInternalAllocator::Free(void* apData, size_t anSize)
    {
        (void)anSize;
        mpHeapMalloc->Free(apData);
    }
}
