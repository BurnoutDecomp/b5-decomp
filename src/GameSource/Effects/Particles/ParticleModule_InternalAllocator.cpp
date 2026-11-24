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
// The secondary IAllocator Free adjustor at 0x82289898 subtracts 4 from
// `this` and branches to the primary Free implementation at 0x82289888.
// The deleting-destructor adjustor follows the same convention. The compiler
// emits both adjustors from the allocator base classes and these overrides.
// ============================================================================

namespace BrnParticle
{
    // The out-of-line destructor anchors both vtables. The module owns mpHeapMalloc.
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

// X360 0x82289810: the name/flags overload. The primary vtable off_82011E14 is
// {dtor 0x822898A0, 0x82289820, 0x82289810, Free 0x82289888}; MSVC lays the ICoreAllocator::Alloc
// overload group out in reverse declaration order, so slot 2 (0x82289810) is this 3-arg form:
// `lwz r3,8(r3); li r5,0x10; b HeapMalloc::Malloc`. The tagged form reached through the IAllocator
// base is the unexported 0x82289878 (`lwz r3,4(r3)`, this = the +4 sub-object).
void* IInternalAllocator::Alloc(size_t anSize, const char* /*apName*/, u32 /*auFlags*/)
{
    return mpHeapMalloc->Malloc(static_cast<s32>(anSize), KI_INTERNAL_ALLOC_ALIGNMENT);
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
