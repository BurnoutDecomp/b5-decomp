#pragma once

// BrnReplays::BrnReplayArray<T, N> -- the fixed-capacity, byte-counted array the replay
// serialisers use for their per-frame record buffers. Distinct from CgsContainers::Array<T,N>
// (CgsArray.h): the live-element count here is a single BYTE that sits AFTER the inline
// element buffer (at offset N*sizeof(T)), the bounds-check accessor streams a different
// message, and there is no "unconstructed" -1 sentinel.
//
// No DWARF and no Feb-2007 source were recovered for this template, so the generic bodies
// below are reconstructed store-for-store from the X360 instantiations that DO survive
// (all in BURNOUT_X360_ARTIST.XEX). Every instantiation shares the same two shapes:
//
//   operator[](u8 index)   (BrnReplayArray.h:93)
//     X360: `lbz r11, N*sizeof(T)(this); cmplw index, r11; blt ok` -- assert index < muLength,
//     streaming "Array index out of bounds: <index> Length: <muLength>\n" via the debug
//     string builder (off_82000D00/off_82000D08 StrStream vtable + sub_821F0E50 int->string),
//     then return `sizeof(T)*index + this` == &maElements[index]. The streamed message
//     collapses to one CGS_ASSERT per project convention.
//       DefaultAreDiffere       @0x822AA638  T=u16   N=254  (muLength @ 0x1FC == 254*2)
//       DefaultAreDifferentFunc @0x822AA250  T=u32   N=4    (muLength @ 0x10  == 4*4)
//       PropSerialiserF         @0x822AA058  T=Slot  N=9    (muLength @ 0x5E8 == 9*168)
//     (The ledger keys are the IDA-truncated instantiation names, not distinct methods.)
//
//   PushBack(const T&)     (BrnReplayArray.h:98)
//     X360 @0x822AA5B8 (T=Vec16, N=254): `lbz r11, 0xFE0(this); cmplwi r11, 0xFE; blt ok`
//     -- assert muLength < MaxLength(254=0xFE, the u8 domain), "muLength < MaxLength", then
//     `stvx128` the 16-byte element to &maElements[muLength] (`rotlwi r,muLength,4` == *16
//     stride) and `stb muLength+1` post-increment. Returns `this` (the caller ignores it).
//
// muLength is a u8; MaxLength is the fixed 254 the u8 count domain allows. N is the element
// capacity, attested per instantiation by muLength's byte offset (== N*sizeof(T)).

// ===== Read/Write (delta serialisation) added for the <u32,4> instantiation =====
//   Read  @0x826512A8  (T=u32, N=4)   Write @0x82654118  (T=u32, N=4)
// The prop serialiser records/plays these arrays delta-encoded against the previous frame.
// Wire format (non-key-frame path):
//   [u8 muLength][u8 luNumRecords]( [u8 index][pad*3][u32 value] x luNumRecords )
// On a key frame the whole element buffer is written/read verbatim (muLength*sizeof(T) bytes).
// The changed set is discovered by comparing the live array against the previous frame's array
// (an inlined CgsContainers::BitArray<N> walked low-to-high via GetFirstNonZeroBit /
// GetNextNonZeroBit); appended tail elements (indices [prevLength, muLength)) are emitted after.
// Bodies live out-of-line in BrnReplayArray.cpp (they pull in BaseSerialiser + CgsBitArray),
// with an explicit template instantiation for <u32,4> there.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (bounds / capacity)

namespace BrnReplays
{
    class BaseSerialiser;

    template <typename T, u8 N>
    struct BrnReplayArray
    {
        // Fixed capacity the u8 length can index; the append guard tests against this.
        static const u8 KU_MAX_LENGTH = 254;

        T   maElements[N];   // @0x0000            inline element buffer
        u8  muLength;        // @N*sizeof(T)       live element count (byte, follows the buffer)

        // Checked indexed accessor (X360 BrnReplayArray.h:93). Asserts the index is in range
        // then returns &maElements[luIndex]; the accessor arithmetic is sizeof(T)*index + base.
        T& operator[](u8 luIndex)
        {
            CGS_ASSERT(luIndex < muLength, "Array index out of bounds: ");
            return maElements[luIndex];
        }

        const T& operator[](u8 luIndex) const
        {
            CGS_ASSERT(luIndex < muLength, "Array index out of bounds: ");
            return maElements[luIndex];
        }

        // Append one element (X360 BrnReplayArray.h:98). Asserts there is room then stores the
        // element at maElements[muLength] and post-increments the count.
        T& PushBack(const T& lrElement)
        {
            CGS_ASSERT(muLength < KU_MAX_LENGTH, "muLength < MaxLength");
            T& lrSlot = maElements[muLength];
            lrSlot = lrElement;
            ++muLength;
            return lrSlot;
        }

        // Play back the array from lpSerialiser (X360 @0x826512A8). See BrnReplayArray.cpp.
        void Read(BaseSerialiser* lpSerialiser);

        // Record the array to lpSerialiser, delta-encoded against the previous frame's
        // (lpPrevData, lu8PrevLength) (X360 @0x82654118). See BrnReplayArray.cpp.
        void Write(BaseSerialiser* lpSerialiser, const T* lpPrevData, u8 lu8PrevLength);
    };

    // One delta record on the wire (X360-attested 8-byte stride): a 1-byte element index at
    // +0x00 followed (4-byte aligned) by the 4-byte element value at +0x04. Read/Write move
    // exactly 8 bytes per changed/appended element.
    struct ReplayArrayUpdateRecord
    {
        u8  mu8Index;    // @0x00 element index
        u8  maPad01[3];  // @0x01 alignment padding (untouched)
        u32 muValue;     // @0x04 element value
    };
}
