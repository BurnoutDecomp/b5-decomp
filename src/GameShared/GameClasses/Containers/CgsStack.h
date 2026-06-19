#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (used-before-Construct / !IsEmpty / !IsFull invariants)

// CgsContainers::Stack<Type, N>
// ----------------------------------------------------------------
// A fixed-capacity LIFO stack with embedded inline storage (Type maData[N]) and a
// live count. The reusable generic the project's fixed-capacity-container family uses
// for bounded stacks (ICE::ActionQueue : public Stack<ICE::ActionRef,20>, plus the
// other instances the DWARF attests: Stack<BrnParticle::Native::TrailEmitter*,96>,
// Stack<CgsGraphics::Im2dTransform,33>, Stack<u16,2>, ...).
//
// SHAPE (DecFIGS DWARF, GameShared/GameClasses/Containers/CgsStack.h, struct
// CgsContainers::Stack<Type,N>):
//     Type    maData[N];   // CgsStack.h:81   (+0x00)
//     s32     miLength;     // CgsStack.h:82   (+N*sizeof(Type))
// Method set attested by the DWARF: Stack()/Construct/Push/Grow/Pop/Peek/GetLength/
//   IsFull/IsEmpty/Contains/GetMaxLength/Clear/operator[](int)[const].
//
// SENTINEL / ASSERT MODEL (from the X360 asm for the <ICE::ActionRef,20> instance):
//   An UNINITIALISED Stack has miLength == 0x7FFFFFFF (INT_MAX). Construct()/Clear()
//   set it to 0. Every mutating/observing op first asserts the stack was constructed
//   (miLength != 0x7FFFFFFF, "Stack used before Construct/Clear was called"). Pop/Peek
//   additionally assert !IsEmpty() (miLength != 0); Push additionally asserts !IsFull()
//   (miLength != N). The X360 inlines IsEmpty/IsFull into each caller, so the
//   constructed-check fires twice per Pop/Peek/Push in the asm (once direct, once from
//   the inlined IsEmpty/IsFull); collapsing them into the single CGS_ASSERT here is
//   semantically identical. The baked CgsStack.h path/line in the original FireAssert
//   calls is dropped (CGS_ASSERT supplies __FILE__/__LINE__).
//
// For Stack<ICE::ActionRef,20>: ActionRef is 8 bytes, so maData[20] occupies +0x00..+0x9F
// (160 bytes) and miLength lands at +0xA0 (160) -- exactly the `*(this + 160)` / `result[40]`
// the X360 pseudocode reads. Push copies the 8-byte element word-by-word (mID, mData), which
// the generic `maData[miLength] = *lpEntry` reproduces.
//
// The element parameter is `class Type` and the non-type parameter is `s32 N`, matching the
// original template signature (the DWARF spells instances with literal extents, e.g.
// Stack<ICE::ActionRef,20>). Project scalar s32 stands in for the original int32_t (same width).

namespace CgsContainers
{

const s32 KI_STACK_UNCONSTRUCTED = 0x7FFFFFFF;   // INT_MAX sentinel: miLength of a not-yet-Construct/Clear'd Stack.

template <class Type, s32 N>
struct Stack
{
    // Bring the stack to its empty-but-usable state. Construct and Clear are the two
    // entry points that clear the unconstructed sentinel (X360: both set miLength = 0).
    void Construct() { miLength = 0; }
    void Clear()     { miLength = 0; }

    // X360 0x825313D0 (Push, <ICE::ActionRef,20>). Assert constructed + !full, then store
    // the element at the top slot and grow the count.
    void Push(const Type& lrEntry)
    {
        CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
        CGS_ASSERT(miLength != N, "!IsFull()");
        maData[miLength] = lrEntry;
        ++miLength;
    }

    // X360 0x825314A0 (Pop, <ICE::ActionRef,20>). Assert constructed + !empty, then drop
    // the top element (count only; the slot is left as-is).
    void Pop()
    {
        CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
        CGS_ASSERT(miLength != 0, "!IsEmpty()");
        --miLength;
    }

    // X360 0x82531550 (Peek, <ICE::ActionRef,20>). Assert constructed + !empty, then return
    // the top element (`8 * miLength + this - 8` == &maData[miLength - 1]).
    const Type& Peek() const
    {
        CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
        CGS_ASSERT(miLength != 0, "!IsEmpty()");
        return maData[miLength - 1];
    }

    // X360 0x8252D5E8 (IsFull, <ICE::ActionRef,20>). Assert constructed, then return
    // miLength == N (the asm computes it branchlessly as cntlzw(miLength - 20) >> 5).
    bool IsFull() const
    {
        CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
        return miLength == N;
    }

    bool IsEmpty() const     { return miLength == 0; }
    s32  GetLength() const   { return miLength; }
    s32  GetMaxLength() const { return N; }

    // Declared-only: not part of this TU (other instances / other call sites supply bodies).
    Type*       Grow();
    bool        Contains(const Type& lrEntry) const;
    Type&       operator[](s32 liIndex);
    const Type& operator[](s32 liIndex) const;

    Type maData[N];
    s32  miLength;
};

} // namespace CgsContainers
