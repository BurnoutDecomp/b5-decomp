#pragma once

#include "types.hpp"

// CgsContainers::LinearSOAHashTable<ValueType> - an open-addressed (linear-probed) hash table
// stored as Structure-of-Arrays: a parallel 64-bit key array and a value array, both sized to
// the (power-of-2) table length. An empty/invalid slot is marked by the 64-bit invalid-key
// sentinel (all-ones, 0xFFFFFFFFFFFFFFFF). Recovered from the DecFIGS DWARF
// (CgsLinearSOAHashTable.h); member + method names from the PS3 DecFIGS DWARF (the X360 build
// is stripped but the function @ 0x828EA3C8 == this Initialize, file:line matches).
//
// NAME NOTE (PS3 reconcile): the PS3 DWARF declares this as a TWO-template-param container
// LinearSOAHashTable<Key,Value> (Key fixed here to u64). Ours keeps the single-param form with
// the key type baked to u64 -- a structural simplification, NOT restructured to the 2-param
// PS3 shape. The method (Initialize) + member names (miLength/miInvalidKey/mpKeys/mpValues)
// below DO match the PS3 DWARF.
//
// Initialize(lpKeys, lpValues, luLength) records the two SOA array bases and the length,
// asserts the length is a non-zero power of 2, then clears every key slot to the invalid-key
// sentinel. Faithful to the X360 ARTIST Initialize body (defined in the per-instantiation
// .cpp via explicit template instantiation, mirroring the X360's per-using-TU emission).
//
// LAYOUT (X360-observed store offsets, authoritative):
//   +0x00  miLength     (u64)  table length (power of 2)        [std @0]
//   +0x08  miInvalidKey (u64)  empty-slot sentinel = ~0ull      [std -1 @8]
//   +0x10  mpKeys       (T*)   64-bit key array base            [stw @0x10]
//   +0x14  mpValues     (V*)   value array base                 [stw @0x14]
// The init loop fills miLength 8-byte key slots at mpKeys with miInvalidKey (stdx, 8-byte stride).
namespace CgsContainers
{
    template <typename ValueType>
    struct LinearSOAHashTable
    {
        // The invalid/empty-slot sentinel: a 64-bit all-ones key. A slot whose key equals
        // this is free. (Our constant name; the X360 stores the literal -1 into miInvalidKey.)
        static const u64 KU_EMPTY_KEY = ~0ull;

        // X360 0x828EA3C8 (CgsLinearSOAHashTable.h:167). Definition lives in the .cpp; the
        // template is explicitly instantiated there for each value type the game uses.
        void Initialize(u64* lpKeys, ValueType* lpValues, u64 luLength);

        u64        miLength;     // +0x00
        u64        miInvalidKey; // +0x08
        u64*       mpKeys;       // +0x10
        ValueType* mpValues;     // +0x14
    };
}
