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
};
}

#endif
