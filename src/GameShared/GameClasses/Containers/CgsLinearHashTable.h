#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsContainers::LinearHashTable<Key, Value> - an open-addressing hash table with linear
// probing over a caller-supplied entry array. The slot for a key is key % miLength
// (miLength is required to be a power of two); on collision it probes forward, wrapping
// once, until it finds the key or an empty slot. An empty slot is one whose miKey equals
// miInvalidKey, which Initialize fixes at all-ones (~0). The resource Pool uses
// LinearHashTable<u64,s32> to map a resource ID hash -> its entry index.
//
// DECOMPILED from the X360 build (these instantiations ARE out-of-line): Initialize
// (0x828E9AD8), FindEntry (0x828DF1B0), AddEntry (0x828DF028) and FindFirstEmptyIndex
// (0x828E0400). Baked asserts: Initialize requires a power-of-two length
// (CgsLinearHashTable.h:182) and AddEntry rejects the invalid key (:256). The
// remove/resize methods (unload path) are declared here and reconstructed when the
// unload path is brought up.

namespace CgsContainers
{
    template <class Key, class Value>
    struct LinearHashEntry
    {
        Key   miKey;     // +0
        Value mValue;    // +sizeof(Key) (8 for u64 -> mValue at +8)
    };

    template <class Key, class Value>
    class LinearHashTable
    {
    public:
        typedef LinearHashEntry<Key, Value> Entry;

        // :156
        Key CalculateRequiredSize(Key luLength);

        // :180 - bind to the entry array (luLength must be a non-zero power of two), set
        // the invalid-key sentinel to all-ones, and clear every slot.
        void Initialize(Entry* lpEntries, Key luLength)
        {
            CGS_ASSERT(luLength != 0 && ((luLength - 1) & luLength) == 0,
                       "Length of hash table must be more than 0 and a power of 2\n");   // :182
            miLength     = luLength;
            mpEntries    = lpEntries;
            miInvalidKey = static_cast<Key>(-1);
            Clear();
        }

        // :204 - mark every slot empty (miKey = miInvalidKey).
        void Clear()
        {
            for (Key luIndex = 0; luIndex < miLength; ++luIndex)
                mpEntries[luIndex].miKey = miInvalidKey;
        }

        // :253 - insert luKey at the first empty slot from its hash position (wrapping);
        // optionally store *lpValue. Returns &slot.mValue, or null if the table is full.
        Value* AddEntry(Key luKey, const Value* lpValue)
        {
            CGS_ASSERT(luKey != miInvalidKey, "Can not add entry with invalid key value to table\n");   // :256
            Key luStart = luKey % miLength;
            Key luPos   = luStart;
            if (luStart < miLength)
            {
                do
                {
                    if (mpEntries[luPos].miKey == miInvalidKey)
                        return Store(luPos, luKey, lpValue);
                } while (++luPos < miLength);
            }
            luPos = 0;
            if (luStart != 0)
            {
                do
                {
                    if (mpEntries[luPos].miKey == miInvalidKey)
                        return Store(luPos, luKey, lpValue);
                } while (++luPos < luStart);
            }
            return 0;
        }

        // :302 - remove luKey: find its slot, empty it, then re-insert the contiguous run that follows
        // (wrapping) so open-addressing probe chains stay intact. Returns true iff the key was present.
        // (No X360 out-of-line body -- it was inlined/unused there; reconstructed as the standard
        // linear-probe delete-with-reinsert, using the same probe order as FindEntry/AddEntry. Re-adds
        // always move an entry backward into the moving gap or leave it in place, never forward past the
        // cursor, so a single forward pass repairs the cluster.)
        bool RemoveEntry(Key luKey)
        {
            if (luKey == miInvalidKey)
                return false;

            // locate the slot holding luKey (probe from its hash position, wrapping)
            Key luFound = miInvalidKey;
            for (Key luc = 0; luc < miLength; ++luc)
            {
                const Key luPos = (luKey % miLength + luc) % miLength;
                if (mpEntries[luPos].miKey == luKey)        { luFound = luPos; break; }
                if (mpEntries[luPos].miKey == miInvalidKey) break;   // empty slot reached -> absent
            }
            if (luFound == miInvalidKey)
                return false;

            mpEntries[luFound].miKey = miInvalidKey;   // open the gap

            // re-insert the run after the gap until an empty slot (repairs probe chains)
            for (Key luPos = (luFound + 1) % miLength;
                 mpEntries[luPos].miKey != miInvalidKey;
                 luPos = (luPos + 1) % miLength)
            {
                const Key   luRehashKey = mpEntries[luPos].miKey;
                const Value lRehashVal  = mpEntries[luPos].mValue;
                mpEntries[luPos].miKey  = miInvalidKey;
                AddEntry(luRehashKey, &lRehashVal);
            }
            return true;
        }

        // :434 - probe from luKey's hash position (wrapping) for luKey; returns
        // &slot.mValue, or null if an empty slot is reached first (key absent).
        Value* FindEntry(Key luKey)
        {
            Key luStart = luKey % miLength;
            Key luPos   = luStart;
            if (luStart < miLength)
            {
                do
                {
                    Key luEntryKey = mpEntries[luPos].miKey;
                    if (luEntryKey == luKey)        return &mpEntries[luPos].mValue;
                    if (luEntryKey == miInvalidKey) return 0;
                } while (++luPos < miLength);
            }
            luPos = 0;
            if (luStart != 0)
            {
                do
                {
                    Key luEntryKey = mpEntries[luPos].miKey;
                    if (luEntryKey == luKey)        return &mpEntries[luPos].mValue;
                    if (luEntryKey == miInvalidKey) return 0;
                } while (++luPos < luStart);
            }
            return 0;
        }

        // :534 - declared; reconstructed with the unload path.
        bool VerifyHashTable();

    private:
        // :400 - first empty slot from luStart (wrapping), or miInvalidKey if full.
        Key FindFirstEmptyIndex(Key luStart)
        {
            Key luPos = luStart;
            if (luStart < miLength)
            {
                do
                {
                    if (mpEntries[luPos].miKey == miInvalidKey) return luPos;
                } while (++luPos < miLength);
            }
            luPos = 0;
            if (luStart != 0)
            {
                do
                {
                    if (mpEntries[luPos].miKey == miInvalidKey) return luPos;
                } while (++luPos < luStart);
            }
            return miInvalidKey;
        }

        // :489 / :351 / :224 - declared; reconstructed with the unload path.
        Key  FindEntryPosition(Key luKey);
        Key  RemoveEntryInternal(Key luKey);
        void ReAddIndex(Key luIndex);

        Value* Store(Key luPos, Key luKey, const Value* lpValue)
        {
            mpEntries[luPos].miKey = luKey;
            if (lpValue) mpEntries[luPos].mValue = *lpValue;
            return &mpEntries[luPos].mValue;
        }

        // ---- Layout (offsets verified against the X360 hash methods) -------------------
        Key    miLength;       // +0  number of slots (power of two)
        Entry* mpEntries;      // +8  caller-owned slot array
        Key    miInvalidKey;   // +16 empty-slot sentinel (all-ones)
    };
}
