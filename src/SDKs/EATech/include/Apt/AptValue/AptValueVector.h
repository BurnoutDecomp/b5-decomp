#pragma once

// ===========================================================================
// EATech Apt -- AptValueVector (the AptActionInterpreter operand stack).
//
// IDA names this class `AptValue>` -- the trailing '>' is a truncated
// template-argument close; the leak forward-declares it as `class
// AptValueVector;` (AptValue.h:134) but ships no body, and there is no DWARF
// for it. SHAPE here is reconstructed from the X360 ARTIST.XEX pseudocode of
// the three owned methods plus the call sites:
//     AptValueVector::PopAndPush  @ 0x82ADBAB8
//     AptValueVector::SafePop     @ 0x82ADBB58
//     AptValueVector::Shutdown    @ 0x82AE14F0
//
// All three callers are AptActionInterpreter operand-stack manipulators
// (callFunction, _FunctionAptAction*, _createObject, ...), so this is the
// interpreter's value stack: a contiguous, externally-allocated array of
// AptValue* with a live count.
//
// LAYOUT (proven by the asm offsets used in the three bodies):
//   +0x00  int32_t       mnTop        (live element count / stack pointer)
//   +0x04  int32_t       mnCapacity   (allocated slot count; used by Shutdown
//                                       to compute the free size 4*capacity)
//   +0x08  AptValue**    mppItems     (the slot array; *(4*(top-i)+items))
//
// Shutdown frees mppItems through DOGMA_PoolManager::Deallocate on the global
// pool (off_8324D808) using 4*mnCapacity bytes, then zeroes all three members.
//
// This is vendor/SDK code reconstructed in its canonical leak home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptValue, Deallocate) are
// kept verbatim; the reconstructed members follow the mpX/mnX prefixes.
// ===========================================================================

#include <cstddef>   // offsetof (_AssertLayout)
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager

// ---------------------------------------------------------------------------
// The global DOGMA pool that backs the operand-stack array. The X360 binary
// loads it from off_8324D808 in Shutdown. Defined in AptGlobals.cpp; wired to
// the shared DOGMA pool by AptAllocatorInitialize (AptInit.cpp @0x82ADD118).
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptOperandStackPool;   // off_8324D808

// ---------------------------------------------------------------------------
// NOTE: PopAndPush takes a "value producer" -- in the X360 asm `a3` is an
// object whose first virtual (vtbl[0]) is invoked with itself as the receiver
// and yields the AptValue* to push. The producer is constructed by the caller
// (an AptActionInterpreter helper); modelled here as a minimal polymorphic
// interface so the indirect-call ORDER is preserved without fabricating the
// concrete producer body (which is owned by the interpreter TUs).
// ---------------------------------------------------------------------------
class AptValueProducer
{
public:
    virtual AptValue* Produce() = 0;
};

class AptValueVector
{
public:

    // ADDITIVE GROW (FLAGGED, non-breaking): an explicit construct-with-capacity
    // factory mirroring the X360 ctor @ 0x82AE1548 (IDA `AptValue>`):
    //   mnTop=0; mnCapacity=nCap; mppItems=0; mppItems=Allocate(pool, 4*nCap).
    // Provided as a static initialiser (NOT a real constructor) so AptValueVector
    // stays an aggregate -- existing default-constructed uses keep working. Inline
    // (header-only) so embedding owners (AptIntervalTimer::mParams, capacity 6)
    // need no extra link symbol. No layout change.
    static AptValueVector ConstructWithCapacity(int32_t nCapacity)
    {
        AptValueVector lvVec;
        lvVec.mnTop      = 0;
        lvVec.mnCapacity = nCapacity;
        lvVec.mppItems   = nullptr;
        lvVec.mppItems   = static_cast<AptValue**>(
            gpAptOperandStackPool->Allocate(sizeof(AptValue*) * nCapacity));
        return lvVec;
    }

    // PopAndPush @ 0x82ADBAB8 -- if at least nCount values are live, produce a
    // new value, Release the top nCount values, and replace them with the new
    // one (net stack delta = 1 - nCount).
    AptValue* PopAndPush(int32_t nCount, AptValueProducer* pProducer);

    // SafePop @ 0x82ADBB58 -- Release and pop the top nCount values, but only
    // when nCount > 0 and at least nCount values are live.
    void SafePop(int32_t nCount);

    // pop @ 0x82ADBBD0 -- Release and remove the single top value
    // (unconditional; no bounds check). Returns the Release result.
    AptValue* pop();

    // PopValue -- the x64 export's pop-WITHOUT-Release twin of pop() (phase-0
    // signature-parity item): pre-decrement the live top and hand the popped
    // value to the caller, who now owns the reference. No bounds check,
    // matching pop(). Body in AptValueVector.cpp.
    AptValue* PopValue();

    // Shutdown @ 0x82AE14F0 -- free the backing array and clear the vector.
    void Shutdown();

