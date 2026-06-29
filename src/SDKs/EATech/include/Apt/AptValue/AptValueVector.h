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

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager

// ---------------------------------------------------------------------------
// FLAG (un-homed, owned by another TU): the global DOGMA pool that backs the
// operand-stack array. The X360 binary loads it from off_8324D808 in Shutdown.
// Declared here as an extern so Shutdown compiles + links; its definition (and
// initialization) lives in the Apt allocator boot TU.
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptOperandStackPool;   // off_8324D808

// ---------------------------------------------------------------------------
// FLAG: PopAndPush takes a "value producer" -- in the X360 asm `a3` is an
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
            gpAptOperandStackPool->Allocate(4 * nCapacity));
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

    // Shutdown @ 0x82AE14F0 -- free the backing array and clear the vector.
    void Shutdown();

    // shutdown @ 0x82AE15A0 -- identical body to Shutdown (a second symbol the
    // X360 interpreter calls; AptActionInterpreter::shutdown reaches this one).
    void shutdown();

    // GetNumValues @0x82AD5228 / IsVectorFull @0x82ADC130 -- trivial state queries.
    int32_t GetNumValues() const { return mnTop; }
    bool    IsVectorFull() const { return mnTop >= mnCapacity; }

    // RemoveAt @0x82ADBFF0 -- drop the element at nIndex (shift the tail down one,
    // clear the vacated top slot). NOTE: does not Release the removed value (the
    // caller owns it), matching the X360.
    void    RemoveAt(int32_t nIndex);

    int32_t     mnTop;        // +0x00
    int32_t     mnCapacity;   // +0x04
    AptValue**  mppItems;     // +0x08
};
