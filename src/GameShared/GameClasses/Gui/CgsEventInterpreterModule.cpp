#include "CgsEventInterpreterModule.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::EventInterpreterModule::sMapEntry::HashTableElement::HashTableElement
//
// Seeds the ten hash buckets with the empty-bucket sentinel. The compiler
// strength-reduced the fill into a pointer loop; it is re-rolled here.

namespace CgsGui
{
EventInterpreterModule::HashTableElement::HashTableElement()
{
    for (int i = 0; i < 10; ++i)
    {
        mBuckets[i] = 0xA00000000ull;
    }
}
}
