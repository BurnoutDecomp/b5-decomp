#pragma once

#include "types.hpp"

// ---------------------------------------------------------------------------
// CgsContainers::FastBitArray<tuNumBits>
//
// DWARF home: GameShared/GameClasses/Containers/CgsFastBitArray.h. A fixed-capacity
// bit set whose storage is an array of 64-bit fields (one field per 64 bits, rounded
// up), distinct from the simpler CgsContainers::BitArray (CgsBitArray.h) by adding the
// "next set bit" iteration helpers (GetFirstBitSet / GetNextBitSet) and a SetAll the
// X360 inlines as a per-bit shift-or loop.
//
// The X360 build inlines every instantiation at its call site (BurnoutSkillzManager's
// mabDirtyFlags<8> SetBit/SetAll/GetFirstBitSet/GetNextBitSet are all folded into the
// manager's bodies, complete with their out-of-range CgsDev::StrStream assert
// machinery). That assert/StrStream scaffolding is owned by the callers (which own the
// CgsDev::Assert API), so this container header stays free of the assert-system
// dependency -- the inlined bit math here is value-identical to those folded sites.
// (Assert-machinery parity is intentionally not reproduced bit-for-bit; it is benign.)
//
// The storage member fixes the type's size so the container can be embedded by value
// (BurnoutSkillzManager::mabDirtyFlags is a FastBitArray<8> @ +0x90, one u64 field).
namespace CgsContainers
{
template <u32 tuNumBits>
class FastBitArray
{
public:
    static const u32 KU_NUMBER_OF_BITS_IN_BIT_FIELD = 64;
    static const u32 KU_NUMBER_OF_BIT_FIELDS =
        (tuNumBits + KU_NUMBER_OF_BITS_IN_BIT_FIELD - 1) / KU_NUMBER_OF_BITS_IN_BIT_FIELD;
    static const u32 KU_BITS_IN_BIT_FIELD_MASK = KU_NUMBER_OF_BITS_IN_BIT_FIELD - 1;
    static const s32 KI_INVALID_BIT_INDEX = -1;

    // Zero every field (the X360 ctor / Construct path leaves a zeroed array).
    void Construct()
    {
        for (u32 luField = 0; luField < KU_NUMBER_OF_BIT_FIELDS; ++luField)
        {
            maxBits[luField] = 0;
        }
    }

    bool IsBitSet(u32 luIndex) const
    {
        const u32 luField = luIndex / KU_NUMBER_OF_BITS_IN_BIT_FIELD;
        const u64 lu64Mask = (u64)1 << (luIndex & KU_BITS_IN_BIT_FIELD_MASK);
        return (maxBits[luField] & lu64Mask) != 0;
    }

    void SetBit(u32 luIndex)
    {
        const u32 luField = luIndex / KU_NUMBER_OF_BITS_IN_BIT_FIELD;
        const u64 lu64Mask = (u64)1 << (luIndex & KU_BITS_IN_BIT_FIELD_MASK);
        maxBits[luField] |= lu64Mask;
    }

    void UnSetBit(u32 luIndex)
    {
        const u32 luField = luIndex / KU_NUMBER_OF_BITS_IN_BIT_FIELD;
        const u64 lu64Mask = (u64)1 << (luIndex & KU_BITS_IN_BIT_FIELD_MASK);
        maxBits[luField] &= ~lu64Mask;
    }

    // Set every bit in [0, tuNumBits) (the X360 OnEnterRoad shift-or loop that ORs each
    // 1<<i into the single field, i in [0, 8)).
    void SetAll()
    {
        for (u32 luIndex = 0; luIndex < tuNumBits; ++luIndex)
        {
            SetBit(luIndex);
        }
    }

    void UnSetAll()
    {
        for (u32 luField = 0; luField < KU_NUMBER_OF_BIT_FIELDS; ++luField)
        {
            maxBits[luField] = 0;
        }
    }

    // Index of the lowest set bit, or KI_INVALID_BIT_INDEX(-1) if none / out of range.
    s32 GetFirstBitSet() const
    {
        for (u32 luIndex = 0; luIndex < tuNumBits; ++luIndex)
        {
            if (IsBitSet(luIndex))
            {
                return static_cast<s32>(luIndex);
            }
        }
        return KI_INVALID_BIT_INDEX;
    }

