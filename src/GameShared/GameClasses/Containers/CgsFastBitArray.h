#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT_STREAM
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // the streamed assert messages

#include <ppcintrinsics.h>   // _CountLeadingZeros64 (Iterator::operator++)

// CgsContainers::FastBitArray<NUMBITS> -- a fixed-capacity bit set stored as an array of
// 64-bit fields, with an iterator over its set bits. Member set, names, types and lines from
// the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/
// CgsFastBitArray.h); the template parameter is an int (DecFIGS mangles every instance
// FastBitArray<int N>, e.g. ._ZN13CgsContainers12FastBitArrayILi601EE6SetBitEi @0x9702B4);
// the file's shape and the bodies from the LegacySrc copy of the header, checked against
// ARTIST, which inlines every member into its callers (no out-of-line copy exists).
//
// SOURCE LINES: ARTIST bakes this header's assert lines into every inlined copy (file string
// @0x820184D0): 235, 250/251, 282/284, 314/316, 374, 396, 415, 431, 442, 452, 463, 494 (629
// sites in 115 functions). Each is its DecFIGS DWARF definition line + 3 + the assert's offset
// inside the body, the offsets of the LegacySrc/DecFIGS bodies (the PS3 out-of-line
// Iterator::operator++ @0x96CA74 passes 247/248/279/281/311/313 for ARTIST's 250/251/282/284/
// 314/316). So every definition below sits on its DWARF line + 3. Notes on the evidence for
// each body follow the namespace.



