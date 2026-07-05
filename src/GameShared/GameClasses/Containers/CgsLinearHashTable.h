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
// DECOMPILED from the X360 build (these instantiations ARE out-of-line):
//   Initialize           @ 0x828E9AD8   (power-of-two length assert, :182)
//   FindEntry            @ 0x828DF1B0
//   AddEntry             @ 0x828DF028   (invalid-key assert, :256)
//   FindFirstEmptyIndex  @ 0x828E0400
//   RemoveEntry          @ 0x828F2590   (invalid-key assert, :305)
//   RemoveEntryInternal  @ 0x828EAC10   (no-empty-slot :357 / not-empty-slot :358 asserts)
// Layout + method shapes are pinned by the DecFIGS DWARF for LinearHashTable<u64,s32>:
// miLength (u64 @+0), mpEntries (Entry* @+8), miInvalidKey (u64 @+16); Entry is
// {miKey u64 @+0, mValue s32 @+8} with a 16-byte stride (asm slwi ,4). CalculateRequiredSize
// / VerifyHashTable / FindEntryPosition remain declared-only (not yet brought up).

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

        // :302 - remove luKey: probe (wrapping) for its slot; if found, hand the slot index to
        // RemoveEntryInternal (which empties it and repairs the following probe run) and return
        // true. Returns false if the key is absent (an empty slot is reached first).
        // (X360 0x828F2590 -- store-for-store: invalid-key assert :305, key % miLength start, the
        // two-loop [start,miLength) then [0,start) probe, bl RemoveEntryInternal on match.)
        bool RemoveEntry(Key luKey)
        {
            CGS_ASSERT(luKey != miInvalidKey, "Can not remove entry with invalid key value to table\n");   // :305
            Key luStart = luKey % miLength;
            Key luPos   = luStart;
            if (luStart < miLength)
            {
                do
                {
                    Key luEntryKey = mpEntries[luPos].miKey;
                    if (luEntryKey == luKey)        { RemoveEntryInternal(luPos); return true; }
                    if (luEntryKey == miInvalidKey) return false;
                } while (++luPos < miLength);
            }
            luPos = 0;
            if (luStart != 0)
            {
                do
                {
                    Key luEntryKey = mpEntries[luPos].miKey;
                    if (luEntryKey == luKey)        { RemoveEntryInternal(luPos); return true; }
                    if (luEntryKey == miInvalidKey) return false;
                } while (++luPos < luStart);
            }
            return false;
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

        // :351 - empty the slot at luIndex, then re-add the contiguous occupied run that follows
        // (wrapping past miLength) up to the first empty slot, so open-addressing probe chains stay
        // intact. Asserts there is an empty slot in the table (:357) and that luIndex itself is not
        // already empty (:358). Returns luIndex. (X360 0x828EAC10, store-for-store.)
        Key RemoveEntryInternal(Key luIndex)
        {
            Key luFirstEmpty = FindFirstEmptyIndex(luIndex);
            CGS_ASSERT(luFirstEmpty != miInvalidKey,
                       "There is no empty slot in the hash table - this should NEVER happen\n");   // :357
            CGS_ASSERT(luFirstEmpty != luIndex, "Can not remove an empty slot!\n");                 // :358

            mpEntries[luIndex].miKey = miInvalidKey;
            Key luPos = luIndex + 1;
            if (luFirstEmpty > luIndex)
            {
                for (; luPos < luFirstEmpty; ++luPos)
                    ReAddIndex(luPos);
            }
            else
            {
                for (; luPos < miLength; ++luPos)
                    ReAddIndex(luPos);
                for (Key luWrap = 0; luWrap < luFirstEmpty; ++luWrap)
                    ReAddIndex(luWrap);
            }
            return luIndex;
        }

        // :224 - lift the (occupied) entry at luIndex out and re-insert it via AddEntry, keeping its
        // stored value. Empty slots are skipped. This is the inlined body of the RemoveEntryInternal
        // repair loops (asm: read miKey; if != miInvalidKey, set slot empty and AddEntry(key, &mValue)).
        void ReAddIndex(Key luIndex)
        {
            if (mpEntries[luIndex].miKey != miInvalidKey)
            {
                Key luKey = mpEntries[luIndex].miKey;
                mpEntries[luIndex].miKey = miInvalidKey;
                AddEntry(luKey, &mpEntries[luIndex].mValue);
            }
        }

        // :489 - declared; reconstructed with the unload path.
        Key  FindEntryPosition(Key luKey);

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
