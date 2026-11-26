#pragma once

#include <string.h>

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// ARTIST bakes two spellings of this header's path into the asserts of the inlined accessors
// below (IsBitSet :203, SetBit :222, UnSetBit :241), one per translation unit, decided by the
// include path through which the unit first reached this header:
//   "..\..\..\GameShared\GameClasses\Containers/CgsBitArray.h" through the GameShared include
//     root (e.g. TrafficEntityModule::RecalculateActiveHulls 0x8274C870, Profile::RecordPropHit
//     0x82361C48);
//   "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\containers\./CgsBitArray.h" through
//     a nested "./CgsBitArray.h" include from the containers directory (e.g. PropZoneManager::
//     UnloadZone 0x82303790, ObjectPool<VolumeSlot,4608,int>::FreeObject 0x828B9030; the PS3
//     DWARF records the same "Containers/./CgsBitArray.h" path for those units' inlines).
// A source compiled in a unit of the second kind defines CGS_ARTIST_CGSBITARRAY_NESTED_INCLUDE
// before its first #include, so the path is fixed when this header is first read, as __FILE__ was.
#if defined(CGS_ARTIST_CGSBITARRAY_NESTED_INCLUDE)
#define CGS_ARTIST_CGSBITARRAY_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\containers\\./CgsBitArray.h"
#else
#define CGS_ARTIST_CGSBITARRAY_FILE "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h"
#endif

namespace CgsContainers
{
// Reconstructed from the DecFIGS DWARF (CgsBitArray.h). A fixed-capacity bit set
// whose storage is an array of 64-bit fields (one field per 64 bits, rounded up).
// The storage member fixes the type's size so the container can be embedded by
// value. The bit-manipulation methods are header-inline because the X360 build
// inlines every instantiation at its call site (e.g. IsBitSet/UnSetBit folded
// into StreamedVaultAllocator::GetSlotMemory/ReleaseSlot); the semantics here
// match that inlined bit math (field = index / 64, bit = index % 64).
template <u32 tuNumBits>
class BitArray
{
public:
    static const u32 kuNumberOfBitsInBitField = 64;
    static const u32 kuNumberOfBitFields =
        (tuNumBits + kuNumberOfBitsInBitField - 1) / kuNumberOfBitsInBitField;
    static const u32 kuBitsInBitFieldMask = kuNumberOfBitsInBitField - 1;
    static const s32 KI_INVALID_BITINDEX = -1;

    // DWARF CgsBitArray.h:191 / :210 / :229. Each accessor owns its index tripwire (LegacySrc
    // CgsBitArray.h; ARTIST lines 203 / 222 / 241). ARTIST inlines all three, with those asserts,
    // at every one of their ~730 call sites, and the image holds no out-of-line BitArray method;
    // the XDK declines the streamed bodies under /Ob2, so they are __forceinline.
    __forceinline const bool IsBitSet(const u32 luIndex) const
    {
        CGS_ASSERT_STREAM_AT(luIndex < tuNumBits, "invalid index : " << luIndex << " < " << tuNumBits,
                             CGS_ARTIST_CGSBITARRAY_FILE, 203);
        return (maxBits[luIndex / kuNumberOfBitsInBitField] & ((u64)1 << (luIndex & kuBitsInBitFieldMask))) != 0;
    }

    __forceinline void SetBit(const u32 luIndex)
    {
        CGS_ASSERT_STREAM_AT(luIndex < tuNumBits, "Index: " << luIndex << ", Number of bits: " << tuNumBits,
                             CGS_ARTIST_CGSBITARRAY_FILE, 222);
        maxBits[luIndex / kuNumberOfBitsInBitField] |= ((u64)1) << (luIndex & kuBitsInBitFieldMask);
    }

    __forceinline void UnSetBit(const u32 luIndex)
    {
        CGS_ASSERT_AT(luIndex < tuNumBits, "luIndex < NUMBITS", CGS_ARTIST_CGSBITARRAY_FILE, 241);
        maxBits[luIndex / kuNumberOfBitsInBitField] &= ~(((u64)1) << (luIndex & kuBitsInBitFieldMask));
    }

    // DWARF CgsBitArray.h:250 (LegacySrc body).
    void SetBitToBool(const u32 luIndex, bool lbValue)
    {
        if (lbValue)
        {
            SetBit(luIndex);
        }
        else
        {
            UnSetBit(luIndex);
        }
    }