    // Index of the lowest set bit strictly after liAfter, or -1 if none. Matches the
    // X360 "scan forward from liAfter+1" iteration the manager inlines after handling a
    // dirty player.
    s32 GetNextBitSet(s32 liAfter) const
    {
        for (s32 liBit = liAfter + 1; static_cast<u32>(liBit) < tuNumBits; ++liBit)
        {
            if (IsBitSet(static_cast<u32>(liBit)))
            {
                return liBit;
            }
        }
        return KI_INVALID_BIT_INDEX;
    }

    u32 GetCapacity() const
    {
        return tuNumBits;
    }

    // DWARF CgsFastBitArray.h:519 -- true when every bit field is zero (the X360 inlines
    // it as a field scan, e.g. HudMessageAnalyzer::HandleDeveloperChallengeMessageDEBUG
    // @0x824F9D88..A4 ldx/cmpldi over the fields).
    bool IsZero() const
    {
        for (u32 luField = 0; luField < KU_NUMBER_OF_BIT_FIELDS; ++luField)
        {
            if (maxBits[luField] != 0)
            {
                return false;
            }
        }
        return true;
    }

    // DWARF CgsFastBitArray.h:556 -- this = a | b, field-wise (X360 inline: ld/or/std per
    // field, e.g. @0x824F9DDC..E8 for the 1-field <15> instantiation).
    void SetOr(const FastBitArray<tuNumBits>& lrA, const FastBitArray<tuNumBits>& lrB)
    {
        for (u32 luField = 0; luField < KU_NUMBER_OF_BIT_FIELDS; ++luField)
        {
            maxBits[luField] = lrA.maxBits[luField] | lrB.maxBits[luField];
        }
    }

    // DWARF CgsFastBitArray.h:60 -- the "next set bit" iterator (members h:91-93). The
    // X360 folds every instantiation inline; the <15> fold in
    // HudMessageAnalyzer::TriggerDeveloperChallengeMessageDEBUG (@0x825204EC..0x82520660)
    // pins the observable contract: construction positions at the LOWEST set bit; an
    // EMPTY array positions at tuNumBits (== End()); GetIndex() returns the raw index
    // either way (the X360 additionally fires "Attempt to get index when out of range",
    // h:235, and "Internal mask has wrapped - expected to find valid bit", h:282 -- the
    // assert scaffolding is caller-owned per this header's policy and is intentionally
    // not reproduced). MEASURED: the 1-field scan shape + the empty->tuNumBits result.
    // INFERENCE: the multi-field advance (field = miIndex/64 walk) -- no multi-field
    // instantiation of Begin() is inlined anywhere in the X360 spine we have decoded.
    class Iterator
    {
    public:
        // DWARF h:189 -- position at the first set bit of lpxSourceMasks.
        explicit Iterator(const u64* lpxSourceMasks)
            : miIndex(0)
            , mpxSourceMasks(lpxSourceMasks)
            , mxMask(1)
        {
            if ((mpxSourceMasks[0] & mxMask) == 0)
            {
                Advance();
            }
        }

        s32 GetIndex() const { return miIndex; }                            // DWARF h:230
        Iterator& operator++() { Advance(); return *this; }                 // DWARF h:245
        bool operator==(s32 liIndex) const { return miIndex == liIndex; }   // DWARF h:340
        bool operator!=(s32 liIndex) const { return miIndex != liIndex; }   // DWARF h:354
        u64 GetMask() const { return mxMask; }                              // DWARF h:369

    private:
        // Scan forward for the next set bit; stop at tuNumBits (== End()) when none.
        void Advance()
        {
            do
            {
                ++miIndex;
                if (miIndex >= static_cast<s32>(tuNumBits))
                {
                    mxMask = 0;   // exhausted -- parked at End()
                    return;
                }
                mxMask = (mxMask == 0) ? 1 : (mxMask << 1);
                if (mxMask == 0)  // 64-bit boundary: wrap into the next field
                {
                    mxMask = 1;
                }
            }
            while ((mpxSourceMasks[miIndex / static_cast<s32>(KU_NUMBER_OF_BITS_IN_BIT_FIELD)]
                    & mxMask) == 0);
        }

        s32        miIndex;          // DWARF h:91
        const u64* mpxSourceMasks;   // DWARF h:92
        u64        mxMask;           // DWARF h:93
    };

    // DWARF h:134/h:138.
    Iterator Begin() const { return Iterator(maxBits); }
    s32 End() const { return static_cast<s32>(tuNumBits); }

private:
    u64 maxBits[KU_NUMBER_OF_BIT_FIELDS];
};
}
