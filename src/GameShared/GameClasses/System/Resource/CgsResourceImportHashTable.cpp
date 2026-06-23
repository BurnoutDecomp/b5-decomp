#include "GameShared/GameClasses/Containers/CgsLinearSOAHashTable.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsContainers::LinearSOAHashTable<CgsResource::ImportHashTableValue>::Initialise
//   reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828EA3C8 (CgsLinearSOAHashTable.h:167).
//
// This is the import-index map the resource ScratchPool uses to dedupe imported resources during
// a defrag pass (the "8-byte-stride id map" referenced in CgsResourceScratchPool.h). The X360
// build emits one Initialise per using-TU; this is the CgsResource::ImportHashTableValue
// instantiation. Definition placed here (not inline) so the SOA table template has a concrete
// .cpp home, matching the X360's per-instantiation function emission.
//
// X360 body (store-for-store):
//   std  length, this+0x00              ; muLength  = luLength (64-bit)
//   stw  keys,   this+0x10              ; mpKeys    = lpKeys
//   stw  values, this+0x14              ; mpValues  = lpValues
//   std  -1,     this+0x08              ; mEmptyKey = 0xFFFFFFFFFFFFFFFF
//   loop slot in [0, muLength):  *(keys + 8*slot) = mEmptyKey   ; clear every key slot
// preceded by the power-of-2 length assert (the CGS_ASSERT below).

namespace CgsResource
{
    // The value carried in each import-hash-table slot. The X360 Initialise only touches the
    // 64-bit key array (it clears keys to the empty sentinel); the value array base is recorded
    // but not initialised here, so its concrete contents are opaque to this TU. Modelled as a
    // 16-byte import record (id + resolved pointer + offset) at the asm-observed value stride.
    struct ImportHashTableValue
    {
        u64   muImportId;       // import resource id (key mirror)
        void* mpResolved;       // resolved import pointer
        u32   muOffset;         // byte offset of the import pointer within the resource
        u32   muPad;            // alignment
    };
}

namespace CgsContainers
{
    template <typename ValueType>
    void LinearSOAHashTable<ValueType>::Initialise(u64* lpKeys, ValueType* lpValues, u64 luLength)
    {
        CGS_ASSERT(luLength != 0 && ((luLength - 1) & luLength) == 0,
                   "Length of hash table must be more than 0 and a power of 2");

        muLength  = luLength;
        mpKeys    = lpKeys;
        mpValues  = lpValues;
        mEmptyKey = KU_EMPTY_KEY;

        for (u64 luSlot = 0; luSlot < muLength; ++luSlot)
            mpKeys[luSlot] = mEmptyKey;
    }

    // Explicit instantiation: emit Initialise for the import value type (X360 0x828EA3C8).
    template void LinearSOAHashTable<CgsResource::ImportHashTableValue>::Initialise(
        u64*, CgsResource::ImportHashTableValue*, u64);
}