    // ADDITIVE GROW: extract luCount contiguous bits starting at luStartBit into lpaOut, one
    // 64-bit word per 64 bits (ceil(luCount/64) words written), each word LSB-aligned; the
    // final partial word masked to (luCount % 64) bits. Mirrors the X360 cross-field
    // shift-extract (field = start/64, shift = start%64) inlined at PropZoneManager::
    // GetHitPropsFromZone (asm bounds asserts CgsBitArray.h:862/874/914 owned by the caller's
    // assert system, so this stays assert-free). The
    // (luShift == 0 ? 0 : ...) guard reproduces the PPC sld-by-64 == 0 behaviour on the host
    // where a 64-bit shift-by-64 is UB.
    void GetBitRange(u32 luStartBit, u32 luCount, u64* lpaOut) const
    {
        const u32 luStartField = luStartBit / kuNumberOfBitsInBitField;  // start/64
        const u32 luShift      = luStartBit & kuBitsInBitFieldMask;      // start%64
        const u32 luCoShift    = kuNumberOfBitsInBitField - luShift;     // 64-shift
        const u32 luFullWords  = luCount / kuNumberOfBitsInBitField;     // whole 64-bit out words
        const u32 luTailBits   = luCount & kuBitsInBitFieldMask;         // remaining bits in last word

        u32 luOutWord = 0;
        u32 luField   = luStartField;
        for (; luOutWord < luFullWords; ++luOutWord, ++luField)
        {
            const u64 lu64Low  = maxBits[luField] >> luShift;
            const u64 lu64High = (luShift == 0) ? 0 : (maxBits[luField + 1] << luCoShift);
            lpaOut[luOutWord]  = lu64Low | lu64High;
        }
        if (luTailBits != 0)
        {
            const u64 lu64TailMask = ((u64)1 << luTailBits) - 1;
            u64 lu64Low = maxBits[luField] >> luShift;
            if (luCoShift < luTailBits)  // the tail straddles into the next field
            {
                lu64Low |= (maxBits[luField + 1] << luCoShift);
            }
            lpaOut[luOutWord] = lu64Low & lu64TailMask;
        }
    }

public:

    u32 GetCapacity() const
    {
        return tuNumBits;
    }

    // uint32_t CountSetBits() const -- the DWARF-attested name (CgsBitArray.h, alongside
    // GetCapacity/IsZero). The X360 build inlines it at every call site as the textbook SWAR
    // 64-bit population count, one field at a time; PropDebugComponent::RenderStats @0x826131E8
    // carries two verbatim copies of it (on PropManager::mUsedProps @+0x80 and mUsedParts @+0x90),
    // and the constant chain there is exactly this sequence:
    //     x -= (x >> 1) & 0x5555555555555555
    //     x  = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333)
    //     x  = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F
    //     return (u32)((x * 0x0101010101010101) >> 56)
    // Note the X360 does NOT mask off the bits above tuNumBits: the fields only ever hold bits
    // this container itself set, so the unused high bits are always zero. Reproduced as-is.
    u32 CountSetBits() const
    {
        u32 luCount = 0;
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            u64 lu64 = maxBits[luField];
            lu64 -= (lu64 >> 1) & 0x5555555555555555ULL;
            lu64  = (lu64 & 0x3333333333333333ULL) + ((lu64 >> 2) & 0x3333333333333333ULL);
            lu64  = (lu64 + (lu64 >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
            luCount += static_cast<u32>((lu64 * 0x0101010101010101ULL) >> 56);
        }
        return luCount;
    }

    // Per-field bitwise OR of two arrays into this one (this = lpArrayA | lpArrayB).
    // The exact method the DecFIGS DWARF attests for BitArray<60u> (CgsBitArray.h:491,
    // alongside the symmetric ANDArrays in the LegacySrc reference). The X360
    // build inlines it at its call site (AchievementPopupComponent::Display-
    // NewAchievementNotification folds it into a single ld/or/std over the one 64-bit
    // field); header-inline here, matching that inlined per-field OR. Zero-risk additive
    // (no layout change, no behaviour change to existing users).
    void ORArrays(const BitArray* lpArrayA, const BitArray* lpArrayB)
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            maxBits[luField] = lpArrayA->maxBits[luField] | lpArrayB->maxBits[luField];
        }
    }

    // DWARF CgsBitArray.h:306 / :324 / :342 / :360 / :378 / :397, bodies from LegacySrc
    // CgsBitArray.h: SetAll and UnSetAll fill the storage with memset, and the lifecycle
    // methods all clear it through UnSetAll (Prepare reports success). ARTIST inlines them; e.g.
    // VehiclePhysics::Prepare 0x82638080/84 stores one 64-bit zero per single-field array
    // (the PS3 build calls BitArray<4u>/<8u>::Prepare there out of line).
    void SetAll()
    {
        memset(maxBits, 0xFF, sizeof(maxBits));
    }

    void UnSetAll()
    {
        memset(maxBits, 0, sizeof(maxBits));
    }

    void Construct()
    {
        UnSetAll();
    }

    void Destruct()
    {
        UnSetAll();
    }

    bool Prepare()
    {
        UnSetAll();
        return true;
    }

    void Release()
    {
        UnSetAll();
    }

