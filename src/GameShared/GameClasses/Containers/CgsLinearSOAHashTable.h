#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsContainers::LinearSOAHashTable<ValueType, KeyType> - an open-addressed (linear-probed) hash
// table stored as Structure-of-Arrays: a parallel key array and a value array, both sized to the
// (power-of-2) table length. An empty/invalid slot is marked by the invalid-key sentinel (all-ones).
//
// GENERALISED from the earlier single-param form to the 2-param LinearSOAHashTable<ValueType, KeyType>.
// KeyType defaults to u64 so the already-committed LinearSOAHashTable<CgsResource::ImportHashTableValue>
// usage (Initialize @0x828EA3C8) keeps its exact spelling and byte layout (KeyType=u64:
// miLength@0x00, miInvalidKey@0x08, mpKeys@0x10, mpValues@0x14). The <int,int> instantiation
// (KeyType=int) gives miLength@0, miInvalidKey@4, mpKeys@8, mpValues@0xC -- exactly the
// AddEntry@0x828DF260 / FindEntry@0x828DF3E0 asm-observed offsets/strides (PatchManager path-hash map).
//
// PARAM ORDER: kept as <ValueType, KeyType=u64> (NOT the PS3 DWARF's <Key,Value> order) so the
// committed single-arg usage `LinearSOAHashTable<CgsResource::ImportHashTableValue>` stays valid.
//
// LAYOUT (X360-observed store offsets, authoritative):
//   KeyType=u64:  miLength@0x00, miInvalidKey@0x08, mpKeys@0x10, mpValues@0x14
//   KeyType=int:  miLength@0x00, miInvalidKey@0x04, mpKeys@0x08, mpValues@0x0C
//
// DECOMPILED from the X360 build:
//   Initialize @ 0x828EA3C8  (KeyType=u64 <ImportHashTableValue> -- body in CgsResourceImportHashTable.cpp)
//   AddEntry   @ 0x828DF260  (LinearSOAHashTable<int,int>)
//   FindEntry  @ 0x828DF3E0  (LinearSOAHashTable<int,int>)
namespace CgsContainers
{
    template <typename ValueType, typename KeyType = u64>
    struct LinearSOAHashTable
    {
        // The invalid/empty-slot sentinel: an all-ones key. A slot whose key equals this is free.
        static const KeyType KU_EMPTY_KEY = static_cast<KeyType>(~static_cast<KeyType>(0));

        // X360 0x828EA3C8 (CgsLinearSOAHashTable.h:167) for the <ImportHashTableValue> (KeyType=u64)
        // instantiation. Definition lives in CgsResourceImportHashTable.cpp (2-param form).
        void Initialize(KeyType* lpKeys, ValueType* lpValues, KeyType luLength);

        // :242 -- insert luKey at the first empty slot from its hash position (wrapping); optionally
        // store *lpValue into the parallel value array. Returns &mpValues[pos], or null if full.
        // (X360 0x828DF260, LinearSOAHashTable<int,int>.)
        ValueType* AddEntry(KeyType luKey, const ValueType* lpValue)
        {
            CGS_ASSERT(luKey != miInvalidKey,
                       "Can not add entry with invalid key value to table\n");   // :242

            const KeyType luStart = luKey % miLength;
            KeyType       luPos   = luStart;
            if (luStart < miLength)
            {
                KeyType* lpKey = &mpKeys[luStart];
                while (*lpKey != miInvalidKey)
                {
                    ++luPos;
                    ++lpKey;
                    if (luPos >= miLength)
                        goto WrapScan;
                }
                mpKeys[luPos] = luKey;
                if (lpValue)
                    mpValues[luPos] = *lpValue;
                return &mpValues[luPos];
            }

        WrapScan:
            luPos = 0;
            if (luStart != 0)
            {
                KeyType* lpKey = mpKeys;
                while (*lpKey != miInvalidKey)
                {
                    ++luPos;
                    ++lpKey;
                    if (luPos >= luStart)
                        return 0;
                }
                mpKeys[luPos] = luKey;
                if (lpValue)
                    mpValues[luPos] = *lpValue;
                return &mpValues[luPos];
            }
            return 0;
        }

        // :420 -- probe from luKey's hash position (wrapping) for luKey; returns &mpValues[pos], or
        // null if an empty slot is reached first (key absent). (X360 0x828DF3E0, <int,int>.)
        ValueType* FindEntry(KeyType luKey)
        {
            const KeyType luStart = luKey % miLength;
            KeyType       luPos   = luStart;
            if (luStart < miLength)
            {
                KeyType* lpKey = &mpKeys[luStart];
                while (*lpKey != luKey)
                {
                    if (*lpKey == miInvalidKey)
                        return 0;
                    ++luPos;
                    ++lpKey;
                    if (luPos >= miLength)
                        goto WrapScan;
                }
                return &mpValues[luPos];
            }

        WrapScan:
            luPos = 0;
            if (luStart == 0)
                return 0;
            {
                KeyType* lpKey = mpKeys;
                while (*lpKey != luKey)
                {
                    if (*lpKey == miInvalidKey)
                        return 0;
                    ++luPos;
                    ++lpKey;
                    if (luPos >= luStart)
                        return 0;
                }
                return &mpValues[luPos];
            }
        }

        // ---- LAYOUT (X360-observed store offsets, authoritative) ----
        //   KeyType=u64:  miLength@0x00, miInvalidKey@0x08, mpKeys@0x10, mpValues@0x14
        //   KeyType=int:  miLength@0x00, miInvalidKey@0x04, mpKeys@0x08, mpValues@0x0C
        KeyType    miLength;     // +0x00              table length (power of 2)
        KeyType    miInvalidKey; // +sizeof(KeyType)   empty-slot sentinel = ~0
        KeyType*   mpKeys;       // key array base
        ValueType* mpValues;     // value array base
    };
}
