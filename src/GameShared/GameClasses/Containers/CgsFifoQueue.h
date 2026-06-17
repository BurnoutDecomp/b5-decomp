#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// FifoQueue<T,N> -- a fixed-capacity, single-producer FIFO ring buffer. Shape
// (member names/types/order, method set + signatures) is authoritative from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/CgsFifoQueue.h):
//     T       maData[N];   // CgsFifoQueue.h:58   (+0x00)
//     int32_t miReadPos;   // CgsFifoQueue.h:59
//     int32_t miWritePos;  // CgsFifoQueue.h:60
//     int32_t miLength;    // CgsFifoQueue.h:61
//
// Matches the X360 Push pseudocode for the <float,10> instantiation exactly: with
// float maData[10] occupying +0x00..+0x27 (40 bytes), miReadPos@+40, miWritePos@+44,
// miLength@+48. Push reads/writes the slot at miWritePos (`4 * miWritePos + maData`),
// advances+wraps miWritePos at N, bumps miLength, refusing (return false) once
// miLength >= N. DWARF spells Push's return as bool; the X360 renders it as int 0/1.
//
// Spelled as the unqualified FifoQueue<T,N> to match the committed Array<T,N> container
// convention (the DWARF spells these CgsContainers::FifoQueue but the project's committed
// containers live at global scope).
template <typename T, s32 N>
class FifoQueue
{
public:
    // Bring the queue to its empty-but-usable state (read/write cursors and length all 0).
    void Construct()
    {
        Clear();
    }

    // Enqueue one element at the write cursor; advance+wrap the cursor and grow the length.
    // Returns false (and stores nothing) when the queue is already full. X360 0x8235E7C0:
    // the full check is the `miLength >= N` test, and the soft refusal returns 0.
    bool Push(const T* lpItem)
    {
        if (miLength >= N)
        {
            return false;
        }
        maData[miWritePos] = *lpItem;
        ++miWritePos;
        ++miLength;
        if (miWritePos >= N)
        {
            miWritePos = 0;
        }
        return true;
    }

    // Dequeue one element into *lpItem from the read cursor; advance+wrap it and shrink the
    // length. Returns false (and writes nothing) when empty. (Reconstructed from layout.)
    bool Pop(T* lpItem)
    {
        if (miLength <= 0)
        {
            return false;
        }
        *lpItem = maData[miReadPos];
        ++miReadPos;
        --miLength;
        if (miReadPos >= N)
        {
            miReadPos = 0;
        }
        return true;
    }

    // Copy the front element into *lpItem without removing it. Returns false when empty.
    // (Reconstructed from layout.)
    bool Peek(T* lpItem) const
    {
        if (miLength <= 0)
        {
            return false;
        }
        *lpItem = maData[miReadPos];
        return true;
    }

    // Number of queued elements.
    s32 GetLength() const { return miLength; }

    // Fixed capacity of the queue.
    s32 GetMaxLength() const { return N; }

    // Drop all queued elements (reset cursors and length to empty).
    void Clear()
    {
        miReadPos  = 0;
        miWritePos = 0;
        miLength   = 0;
    }

private:
    T   maData[N];      // CgsFifoQueue.h:58  (+0x00)  inline element ring buffer
    s32 miReadPos;      // CgsFifoQueue.h:59  read cursor
    s32 miWritePos;     // CgsFifoQueue.h:60  write cursor (the one Push advances/wraps)
    s32 miLength;       // CgsFifoQueue.h:61  live element count (full when == N)
};
