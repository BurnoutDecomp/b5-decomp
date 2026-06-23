#ifndef CGS_EVENT_INTERPRETER_MODULE_H
#define CGS_EVENT_INTERPRETER_MODULE_H

#include "types.hpp"

namespace CgsGui
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E1790.
// The hash-table element of the event interpreter's entry map. Its constructor
// seeds ten buckets with the empty-bucket sentinel (key 0, slot count 10).
class EventInterpreterModule
{
public:
    struct HashTableElement
    {
        HashTableElement();

        u8  mPad0[16];      // header preceding the bucket array
        u64 mBuckets[10];   // each seeded with 0xA00000000
    };

    // The map entry's event-subscription mask: a fixed 600-bit bit array (ten 64-bit
    // words) recording which event indices this entry is registered for.
    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8284E870.
    struct sMapEntry
    {
        static const s32 KI_NUM_BITS  = 600;    // CgsBitArray.h:222 bound (< 0x258)
        static const s32 KI_NUM_WORDS = 10;     // ten 64-bit words back the 600 bits

        // @ 0x8284E870 - zero the bit array, then for each of liCount indices in
        // lpaIndices assert the index is < 600 (CgsBitArray.h:222) and set that bit.
        sMapEntry(const u32* lpaIndices, s32 liCount);

        u64 maBits[KI_NUM_WORDS];   // +0x00 .. +0x4F (80 bytes)
    };
};
}

#endif
