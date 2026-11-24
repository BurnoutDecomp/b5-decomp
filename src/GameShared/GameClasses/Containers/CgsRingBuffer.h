#pragma once
#line 3 "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsRingBuffer.h"
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"      // CGS_ASSERT(_STREAM); kept for its users
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // Push's streamed assert

// CgsContainers::RingBuffer<Type> -- a ring over a caller-supplied buffer: Push overwrites the
// oldest entry once full; FixedRingBuffer<Type, Length> supplies an inline buffer. Member set,
// names and lines from the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/
// Containers/CgsRingBuffer.h: namespace h:24, mpData..miLength h:83-87, FixedRingBuffer::maData
// h:106, FixedRingBuffer::Construct h:265); the header's shape and Push's body from the LegacySrc
// copy; ARTIST confirms the bodies and the assert lines it bakes into every inlined copy (file
// string @0x82018438): Push 137 (streamed "Read pos should equal write pos if buffer is
// full\n", e.g. 0x822AD1A0), operator[] const 249 (e.g. TrafficManager::FindHashForFrame
// @0x82552490), operator[] 258 (e.g. 0x822AD4E8).
//
//
//
//
//
//
//
//
namespace CgsContainers
{














template <class Type>
class RingBuffer
{
public:
    // Construct h:113: adopt the buffer and clear.
    inline void Construct(Type* lpBuffer, s32 liBufferLength);

    // Push h:122: append, overwriting the oldest entry when full.
    inline void Push(const Type* lpEntry);

    // Pop h:170 / Peek h:195: the oldest entry; false when empty.
    inline bool Pop(Type* lpEntry);
    inline bool Peek(Type* lpEntry) const;

    // GetLength h:210, GetMaxLength h:217, GetSpace h:224, IsFull h:231, Clear h:238.
    inline s32 GetLength() const;
    inline s32 GetMaxLength() const;
    inline s32 GetSpace() const;
    inline bool IsFull() const;
    inline void Clear();

    // operator[] h:247 (const) / h:256: the entry luIndex places after the oldest.
    inline const Type& operator[](u32 luIndex) const;
    inline Type& operator[](u32 luIndex);


















private:
    Type* mpData;           // DWARF h:83
    s32   miMaxLength;      // DWARF h:84
    s32   miReadPos;        // DWARF h:85
    s32   miWritePos;       // DWARF h:86
    s32   miLength;         // DWARF h:87
};

template <class Type, int Length>
class FixedRingBuffer : public RingBuffer<Type>
{
public:
    // Construct h:265: run the base Construct over the inline buffer.
    inline void Construct();

private:








    Type maData[Length];    // DWARF h:106
};



template <class Type>
inline void
RingBuffer<Type>::Construct(Type* lpBuffer, s32 liBufferLength)
{
    mpData      = lpBuffer;
    miMaxLength = liBufferLength;
    Clear();
}

template <class Type>
inline void
RingBuffer<Type>::Push(const Type* lpEntry)
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
        CGS_ASSERT_STREAM(miReadPos == miWritePos, "Read pos should equal write pos if buffer is full\n");
    }
}




























template <class Type>
inline bool
RingBuffer<Type>::Pop(Type* lpEntry)
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



template <class Type>
inline bool
RingBuffer<Type>::Peek(Type* lpEntry) const
{
    if (miLength <= 0)
    {
        return false;
    }

    *lpEntry = mpData[miReadPos];
    return true;
}



template <class Type>
inline s32
RingBuffer<Type>::GetLength() const
{
    return miLength;
}

template <class Type>
inline s32
RingBuffer<Type>::GetMaxLength() const
{
    return miMaxLength;
}

template <class Type>
inline s32
RingBuffer<Type>::GetSpace() const
{
    return miMaxLength - miLength;
}

template <class Type>
inline bool
RingBuffer<Type>::IsFull() const
{
    return GetSpace() == 0;
}

template <class Type>
inline void
RingBuffer<Type>::Clear()
{
    miReadPos  = 0;
    miWritePos = 0;
    miLength   = 0;
}

template <class Type>
inline const Type&
RingBuffer<Type>::operator[](u32 luIndex) const
{
    CGS_ASSERT(luIndex < static_cast<u32>(miLength), "Attempt to access element outside range");
    return mpData[(miReadPos + luIndex) % miMaxLength];
}


template <class Type>
inline Type&
RingBuffer<Type>::operator[](u32 luIndex)
{
    CGS_ASSERT(luIndex < static_cast<u32>(miLength), "Attempt to access element outside range");
    return mpData[(miReadPos + luIndex) % miMaxLength];
}


template <class Type, int Length>
inline void
FixedRingBuffer<Type, Length>::Construct()
{
    RingBuffer<Type>::Construct(maData, Length);
}


} // namespace CgsContainers
