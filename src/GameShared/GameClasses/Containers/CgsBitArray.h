#pragma once

#include "types.hpp"

namespace CgsContainers
{
// Reconstructed from the DecFIGS DWARF (CgsBitArray.h). A fixed-capacity bit set
// whose storage is an array of 64-bit fields (one field per 64 bits, rounded up).
// The storage member fixes the type's size so the container can be embedded by
// value. The bit-manipulation methods are header-inline because the X360 build
// inlines every instantiation at its call site (e.g. IsBitSet/UnSetBit folded
// into StreamedVaultAllocator::GetSlotMemory/ReleaseSlot); the semantics here
// match that inlined bit math (field = index / 64, bit = index % 64).
//
// Bounds asserts that the DWARF shows guarding these methods (CgsBitArray.h:203,
// :241) are emitted at the call sites by callers that own the CgsDev::Assert API,
// so this header stays free of the assert-system dependency.
template <u32 tuNumBits>
class BitArray
{
public:
    static const u32 kuNumberOfBitsInBitField = 64;
    static const u32 kuNumberOfBitFields =
        (tuNumBits + kuNumberOfBitsInBitField - 1) / kuNumberOfBitsInBitField;
    static const u32 kuBitsInBitFieldMask = kuNumberOfBitsInBitField - 1;
    static const s32 KI_INVALID_BITINDEX = -1;

    bool IsBitSet(u32 luIndex) const
    {
        const u32 luField = luIndex / kuNumberOfBitsInBitField;
        const u64 lu64Mask = (u64)1 << (luIndex & kuBitsInBitFieldMask);
        return (maxBits[luField] & lu64Mask) != 0;
    }

    // ADDITIVE GROW: extract luCount contiguous bits starting at luStartBit into lpaOut, one
    // 64-bit word per 64 bits (ceil(luCount/64) words written), each word LSB-aligned; the
    // final partial word masked to (luCount % 64) bits. Mirrors the X360 cross-field
    // shift-extract (field = start/64, shift = start%64) inlined at PropZoneManager::
    // GetHitPropsFromZone (asm bounds asserts CgsBitArray.h:862/874/914 owned by the caller's
    // assert system, so this stays assert-free like the rest of the header). The
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

    void SetBit(u32 luIndex)
    {
        const u32 luField = luIndex / kuNumberOfBitsInBitField;
        const u64 lu64Mask = (u64)1 << (luIndex & kuBitsInBitFieldMask);
        maxBits[luField] |= lu64Mask;
    }

    void UnSetBit(u32 luIndex)
    {
        const u32 luField = luIndex / kuNumberOfBitsInBitField;
        const u64 lu64Mask = (u64)1 << (luIndex & kuBitsInBitFieldMask);
        maxBits[luField] &= ~lu64Mask;
    }

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
    // alongside the symmetric ANDArrays @ :445 in the Feb-2007 reference). The X360
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

    // ===== Added for the CarCheckpointData TU (BitArray<16>) =====
    // The four generic methods below are the EXACT methods the Feb-2007 reference template
    // declares (DWARF BitArray<16u> at CgsBitArray.h:4482/:4485/:4496/:4502 attests
    // UnSetAll/IsZero/GetFirstNonZeroBit/GetNextNonZeroBit) and that CarCheckpointData's four
    // X360 bodies inline. Header-inline, matching the X360 inlined bit math; zero-risk additive
    // (no layout change, no behaviour change to existing IsBitSet/SetBit/UnSetBit users).

    // ADDITIVE GROW (VehiclePhysics::Prepare wave, 2026-08-11). `Prepare()` is a REAL method of
    // this template, not a synonym invented here: the PS3 build exports it out of line for at
    // least eight instantiations, mangled -- BitArray<1u>, <2u>, <3u>, <4u>, <8u>, <28u>, <46u>,
    // <50u>, <64u> all carry `_ZN13CgsContainers8BitArrayILj..EE7PrepareEv`. The X360 inlines
    // every one of them; VehiclePhysics::Prepare @0x82638080/84 is two such inlines side by side
    // (`std r30,0x1158` / `std r30,0x1220` -- one 64-bit zero per single-field array), and the
    // PS3 build of that same source calls `CgsContainers::BitArray<4u>::Prepare(this+0x1148)`
    // and its BitArray<8u> sibling at the matching two offsets. Same clearing semantics as
    // UnSetAll above; kept as a separate name because the console source distinguishes them and
    // the call sites should read the way the console source reads.
    void Prepare()
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            maxBits[luField] = 0;
        }
    }

    // Clear every bit (the X360 `*this = 0` of the single-field BitArray<16>).
    void UnSetAll()
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            maxBits[luField] = 0;
        }
    }

    // ADDITIVE GROW (StreetData wave-C keystone; DWARF CgsBitArray.h:55 attests SetAll on the
    // reference template). Set every bit -- whole-word ~0 stores, exactly the X360 single
    // `std -1` of the one-field BitArray<2> in StreetData::FixUp/FixDown's par-score loop.
    void SetAll()
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            maxBits[luField] = ~static_cast<u64>(0);
        }
    }

    // ADDITIVE GROW (race-car render wave 2026-07-31). Whole-field store/load. The X360
    // ActiveRaceCar::RenderParams::Reset @0x822E6818 seeds the 96-bit body-part mask by
    // storing a single 64-bit LITERAL (0xB80FFFFFFFF) into each of its two fields -- not
    // by setting individual bits -- so the faithful expression of that store needs a
    // field-granular setter rather than a bit-granular one.
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