    // ADDITIVE GROW (race-car render wave 2026-07-31). Whole-field store/load, for the
    // console's own whole-field `std` stores.
    //
    // ⛔ 2026-09-07: this comment used to cite ActiveRaceCar::RenderParams::Reset
    // @0x822E6818 as seeding the body-part mask with "a single 64-bit LITERAL
    // (0xB80FFFFFFFF)". IT DOES NOT -- that literal is a Hex-Rays fusion of the unrelated
    // `li r6, 0xB80` (a byte offset for mPaintColour) with `li r7, -1`; the asm stores -1.
    // Reset now calls SetAll(). The live user of this setter is
    // PropEntityModule's maPropsPreviouslyHit restore, which really does store whole words.
    void SetBitField(u32 luField, u64 lu64Value)
    {
        maxBits[luField] = lu64Value;
    }
    u64 GetBitField(u32 luField) const
    {
        return maxBits[luField];
    }

    // True iff no bit is set (the popcount==0 guard).
    bool IsZero() const
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            if (maxBits[luField] != 0)
            {
                return false;
            }
        }
        return true;
    }

    // Index of the lowest CLEAR bit, or KI_INVALID_BITINDEX(-1) if every one of the tuNumBits
    // is set. DWARF-attested name (BitArray<20u>::GetFirstZeroBit, resolved by
    // DetachedWheelManager::DetachWheel @0x8260E430, where the console inlines it as: walk the
    // 64-bit fields for one that is not all-ones (`cmpdi -1`), take the complement's lowest set
    // bit (`~w & (~w - 1)` / cntlzd -> `64*field + 63 - clz`), and treat an index >= tuNumBits
    // as "no free slot" (0x8260E7A0 `cmplwi r30, 0x14`).
    s32 GetFirstZeroBit() const
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            if (maxBits[luField] != ~static_cast<u64>(0))
            {
                const s32 liBit = static_cast<s32>(luField) * static_cast<s32>(kuNumberOfBitsInBitField)
                                + GetZeroBitInInt(~maxBits[luField]);
                if (static_cast<u32>(liBit) >= tuNumBits)
                {
                    return KI_INVALID_BITINDEX;
                }
                return liBit;
            }
        }
        return KI_INVALID_BITINDEX;
    }

    // Index of the lowest set bit, or KI_INVALID_BITINDEX(-1) if none.
    s32 GetFirstNonZeroBit() const
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            if (maxBits[luField] != 0)
            {
                const s32 liBit = static_cast<s32>(luField) * static_cast<s32>(kuNumberOfBitsInBitField)
                                + GetZeroBitInInt(maxBits[luField]);
                if (static_cast<u32>(liBit) >= tuNumBits)
                {
                    return KI_INVALID_BITINDEX;
                }
                return liBit;
            }
        }
        return KI_INVALID_BITINDEX;
    }

    // Index of the lowest set bit with index strictly greater than liAfter, or -1 if none.
    s32 GetNextNonZeroBit(s32 liAfter) const
    {
        for (s32 liBit = liAfter + 1; static_cast<u32>(liBit) < tuNumBits; ++liBit)
        {
            if (IsBitSet(static_cast<u32>(liBit)))
            {
                return liBit;
            }
        }
        return KI_INVALID_BITINDEX;
    }

    // Index of the lowest CLEAR (zero) bit, or KI_INVALID_BITINDEX(-1) if every bit in
    // [0, tuNumBits) is set. Mirrors the X360 find-first-clear-bit slot allocator
    // (TriggerEntityModule::ProcessAddTriggerEvents 0x822D8F48): per 64-bit field, skip
    // all-ones fields (== ~0), else complement the word and reuse the lowest-set-bit
    // helper on the complement (the lowest set bit of ~field == the lowest clear bit of field).
    s32 GetFirstClearBit() const
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            if (maxBits[luField] == ~static_cast<u64>(0))
            {
                continue;   // every bit in this field is set
            }
            const s32 liBit = static_cast<s32>(luField) * static_cast<s32>(kuNumberOfBitsInBitField)
                            + GetZeroBitInInt(~maxBits[luField]);
            if (static_cast<u32>(liBit) >= tuNumBits)
            {
                return KI_INVALID_BITINDEX;
            }
            return liBit;
        }
        return KI_INVALID_BITINDEX;
    }

private:
    // Index of the lowest set bit within a single 64-bit field (the X360 (field*64 -
    // clz64(field & -field) + 63) lowest-set-bit idiom, value-identical to a count-trailing-zeros).
    static s32 GetZeroBitInInt(u64 lu64Field)
    {
        s32 liBit = 0;
        while ((lu64Field & 1) == 0)
        {
            lu64Field >>= 1;
            ++liBit;
        }
        return liBit;
    }

    u64 maxBits[kuNumberOfBitFields];
};
}
