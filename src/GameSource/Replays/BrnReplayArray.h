#pragma once

// BrnReplays::BrnReplayArray<T, N, TAreDifferent> -- the fixed-capacity, byte-counted array the
// replay serialisers use for their per-frame record buffers. Distinct from
// CgsContainers::Array<T,N> (CgsArray.h): the live-element count here is a single BYTE that sits
// AFTER the inline element buffer (at offset N*sizeof(T)), the bounds-check accessor streams a
// different message, and there is no "unconstructed" -1 sentinel.
//
// The generic bodies below are reconstructed store-for-store from the surviving per-T
// instantiations. Every instantiation shares the same shapes:
//
//   operator[](u8 index)
//     assert index < muLength, streaming "Array index out of bounds: <index> Length: <muLength>\n"
//     via the debug string builder, then return &maElements[index] (stride sizeof(T)).
//     Instantiations read: T=u16 N=254 (muLength @ +0x1FC), T=u32 N=4 (@ +0x10),
//     T=PropLoadedZoneRecord N=9 (@ +0x5E8). The streamed message collapses to one CGS_ASSERT
//     per project convention.
//
//   PushBack(const T&)
//     assert muLength < MaxLength (254, the u8 count domain), "muLength < MaxLength", store the
//     element at &maElements[muLength] and post-increment the count byte. Returns the slot
//     (every caller ignores it).
//
// muLength is a u8; MaxLength is the fixed 254 the u8 count domain allows. N is the element
// capacity, attested per instantiation by muLength's byte offset (== N*sizeof(T)).

// ===== Read / Write : the delta serialisation pair ==============================================
// The prop serialiser records/plays these arrays delta-encoded against the previous frame.
// Wire format (non-key-frame path):
//   [u8 muLength][u8 luNumRecords]( ReplayArrayUpdateRecord<T> x luNumRecords )
// On a key frame the whole element buffer is written/read verbatim (muLength*sizeof(T) bytes).
// The changed set is discovered by comparing the live array against the previous frame's array
// (an inlined CgsContainers::BitArray<N> walked low-to-high via GetFirstNonZeroBit /
// GetNextNonZeroBit); appended tail elements (indices [prevLength, muLength)) are emitted after.
// Bodies live out-of-line in BrnReplayArray.cpp (they pull in BaseSerialiser + CgsBitArray),
// with one explicit instantiation there per (T, N) pair the prop frame drives.
//
// ⭐ THE UPDATE RECORD IS PER-T, NOT A FIXED 8-BYTE {index,u32} PAIR. An earlier revision of this
// header modelled it as `{u8 index; u8 pad[3]; u32 value;}` and had Read do
// `maElements[i] = static_cast<T>(record.muValue)`. That is the <u32,4> shape ONLY; it does not
// compile for a struct element and it serialises the WRONG WIRE FORMAT for the u16 arrays. Each
// instantiation's own record size and payload offset were measured, and every one of them is
// exactly `struct { u8 mu8Index; T muValue; }` at natural alignment:
//     T=u32   (align 4)   record 8    value @ +4
//     T=u16   (align 2)   record 4    value @ +2
//     T=Vector3 / Quaternion (align 16) record 32  value @ +16
//     T=PropLoadedZoneRecord (align 8, sizeof 168) record 176  value @ +8  (168-byte payload copy)
// so the template below reproduces all four without a single hand-written stride.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (bounds / capacity)

namespace BrnReplays
{
    class BaseSerialiser;

    // The "has this element changed since the previous frame?" policy Write's changed-bit scan
    // runs. It is the array's third template parameter, defaulted -- the mangled instantiation
    // names in the shipped image spell it `DefaultAreDifferentFunctor<T>`, and every surviving
    // instantiation uses that default.
    //
    // The primary template is an exact inequality; that is what the u16 / u32 instantiations do.
    // The float-element instantiations (Vector3, Quaternion) compare with a tolerance instead --
    // those specialisations live in BrnReplayArray.cpp, next to the only code that instantiates
    // them, so this header keeps no dependency on the RenderWare math types.
    template <typename T>
    struct DefaultAreDifferentFunctor
    {
        static bool AreDifferent(const T& lrA, const T& lrB) { return !(lrA == lrB); }
    };

    // One delta record on the wire: a 1-byte element index followed, at T's natural alignment, by
    // the element value. sizeof(ReplayArrayUpdateRecord<T>) IS the console's per-T record stride
    // for every instantiation the prop frame drives (see the banner table above); Read/Write move
    // exactly that many bytes per changed/appended element.
    template <typename T>
    struct ReplayArrayUpdateRecord
    {
        u8 mu8Index;   // @0x00 element index
        T  muValue;    // @alignof(T) element value
    };

    template <typename T, u8 N, typename TAreDifferent = DefaultAreDifferentFunctor<T> >
    struct BrnReplayArray
    {
        // Fixed capacity the u8 length can index; the append guard tests against this.
        static const u8 KU_MAX_LENGTH = 254;

        T   maElements[N];   // @0x0000            inline element buffer
        u8  muLength;        // @N*sizeof(T)       live element count (byte, follows the buffer)

        // Checked indexed accessor. Asserts the index is in range then returns
        // &maElements[luIndex]; the accessor arithmetic is sizeof(T)*index + base.
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

        // Append one element. Asserts there is room then stores the element at
        // maElements[muLength] and post-increments the count.
        T& PushBack(const T& lrElement)
        {
            CGS_ASSERT(muLength < KU_MAX_LENGTH, "muLength < MaxLength");
            T& lrSlot = maElements[muLength];
            lrSlot = lrElement;
            ++muLength;
            return lrSlot;
        }

        // Play back the array from lpSerialiser. See BrnReplayArray.cpp.
        void Read(BaseSerialiser* lpSerialiser);

        // Record the array to lpSerialiser, delta-encoded against the previous frame's
        // (lpPrevData, lu8PrevLength). See BrnReplayArray.cpp.
        void Write(BaseSerialiser* lpSerialiser, const T* lpPrevData, u8 lu8PrevLength);
    };
}