    // shutdown @ 0x82AE15A0 -- identical body to Shutdown (a second symbol the
    // X360 interpreter calls; AptActionInterpreter::shutdown reaches this one).
    void shutdown();

    // -----------------------------------------------------------------------
    // NOTE (layout collision -- two X360 classes share this one home + storage)
    //
    // IDA exports TWO distinct symbol families against this 12-byte
    // {int@0, int@4, AptValue**@8} shape:
    //
    //   (A) `AptValue>::*`  (the operand-stack template instantiation; methods
    //       PopAndPush/SafePop/pop/Shutdown/shutdown above, ctor @0x82AE1548):
    //          +0x00 = live count / top, +0x04 = capacity.
    //
    //   (B) `AptValueVector::*` (the real `AptValueVector` symbols; ctor
    //       @0x82AE32F0, plus GetAt/SetAt/GetNumValues/IsVectorFull/RemoveAt/
    //       ReleaseValues):
    //          +0x00 = capacity, +0x04 = live count / top.   <-- SWAPPED.
    //
    // Both ctors prove this: @0x82AE1548 stores capacity at +4 and 0 at +0;
    // @0x82AE32F0 stores capacity at +0 and 0 at +4. They are genuinely
    // different classes the compiler merged onto identical storage. The home
    // file + the in-tree callers (AptIntervalTimer::mParams, the interpreter
    // TUs, the embed check) were built against family (A), so the members keep
    // the family-(A) names: `mnTop` (+0x00) and `mnCapacity` (+0x04).
    //
    // The family-(B) `AptValueVector::*` bodies below therefore access the
    // PHYSICAL offsets faithfully but through the (A)-named members:
    //     family-(B) capacity  ==  mnTop      (+0x00)
    //     family-(B) live top   ==  mnCapacity (+0x04)
    //     items                 ==  mppItems   (+0x08)
    // Each (B) body restates this so the byte stores match the X360 verbatim.
    // -----------------------------------------------------------------------

    // ctor @0x82AE32F0 (`AptValueVector::AptValueVector`) -- capacity at +0,
    // top=0 at +4, allocate 4*capacity slots into +8. Provided as a static
    // factory (keeps the aggregate) like ConstructWithCapacity, but with the
    // family-(B) field assignment.
    static AptValueVector ConstructAptValueVector(int32_t nCapacity);

    // GetAt @0x82ADBFD0 -- mppItems[nIndex] (no bounds check; raw indexed load).
    // x64 signature parity: non-const (the x64 export's GetAt @0x140838220 takes a
    // mutable this).
    AptValue* GetAt(int32_t nIndex);

    // SetAt @0x82ADBFE0 -- mppItems[nIndex] = pValue (no bounds check).
    void      SetAt(int32_t nIndex, AptValue* pValue);

    // GetNumValues @0x82AD5228 -- the live count (+0x04 == mnCapacity member).
    int32_t GetNumValues() const { return mnCapacity; }

    // IsVectorFull @0x82ADC130 -- full when live-count(+4) >= capacity(+0); i.e.
    // mnCapacity (the +4 top) >= mnTop (the +0 capacity). x64 signature parity:
    // returns int (the export's i32 result), not bool.
    int     IsVectorFull() const { return (mnCapacity >= mnTop) ? 1 : 0; }

    // ReleaseValues @0x82ADCF60 -- pop every live value off the top, Release-ing
    // (or ForceDelete-ing) each; leaves the vector empty. Returns the last
    // Release/ForceDelete result faithfully (X360 returns r3).
    AptValue* ReleaseValues();

    // RemoveAt @0x82ADBFF0 -- drop the element at nIndex (shift the tail down one,
    // clear the vacated top slot). NOTE: does not Release the removed value (the
    // caller owns it), matching the X360. Operates on the +4 (mnCapacity member)
    // live count per family (B).
    void    RemoveAt(int32_t nIndex);

    int32_t     mnTop;        // +0x00
    int32_t     mnCapacity;   // +0x04

    // Layout pinned against the x64 XB1 accessors (never called; member body gives
    // complete-class offsetof context). Family-(B) semantics: capacity@+0, count@+4.
    static void _AssertLayout()
    {
        static_assert(offsetof(AptValueVector, mnTop)      == 0x0, "x64: capacity dword at +0 (IsVectorFull @0x14083B3E0)");
        static_assert(offsetof(AptValueVector, mnCapacity) == 0x4, "x64: live count dword at +4 (GetNumValues @0x140839680)");
        static_assert(offsetof(AptValueVector, mppItems)   == 0x8, "x64: slot array at +8, 8-byte stride (GetAt @0x140838220)");
        static_assert(sizeof(AptValueVector) == 0x10, "natural x64 layout of {i32,i32,ptr}");
    }
    AptValue**  mppItems;     // +0x08
};
