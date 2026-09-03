#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT (the 5-arg Alloc)

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

// ============================================================================================
// THE THREE Alloc OVERLOADS + AddRef/Release -- BODIED 2026-09-03 (boost-exhaust wave).
//
// ⛔ THE COMMENT THAT USED TO STAND HERE WAS THE REGRESSION. ParticleModule.h said these were
// "DECLARE-ONLY: not attested individually in the X360 ledger (only Free and the dtor thunks
// are function-tracked for this TU) -- bodying them would fabricate behavior... a faithful
// Alloc body cannot be written without guessing the tag/name/flags -> alignment mapping."
// Both halves are wrong:
//   * BrnParticle::IInternalAllocator::Alloc @0x82289810 IS in progress/identity.json and IS
//     in the export set with pseudocode. It was found by reading the class's VTABLE, which the
//     inlined constructor inside ParticleModule::Prepare @0x8229BEA0 names outright
//     (`dword_82FAD27C = off_82011E14; dword_82FAD280 = off_82011E00`).
//   * There is no tag->alignment mapping to guess, because the console does not map anything:
//     the tagged Alloc ignores its tag list entirely and passes a LITERAL 16.
//
// The four bodies, from the vtable at off_82011E14 (primary) and off_82011E00 (the secondary
// IAllocator sub-object at this+4):
//     82011E14[0] 0x822898A0  `scalar deleting destructor'
//     82011E14[1] 0x82289820  Alloc(size, name, flags, align, alignOffset)
//     82011E14[2] 0x82289810  Alloc(size, tags)
//     82011E14[3] 0x82289888  Free(ptr, size)
//     82011E00[0] 0x82289878  Alloc(size, tags) reached through the +4 base (same body, `this`
//                             un-adjusted, so it reads mpHeapMalloc at +4 instead of +8)
//     82011E00[1] 0x82289898  Free`adjustor{4}'
//     82011E00[2] 0x82C296C8  AddRef    -- `li r3,1; blr`, ICF-folded with 41 other
//     82011E00[3] 0x82C296C8  Release      one-instruction `return 1` bodies in the image
//     82011E00[4] 0x82289890  `vector deleting destructor'`adjustor{4}'
//
// ⚠ WHY THIS MATTERS RIGHT NOW: this class is the EA::Allocator::ITaggedAllocator that
// ParticleModule::Prepare hands to cLionFX::Init, so EVERY Lion pool -- buckets, emitters,
// locators, triggers, effect instances, the small-block pages -- is carved through these three
// functions. With them undefined the Lion install could not even link.
// ============================================================================================

// The tagged Alloc's alignment: `li r5, 0x10` at 0x82289814, passed straight to
// HeapMalloc::Malloc. Not HeapMalloc's own KI_DEFAULT_ALIGNMENT (4) -- this class overrides it.
static const s32 KI_INTERNAL_ALLOC_ALIGNMENT = 16;

// X360 0x82289810. The tag list is accepted and ignored: the console reads nothing out of it.
void* IInternalAllocator::Alloc(size_t anSize, const EA::TagValuePair& /*arTags*/)
{
    return mpHeapMalloc->Malloc(static_cast<s32>(anSize), KI_INTERNAL_ALLOC_ALIGNMENT);
}

// The name/flags overload. NOT SEPARATELY EMITTED: the X360 image has one body for the 3-arg
// and 5-arg forms -- 0x82289820 takes (this, size, name, flags, align, alignOffset) -- so the
// 3-arg form is the same entry point reached with the trailing pair defaulted. Reproduced as
// the 3-arg forwarding to the 5-arg with the same default alignment the tagged form uses.
void* IInternalAllocator::Alloc(size_t anSize, const char* apName, u32 auFlags)
{
    return Alloc(anSize, apName, auFlags, KI_INTERNAL_ALLOC_ALIGNMENT, 0);
}

// X360 0x82289820. The alignment comes from the CALLER here (r5 -> HeapMalloc::Malloc's third
// argument), and a non-zero alignOffset is an assert, not a supported case
// (ParticleModule.cpp:124).
void* IInternalAllocator::Alloc(size_t anSize, const char* /*apName*/, u32 /*auFlags*/,
                                u32 auAlign, u32 auAlignOffset)
{
    CGS_ASSERT(auAlignOffset == 0, "alignOffset == 0");   // ParticleModule.cpp:124
    return mpHeapMalloc->Malloc(static_cast<s32>(anSize), static_cast<s32>(auAlign));
}

// X360 off_82011E00[2] / [3] -- both slots hold 0x82C296C8, a one-instruction `li r3,1; blr`
// that IDA attributes to CgsSound::Playback::Content::DoOnPostLoad because the linker ICF-folded
// every `return 1` in the image onto one address. That fold is what makes the two slots equal;
// the bodies below are what the fold was made of. This allocator is not reference-counted.
s32 IInternalAllocator::AddRef()
{
    return 1;
}

s32 IInternalAllocator::Release()
{
    return 1;
}
}

// X360 0x82289810 vtable-slot construction, inlined into ParticleModule::Prepare @0x8229BEA0:
// the two vtable pointers (off_82011E14 / off_82011E00) plus `dword_82FAD284 = *(module+568)`,
// i.e. mpHeapMalloc. The vtable stores are the compiler's; the member store is the ctor body.
namespace BrnParticle
{
    IInternalAllocator::IInternalAllocator(CgsMemory::HeapMalloc* lpHeapMalloc)
        : mpHeapMalloc(lpHeapMalloc)
    {
    }
}
