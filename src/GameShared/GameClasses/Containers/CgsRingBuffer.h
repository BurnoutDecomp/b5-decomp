#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (operator[] range / full-buffer invariant)

// CgsContainers::RingBuffer<Type> / FixedRingBuffer<Type, Length>
// ----------------------------------------------------------------
// A circular FIFO over a caller-owned element buffer, plus a fixed-stride variant
// that embeds its own inline storage (Type maData[Length]) and self-attaches it.
//
// SHAPE (DecFIGS DWARF, GameShared/GameClasses/Containers/CgsRingBuffer.h, struct
// CgsContainers::RingBuffer<Type> + FixedRingBuffer<Type,N> : public RingBuffer<Type>):
//   RingBuffer<Type>:        Type* mpData; s32 miMaxLength, miReadPos, miWritePos, miLength;
//   FixedRingBuffer<Type,N>: (bases RingBuffer<Type>) + Type maData[N];
// Methods attested by the DWARF: RingBuffer Construct/Push/Grow/Pop/Peek/GetLength/
//   GetMaxLength/GetSpace/IsFull/Clear/operator[](const&)/operator[](&); FixedRingBuffer Construct().
//
// BODY (grounded in the DWARF method set + the X360 asm of the RingBuffer using sites):
// the read/write-position wrap arithmetic, the full-buffer read-pos advance, and the
// operator[] (miReadPos + index) % miMaxLength addressing are reproduced exactly. The
// two original assert sites (PrintStringed + `tw 31,1,1`) are ported to CGS_ASSERT.
//
// StuntModeScoring embeds FixedRingBuffer<Vector3,256> (mRecentJumpSet) and
// FixedRingBuffer<u16,64> (mRecentPropSet) BY VALUE; its bodies call Construct (in
// StuntModeScoring::Construct), Clear (ClearData/EndCombo) and Push + operator[]
// (UpdateBufferedScore). This generic header covers every instantiation; the embedders
// only need the right member order/types and the named methods, which this provides.
//
// The non-type parameter is a plain `int Length` and the element parameter is `class Type`,
// matching the original template signature; the DWARF spells the instances with literal
// extents (e.g. FixedRingBuffer<rw::math::vpu::Vector3,256>). Project scalar types (s32/u32)
// stand in for the original int32_t/uint32_t (same widths).

namespace CgsContainers
{

template <class Type>
class RingBuffer
{
public:
    // Attach a caller-owned element buffer of liBufferLength slots, then reset to empty.
    void Construct(Type* lpBuffer, s32 liBufferLength)
    {
        mpData      = lpBuffer;
        miMaxLength = liBufferLength;
        Clear();
    }

    // Append one entry at the write position, advancing it with wrap-around. When the
    // buffer is already full the oldest entry is overwritten and the read position is
    // advanced so the window stays at miMaxLength (read pos must then equal write pos).
    void Push(const Type* lpEntry)
    {
        mpData[miWritePos] = *lpEntry;
        miWritePos++;
        miLength++;

        if (miWritePos >= miMaxLength)
        {
            miWritePos = 0;
        }

        if (miLength > miMaxLength)
        {
            miLength  = miMaxLength;
            miReadPos = (miReadPos + 1) % miMaxLength;
            CGS_ASSERT(miReadPos == miWritePos, "Read pos should equal write pos if buffer is full");
        }
    }

    // Pop the oldest entry (optionally copying it out via lpEntry); false when empty.
    bool Pop(Type* lpEntry)
    {
        if (miLength <= 0)
        {
            return false;
        }

        if (lpEntry != 0)
        {
            *lpEntry = mpData[miReadPos];
        }
        miReadPos++;
        miLength--;

        if (miReadPos >= miMaxLength)
        {
            miReadPos = 0;
        }
        return true;
    }

    // Copy the oldest entry out without removing it; false when empty.
    bool Peek(Type* lpEntry) const
    {
        if (miLength <= 0)
        {
            return false;
        }

        *lpEntry = mpData[miReadPos];
        return true;
    }

    s32 GetLength() const    { return miLength; }
    s32 GetMaxLength() const { return miMaxLength; }
    s32 GetSpace() const     { return miMaxLength - miLength; }
    bool IsFull() const      { return GetSpace() == 0; }

    // Reset to empty (positions and live count to zero); the inline buffer is left as-is.
    void Clear()
    {
        miReadPos  = 0;
        miWritePos = 0;
        miLength   = 0;
    }

    // Indexed read of the live window: 0 == oldest, GetLength()-1 == newest.
    const Type& operator[](u32 luIndex) const
    {
        CGS_ASSERT(luIndex < static_cast<u32>(miLength), "Attempt to access element outside range");
        return mpData[(miReadPos + luIndex) % miMaxLength];
    }

    Type& operator[](u32 luIndex)
    {
        CGS_ASSERT(luIndex < static_cast<u32>(miLength), "Attempt to access element outside range");
        return mpData[(miReadPos + luIndex) % miMaxLength];
    }

private:
    Type* mpData;
    s32   miMaxLength;
    s32   miReadPos;
    s32   miWritePos;
    s32   miLength;
};

template <class Type, int Length>
class FixedRingBuffer : public RingBuffer<Type>
{
public:
    // Attach the embedded inline buffer and reset to empty. The base RingBuffer<Type>
    // does the actual position/count reset via its Construct.
    void Construct()
    {
        RingBuffer<Type>::Construct(maData, Length);
    }

private:
    Type maData[Length];
};

} // namespace CgsContainers