namespace CgsContainers
{

























template<int NUMBITS> class FastBitArray
{
public:
    static const uint64_t KU_PADDEDBYTES = (NUMBITS + 7) / 8;
    static const uint64_t KU_NUMBEROFBITFIELDS = (KU_PADDEDBYTES + (sizeof(uint64_t) - 1)) / sizeof(uint64_t);
    static const uint64_t KU_NUMBEROFBITSINBITFIELD = sizeof( uint64_t ) * 8;
    static const uint64_t KU_BITSINBITFIELDMASK = KU_NUMBEROFBITSINBITFIELD - 1;


    class Iterator
    {
    public:

        inline Iterator( );

        inline Iterator( const uint64_t* lpxSourceMasks );

        inline void
        ConstructOrphan( int32_t liIndex );

        inline int32_t
        GetIndex() const;

        inline Iterator&
        operator++();

        inline bool
        operator==(int32_t liIndex) const;

        inline bool
        operator!=(int32_t liIndex) const;

        inline uint64_t
        GetMask() const;





    private:
        int32_t miIndex;
        const uint64_t* mpxSourceMasks;
        uint64_t mxMask;

    };

    inline void Construct( void );
    inline void Destruct( void );

    inline const bool IsBitSet( const int32_t liIndex ) const;
    inline const bool IsBitSet( const Iterator& lrIterator ) const;

    inline void SetBit( const int32_t liIndex );
    inline void SetBit( const Iterator& lrIterator );

    inline void UnSetBit( const int32_t liIndex );
    inline void UnSetBit( const Iterator& lrIterator );

    inline void SetBitTo( const int32_t liIndex, bool lValue );

    inline void SetAll( void );
    inline void UnSetAll( void );

    inline bool IsZero( void ) const;

    inline void SetInverse( const FastBitArray<NUMBITS>& lrSource );
    inline void SetAnd( const FastBitArray<NUMBITS>& lrA, const FastBitArray<NUMBITS>& lrB );
    inline void SetOr( const FastBitArray<NUMBITS>& lrA, const FastBitArray<NUMBITS>& lrB );














    inline Iterator
    Begin() const
    { return Iterator(maxBits); };

    inline int32_t
    End() const
    { return NUMBITS; };

    // Not in the DWARF: scan helpers the tree's callers still use where ARTIST inlines the
    // Iterator (CgsReliableMessageManager, ChallengeManager, ...). Defined after SetOr, so
    // they move no original line; they keep the raw bit test (no assert) they had.
    static const int32_t KI_INVALID_BIT_INDEX = -1;
    inline int32_t GetFirstBitSet() const;
    inline int32_t GetNextBitSet( int32_t liAfter ) const;
    uint32_t GetCapacity() const { return static_cast<uint32_t>(NUMBITS); }






private:
    uint64_t maxBits[KU_NUMBEROFBITFIELDS];
};
















template<int NUMBITS>
inline FastBitArray<NUMBITS>::Iterator::Iterator()
{
    mpxSourceMasks = 0;
    miIndex = 0;
    mxMask = 0;
}








template<int NUMBITS>
inline FastBitArray<NUMBITS>::Iterator::Iterator(const uint64_t* lpxSourceMasks)
{
    mpxSourceMasks = lpxSourceMasks;
    miIndex = 0;
    mxMask = 0x1ULL;

    if( (mpxSourceMasks[0] & mxMask) == 0 )
    {
        ++(*this);
    }
}









template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::Iterator::ConstructOrphan(int32_t liIndex)
{
    miIndex = liIndex;
    mpxSourceMasks = 0;
    mxMask = 0x1ULL << (static_cast<uint64_t>(liIndex) & KU_BITSINBITFIELDMASK);
}











template<int NUMBITS>
inline int32_t
FastBitArray<NUMBITS>::Iterator::GetIndex() const
{
    CGS_ASSERT_STREAM((miIndex >= 0) && (miIndex < NUMBITS), "Attempt to get index when out of range\n");
    return miIndex;
}








template<int NUMBITS>
inline typename FastBitArray<NUMBITS>::Iterator&
FastBitArray<NUMBITS>::Iterator::operator++()
{
    CGS_ASSERT_STREAM((miIndex >= 0) && (miIndex < NUMBITS), "Attempt to get index when out of range\n");
    CGS_ASSERT_STREAM(mpxSourceMasks, "No source mask\n");

    uint64_t luTempSourceMask;
    uint64_t luStartMaskIndex;
    uint64_t luStartMaskMod;
    uint64_t luMaskIndex;
    const uint64_t* lpxSourceMasks;

    int32_t liCurrIndex;
    uint64_t lxCurrMask;


    luStartMaskIndex = static_cast<uint64_t>(miIndex) >> 0x6ULL;
    luStartMaskMod = static_cast<uint64_t>(miIndex) - (luStartMaskIndex << 0x6ULL);


    lpxSourceMasks = mpxSourceMasks;
    luTempSourceMask = lpxSourceMasks[luStartMaskIndex];


    liCurrIndex = miIndex;
    lxCurrMask = mxMask;


    if( static_cast<int32_t>(63ULL-luStartMaskMod) > _CountLeadingZeros64(luTempSourceMask) )
    {

        do
        {
            lxCurrMask = lxCurrMask << 0x1ULL;
            liCurrIndex = liCurrIndex + 0x1L;
            CGS_ASSERT_STREAM(lxCurrMask != 0, "Internal mask has wrapped - expected to find valid bit\n");
        } while( (luTempSourceMask & lxCurrMask) == 0 );
        CGS_ASSERT_STREAM(miIndex < NUMBITS, "Index has gone out of range\n");


        miIndex = liCurrIndex;
        mxMask = lxCurrMask;


        return *this;
    }
    else
    {

        luStartMaskIndex++;
        lxCurrMask = 0x1ULL;
        liCurrIndex = static_cast<int32_t>(luStartMaskIndex) << 0x6ULL;
    }


    for(luMaskIndex = luStartMaskIndex; luMaskIndex < FastBitArray<NUMBITS>::KU_NUMBEROFBITFIELDS; luMaskIndex++, liCurrIndex+=64L)
    {
        if(lpxSourceMasks[luMaskIndex] != 0)
        {

            luTempSourceMask = lpxSourceMasks[luMaskIndex];


            while( (luTempSourceMask & lxCurrMask) == 0 )
            {
                lxCurrMask = lxCurrMask << 0x1ULL;
                liCurrIndex++;
                CGS_ASSERT_STREAM(lxCurrMask != 0, "Internal mask has wrapped - expected to find valid bit\n");
            }
            CGS_ASSERT_STREAM(miIndex < NUMBITS, "Index has gone out of range\n");


            miIndex = liCurrIndex;
            mxMask = lxCurrMask;


            return *this;
        }
    }


    miIndex = NUMBITS;


    return *this;
}








template<int NUMBITS>
inline bool
FastBitArray<NUMBITS>::Iterator::operator==(int32_t liIndex) const
{
    return (miIndex == liIndex);
}








template<int NUMBITS>
inline bool
FastBitArray<NUMBITS>::Iterator::operator!=(int32_t liIndex) const
{
    return (miIndex != liIndex);
}









template<int NUMBITS>
inline uint64_t
FastBitArray<NUMBITS>::Iterator::GetMask() const
{
    CGS_ASSERT_STREAM((miIndex >= 0) && (miIndex < NUMBITS), "Attempt to get mask when out of range\n");
    return mxMask;
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::Construct( void )
{
    UnSetAll();
}

template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::Destruct( void )
{
}

template<int NUMBITS>
inline const bool
FastBitArray<NUMBITS>::IsBitSet( const int32_t liIndex ) const
{
    CGS_ASSERT_STREAM(liIndex < NUMBITS, "Index " << liIndex << " is out of range (max bits: " << NUMBITS << "\n");

    int32_t liMaskIndex = liIndex >> 6;
    int32_t liBitIndex = liIndex - (liMaskIndex << 6);

    if( (maxBits[liMaskIndex] != 0) && ( (maxBits[liMaskIndex] & (0x1ULL << static_cast<uint64_t>(liBitIndex))) != 0 ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

template<int NUMBITS>
inline const bool
FastBitArray<NUMBITS>::IsBitSet( const Iterator& lrIterator ) const
{
    CGS_ASSERT_STREAM(lrIterator.GetIndex() < NUMBITS, "Index " << lrIterator.GetIndex() << " is out of range (max bits: " << NUMBITS << "\n");
    int32_t liMaskIndex = lrIterator.GetIndex() >> 6;
    if( (maxBits[liMaskIndex] & lrIterator.GetMask()) != 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetBit( const int32_t liIndex )
{
    CGS_ASSERT_STREAM(liIndex < NUMBITS, "Index " << liIndex << " is out of range (max bits: " << NUMBITS << "\n");

    int32_t liMaskIndex = liIndex >> 6;
    int32_t liBitIndex = liIndex - (liMaskIndex << 6);
    maxBits[liMaskIndex] = maxBits[liMaskIndex] | (0x1ULL << static_cast<uint64_t>(liBitIndex));
}

template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetBit( const Iterator& lrIterator )
{
    CGS_ASSERT_STREAM(lrIterator.GetIndex() < NUMBITS, "Index " << lrIterator.GetIndex() << " is out of range (max bits: " << NUMBITS << "\n");
    int32_t liMaskIndex = lrIterator.GetIndex() >> 6;
    maxBits[liMaskIndex] = maxBits[liMaskIndex] | lrIterator.GetMask();
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::UnSetBit( const int32_t liIndex )
{
    CGS_ASSERT_STREAM(liIndex < NUMBITS, "Index " << liIndex << " is out of range (max bits: " << NUMBITS << "\n");

    int32_t liMaskIndex = liIndex >> 6;
    int32_t liBitIndex = liIndex - (liMaskIndex << 6);
    maxBits[liMaskIndex] = maxBits[liMaskIndex] & ~(0x1ULL << static_cast<uint64_t>(liBitIndex));
}

template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::UnSetBit( const Iterator& lrIterator )
{
    CGS_ASSERT_STREAM(lrIterator.GetIndex() < NUMBITS, "Index " << lrIterator.GetIndex() << " is out of range (max bits: " << NUMBITS << "\n");
    int32_t liMaskIndex = lrIterator.GetIndex() >> 6;
    maxBits[liMaskIndex] = maxBits[liMaskIndex] & ~lrIterator.GetMask();
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetBitTo( const int32_t liIndex, bool lValue )
{
    if(lValue)
    {
        SetBit( liIndex);
    }
    else
    {
        UnSetBit(liIndex);
    }
}

template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetAll( void )
{
    int32_t liIndex;
    int32_t liMaskCount = NUMBITS >> 6;
    int32_t liBitCount = NUMBITS - (liMaskCount << 6);


    for(liIndex = 0; liIndex < liMaskCount; liIndex++)
    {
        CGS_ASSERT_STREAM(static_cast<uint64_t>(liIndex) < KU_NUMBEROFBITFIELDS, "Bit field index out of range\n");
        maxBits[liIndex] = static_cast<uint64_t>(-1);
    }


    if(liBitCount > 0)
    {
        CGS_ASSERT_STREAM(liMaskCount < KU_NUMBEROFBITFIELDS, "For none-64 bit multiple, mask count at this stage should be less than the number of bit fields\n");
        for(liIndex = 0; liIndex < liBitCount; liIndex++)
        {
            maxBits[liMaskCount] |= ( 0x1ULL << static_cast<uint64_t>(liIndex) );
        }
    }
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::UnSetAll( void )
{
    for(uint64_t luIndex = 0; luIndex < KU_NUMBEROFBITFIELDS; luIndex++)
    {
        maxBits[luIndex] = 0;
    }
}

template<int NUMBITS>
inline bool
FastBitArray<NUMBITS>::IsZero( void ) const
{
    for(uint64_t luIndex = 0; luIndex < KU_NUMBEROFBITFIELDS; luIndex++)
    {
        if(maxBits[luIndex] != 0)
        {
            return false;
        }
    }
    return true;
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetInverse( const FastBitArray<NUMBITS>& lrSource )
{
    for(uint64_t luIndex = 0; luIndex < KU_NUMBEROFBITFIELDS; luIndex++)
    {
        maxBits[luIndex] = ~lrSource.maxBits[luIndex];
    }
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetAnd( const FastBitArray<NUMBITS>& lrA, const FastBitArray<NUMBITS>& lrB )
{
    for(uint64_t luIndex = 0; luIndex < KU_NUMBEROFBITFIELDS; luIndex++)
    {
        maxBits[luIndex] = lrA.maxBits[luIndex] & lrB.maxBits[luIndex];
    }
}


template<int NUMBITS>
inline void
FastBitArray<NUMBITS>::SetOr( const FastBitArray<NUMBITS>& lrA, const FastBitArray<NUMBITS>& lrB )
{
    for(uint64_t luIndex = 0; luIndex < KU_NUMBEROFBITFIELDS; luIndex++)
    {
        maxBits[luIndex] = lrA.maxBits[luIndex] | lrB.maxBits[luIndex];
    }
}

// ===== Not in the DWARF: the tree's scan helpers (declared after End above) =====
// Lowest set bit, or KI_INVALID_BIT_INDEX. A raw field test, no assert: these replace an
// inlined Iterator walk in their callers, which keep their own record of that.
template<int NUMBITS>
inline int32_t
FastBitArray<NUMBITS>::GetFirstBitSet() const
{
    for (uint32_t luIndex = 0; luIndex < static_cast<uint32_t>(NUMBITS); ++luIndex)
    {
        if ((maxBits[luIndex / 64] & (static_cast<uint64_t>(1) << (luIndex & 63))) != 0)
        {
            return static_cast<int32_t>(luIndex);
        }
    }
    return KI_INVALID_BIT_INDEX;
}

// Lowest set bit strictly after liAfter, or KI_INVALID_BIT_INDEX.
template<int NUMBITS>
inline int32_t
FastBitArray<NUMBITS>::GetNextBitSet(int32_t liAfter) const
{
    for (int32_t liBit = liAfter + 1; static_cast<uint32_t>(liBit) < static_cast<uint32_t>(NUMBITS); ++liBit)
    {
        const uint32_t luIndex = static_cast<uint32_t>(liBit);
        if ((maxBits[luIndex / 64] & (static_cast<uint64_t>(1) << (luIndex & 63))) != 0)
        {
            return liBit;
        }
    }
    return KI_INVALID_BIT_INDEX;
}

} // namespace CgsContainers

// EVIDENCE NOTES (kept below the original lines).
// - Iterator::operator++ is the LegacySrc body line for line (its asserts at +2/+3/+34/+36/
//   +66/+68 are ARTIST's 250/251/282/284/314/316), with two changes ARTIST and DecFIGS share:
//   the source masks are const (DWARF h:92) and the field walk advances liCurrIndex, not
//   miIndex (ARTIST ChallengeManager::CountCompletedChallenges @0x8233E908 `addi r21,r21,0x40`
//   on the value later stored to miIndex, while the 316 assert @0x8233E9A4 still compares the
//   unchanged member; DecFIGS operator++ @0x96CA74 `v13 += 64`). The PS3 __builtin_clzll is
//   the XDK's _CountLeadingZeros64 (unsigned result: ARTIST compares with cmplw @0x8233E788).
// - The checks: GetIndex 235 and operator++ 250 "Attempt to get index when out of range\n"
//   (@0x82023FE0, both bounds: e.g. 0x8273F318/0x8273F320), 251 "No source mask\n"
//   (@0x82024060), 282/314 "Internal mask has wrapped - expected to find valid bit\n"
//   (@0x82024028), 284/316 "Index has gone out of range\n" (@0x82024008), GetMask 374
//   "Attempt to get mask when out of range\n" (@0x8202D640), SetAll 494 "Bit field index out
//   of range\n" (@0x82072998, OptionsDataProfile::Construct @0x824FF650). IsBitSet 396,
//   SetBit 431, UnSetBit 452 stream "Index " << i << " is out of range (max bits: " <<
//   NUMBITS << "\n" (@0x82018530, @0x82018510, @0x82001CC4; both ints through
//   operator<<(int32_t) 0x821F0E50: BurnoutSkillzManager::SendUpdatePlayerSkillsEvent
//   @0x82322A64/0x82322A8C), as the iterator overloads do at 415/442/463, reading the index
//   through GetIndex() each time (Vehicle::SetCollidable @0x8271BB30: three inlined 235
//   checks around the 415 assert, then GetMask's 374).
// - The loops count fields in uint64_t (ARTIST cmpldi, e.g. IsZero @0x824F9DA0, SetInverse/
//   SetAnd @0x8272FAAC/0x8272FAE4); ConstructOrphan masks with KU_BITSINBITFIELDMASK
//   (TrafficEntityModule::GenerateCrashedVehicleEvents @0x82720674 `clrldi r11,r31,58`).
// - Iterator() follows LegacySrc (all three members zeroed); no ARTIST site observes it.
