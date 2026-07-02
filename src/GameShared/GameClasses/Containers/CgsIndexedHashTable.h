#pragma once

#include "types.hpp"

// CgsSceneManager-style indexed hash table: BINS bucket heads chaining through a
// caller-owned element array by index (the value type doubles as the index
// type). Recovered from the DecFIGS DWARF member declarations on
// CgsSceneManager::EntityManager (IndexedHashTable<K,V,541/509> +
// IndexedHashTableElement arrays) and the X360 EntityManager::Prepare
// @0x828C5FEC.., whose inlined Clear pins the bucket layout:
//   { u32 muCount @+0; Element* mpElements @+4; u16 mu16First @+8 = -1;
//     u16 mu16Last @+10 = -1 }  (12-byte stride)
// with a one-byte constructed flag after the bucket array.
//
// FLAG: only Clear is attested (this TU); the ELEMENT's field split past its
// key/value (the two chain indices) is the natural doubly-linked reading of the
// attested 12/24-byte element strides and the buckets' first/last pair -- the
// lookup/insert TUs pin it when they land.
namespace CgsContainers
{
    template <typename TKey, typename TValue>
    struct IndexedHashTableElement
    {
        TKey   mKey;
        TValue mValue;
        TValue mNext;   // FLAG: chain indices (see banner)
        TValue mPrev;
    };

    template <typename TKey, typename TValue, u32 BINS>
    class IndexedHashTable
    {
    public:
        typedef IndexedHashTableElement<TKey, TValue> Element;

        // The inlined Clear (EntityManager::Prepare @0x828C6010/@0x828C6050):
        // every bucket empties onto the shared element array and the table is
        // marked constructed.
        void Clear(Element* lpElements)
        {
            for (u32 luBin = 0; luBin < BINS; ++luBin)
            {
                maBins[luBin].muCount    = 0;
                maBins[luBin].mpElements = lpElements;
                maBins[luBin].mu16First  = static_cast<u16>(-1);
                maBins[luBin].mu16Last   = static_cast<u16>(-1);
            }
            mbConstructed = true;
        }

    private:
        struct Bin
        {
            u32      muCount;      // +0x00
            Element* mpElements;   // +0x04 (X360; shared element array)
            u16      mu16First;    // +0x08 (-1 == empty)
            u16      mu16Last;     // +0x0A (-1 == empty)
        };

        Bin  maBins[BINS];
        bool mbConstructed;   // the stb 1 after the bucket loop
    };
}
