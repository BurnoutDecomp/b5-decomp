#ifndef BRN_STREET_DATA_H
#define BRN_STREET_DATA_H

// ---------------------------------------------------------------------------
// SharedClasses/StreetData/BrnStreetData.h  (canonical home for BrnStreetData::StreetData)
//
// Full BrnStreetData::StreetData slice for the three inline accessors the X360
// build emitted out-of-line:
//   StreetData::GetRoad              @ 0x8230ECD0
//   StreetData::GetChallengeParScore @ 0x8230ED48
//   StreetData::GetStreet            @ 0x8231BAD0
//
// Member ORDER is pinned to the X360 binary (which matches the DecFIGS version-5
// DWARF for BrnStreetData::StreetData exactly). The console OFFSETS are
//   miVersion +0, miSize +4, mpaStreets +8, mpaJunctions +0xC, mpaRoads +0x10,
//   mpaChallengeParScores +0x14, miStreetCount +0x18, miJunctionCount +0x1C,
//   miRoadCount +0x20   (sizeof 36; strides Street 16 / Junction 36 / Road 64).
//
// ⚠️ THOSE ARE 32-BIT-POINTER OFFSETS AND DO NOT HOLD ON THE x64 HOST. The four
// pointer members widen 4 -> 8 bytes, so every field behind them shifts and the
// element strides grow:
//   miVersion +0, miSize +4, mpaStreets +8, mpaJunctions +0x10, mpaRoads +0x18,
//   mpaChallengeParScores +0x20, miStreetCount +0x28, miJunctionCount +0x2C,
//   miRoadCount +0x30   (sizeof 56; strides Street 16 / Junction 48 / Road 72).
// The _AssertLayout() block at the bottom of this header pins all of it.
//
// This matters beyond the C++: STREETDATA.DAT is loaded IN PLACE -- the resource
// blob IS this object and StreetData::FixUp only adds the load base to the
// serialised offset slots. The bundle converter
// (tools/assets/bundles/nonapt_transcode.py::_street_data) therefore emits the
// NATIVE x64 image above, not the console's compact one. Emitting the console
// layout is what made FixUp read miRoadCount out of street[0] and fire
// "The number of roads in the design data doesn't match the code const" at boot
// (measured 2026-08-11: both the retail X360 file and the converted file carry
// miRoadCount == 64, exactly the code const -- only the field's ADDRESS was wrong).
//
// SpanIndex is int16_t on X360 (GetStreet takes __int16) -- agrees with the
// version-5 DWARF; the Feb-2007 leak's int32_t form is the older build and is
// overridden by the binary.
//
// TAG NOTE: declared `struct` (not `class`) so it agrees with the committed
// SharedClasses/StreetData/BrnStreetDataResourceType.h forward declaration
// `struct StreetData` (avoids the MSVC C4099 struct/class tag mismatch). The
// FixUp/FixDown(int) signatures match that stub's caller contract verbatim.
// ---------------------------------------------------------------------------

#include <cstddef>                                       // offsetof (the _AssertLayout pins)

#include "types.hpp"
#include "BrnCommonTypes.h"                              // CgsID (u64)
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ChallengeData / ChallengeParScoresEntry (shared home; GetChallengeParScore row type)

namespace CgsMemory { class LinearMalloc; }

namespace BrnStreetData
{
    static const int32_t KI_MAX_NAME = 16;

    // X360 widths (== version-5 DWARF). The binary's GetStreet(__int16) pins
    // SpanIndex at 16-bit; RoadIndex/ChallengeIndex stay 32-bit.
    typedef int16_t SpanIndex;
    typedef int32_t RoadIndex;
    typedef int32_t ChallengeIndex;
    typedef int16_t JunctionIndex;
    typedef int16_t StreetIndex;

    extern const SpanIndex KI_JUNCTION_START_INDEX;      // = 15000 (BrnStreetData.cpp)
    extern const SpanIndex KI_INVALID_SPAN_INDEX;
    extern const RoadIndex KI_INVALID_ROAD_INDEX;

    // -- minimal AIInfo (2 bytes) -----------------------------------------
    class AIInfo
    {
    public:
        void Construct( uint8_t luMinSpeed, uint8_t luMaxSpeed );
        void Construct( AIInfo* lpAIInfo );
        void Destruct();
        void GetSpeeds( uint8_t* lpMinSpeed, uint8_t* lpMaxSpeed ) const;
        void FixDown();
        void FixUp();
    private:
        uint8_t muMaxSpeedMPS;   // +0
        uint8_t muMinSpeedMPS;   // +1
    };

    // -- SpanBase (12 bytes) ----------------------------------------------
    class SpanBase
    {
    public:
        enum ESpanType
        {
            STREET = 0,
            JUNCTION = 1,
            SPAN_TYPE_COUNT = 2
        };

        void       Construct( RoadIndex liRoadIndex, SpanIndex liSpanIndex );
        ESpanType  GetSpanType() const;
        RoadIndex  GetRoadIndex() const;
        SpanIndex  GetSpanIndex() const;
        void       FixDown();
        void       FixUp();
    protected:
        RoadIndex  miRoadIndex;  // +0
        SpanIndex  miSpanIndex;  // +4  (int16; +6..+7 padding)
        // +8. MEASURED from the retail X360 image: every junction record carries the
        // bytes `01 00 00 00` here and every street record `00 00 00 00` -- never the
        // `00 00 00 01` a big-endian 4-byte enum would hold. The console stores this
        // enum in the low byte of the slot, so the converter copies the four bytes
        // verbatim and the little-endian host reads back JUNCTION == 1 / STREET == 0.
        ESpanType  meSpanType;   // +8
    };

    // -- Street : SpanBase (16 bytes) -- stride for GetStreet (16*i) ------
    class Street : public SpanBase
    {
    public:
        void          Construct( RoadIndex liRoad, int32_t liSpanIndex, AIInfo* lpAIInfo );
        void          Destruct();
        const AIInfo* GetAIInfo() const;
        void          FixDown();
        void          FixUp();
    private:
        AIInfo mAIInfo;          // +12 (2 bytes; struct padded to 16)
    };

    // -- Junction : SpanBase ----------------------------------------------
    class Junction : public SpanBase
    {
    public:
        class Exit
        {
        public:
            SpanIndex mSpan;     // +0
            float     mrAngle;   // +4
        };

        void        Construct( SpanIndex liSpanIndex, int32_t liExitCount, CgsMemory::LinearMalloc* lpLinearMalloc );
        void        Destruct();
        const char* GetName();
        const Exit* GetExit( int32_t liExitId );
        const Exit* GetExit( float lrAngle );
        int32_t     GetExitCount();

        // The X360 folds these into StreetData::FixDown/FixUp (no standalone symbol for
        // either in the ledger), so a header inline IS the faithful shape -- and it is
        // what lets StreetData walk the table by NAME instead of by a console byte
        // offset. Delta is pointer-width: the x64 resource load base does not fit in an
        // int (same reason CgsLanguageResourceType uses CgsResource::GetLoadBase64).
        void        FixDown( uintptr_t luDelta );
        void        FixUp( uintptr_t luDelta );

        // Compile-time-only layout pin (see the block at the bottom of this header).
        static void _AssertLayout();
    private:
        Exit*   mpaExits;                // +0x10 (console +12)
        int32_t miExitCount;             // +0x18 (console +16)
        char    macName[KI_MAX_NAME];    // +0x1C (console +20)
    };

    // -- Road (console 64 bytes / x64 72) -- the console's GetRoad indexed it
    // with i<<6; on the host it is plain mpaRoads[i] over sizeof(Road) == 72.
    // maReferencePosition is rw::math::fpu::Vector3Template<float> in the real
    // build; modelled here as a 12-byte placeholder without dragging the
    // RenderWare math header into this slice.
    class Road
    {
    public:
        void        Construct( const char* lpacDebugName,
                               CgsID lId, CgsID lRoadLimitId0, CgsID lRoadLimitId1,
                               int32_t liSpanCount, int32_t liChallenge,
                               CgsMemory::LinearMalloc* lpLinearMalloc );
        void        Destruct();
        CgsID       GetId() const;
        CgsID       GetRoadLimitId0() const;
        CgsID       GetRoadLimitId1() const;
        SpanIndex   GetSpanIndex( int32_t liSequence ) const;
        void        SetSpanIndex( int32_t liSequence, int32_t liSpanIndex );
        ChallengeIndex GetChallengeIndex() const;
        const char* GetDebugName() const;
        int32_t     GetSpanCount() const;

        // Header-inline for the same reason as Junction::FixDown/FixUp above
        // (X360-folded into StreetData::FixDown/FixUp; no ledger symbol).
        void        FixDown( uintptr_t luDelta );
        void        FixUp( uintptr_t luDelta );

        // Compile-time-only layout pin (see the block at the bottom of this header).
        static void _AssertLayout();
    private:
        f32            maReferencePosition[3];   // +0    (Vector3Template<float>)
        SpanIndex*     mpaSpans;                 // +0x10 (console +12)
        CgsID          mId;                       // +0x18 (console +16)
        CgsID          miRoadLimitId0;            // +0x20 (console +24)
        CgsID          miRoadLimitId1;            // +0x28 (console +32)
        char           macDebugName[KI_MAX_NAME]; // +0x30 (console +40)
        ChallengeIndex mChallenge;                // +0x40 (console +56)
        int32_t        miSpanCount;               // +0x44 (console +60; sizeof 72 / 64)
    };

    // -- ChallengeParScoresEntry (40 bytes) -- stride for GetChallengeParScore.
    // The former byte-identical local POD model was retired by the wave-C StreetManager
    // keystone: the type now comes from its canonical shared home,
    // SharedClasses/StreetData/BrnChallengeData.h (ChallengeData base + CgsID mRivals[2]
    // + the Copy/GetScore methods the StreetManager callers spell by name). Same 40-byte
    // layout; included above.

    // -- StreetData -------------------------------------------------------
    struct StreetData
    {
    public:
        friend class StreetDataCompiler;

        static const int32_t KI_VERSION   = 5;   // X360/DWARF version-5 build
        static const int32_t KI_ALIGNMENT = 16;

        void Construct( int32_t liStreetCount, int32_t liJunctionCount,
                        int32_t liRoadCount, CgsMemory::LinearMalloc* lpLinearMalloc );
        void Destruct();
        void Update();

        int32_t      GetRoadCount() const;

        // --- the three reconstructed accessors (bodies below, inline) ---
        const Road*                    GetRoad( RoadIndex liIndex ) const;          // 0x8230ECD0
        const ChallengeParScoresEntry* GetChallengeParScore( ChallengeIndex liIndex ) const; // 0x8230ED48
        const Street*                  GetStreet( SpanIndex liIndex );              // 0x8231BAD0
        // ----------------------------------------------------------------

        const Junction*  GetJunction( SpanIndex liIndex );
        bool             IsJunction( SpanIndex liIndex );
        bool             IsStreet( SpanIndex liIndex );
        const SpanBase*  GetSpan( SpanIndex liIndex );

        int32_t GetSize() const;
        void    SetSize( int32_t liSize );

        // BrnStreetDataResourceType.cpp calls these with the rw::Resource load base.
        // The X360 passes it as an int because its pointers are 32-bit; on the host it
        // is a full-width uintptr_t (CgsResource::GetLoadBase64) -- an int truncates the
        // x64 heap base and would relocate every array to a wild address. void return:
        // the console's `result` is the this-pointer ABI artifact and no caller reads it
        // (same call shape as CgsLanguage::LanguageResourceType::FixUp).
        void FixUp( uintptr_t luDelta );
        void FixDown( uintptr_t luDelta );

        // Compile-time-only layout pin (see the block at the bottom of this header).
        static void _AssertLayout();

    private:
        JunctionIndex GetJunctionIndexFromSpanIndex( SpanIndex liIndex );
        StreetIndex   GetStreetIndexFromSpanIndex( SpanIndex liIndex );
        SpanIndex     GetSpanIndexFromJunction( JunctionIndex liIndex );
        StreetIndex   GetSpanIndexFromStreet( StreetIndex liIndex );

        int32_t                  miVersion;              // +0
        int32_t                  miSize;                 // +4
        Street*                  mpaStreets;             // +8    (console +8)
        Junction*                mpaJunctions;           // +0x10 (console +0xC)
        Road*                    mpaRoads;               // +0x18 (console +0x10)
        ChallengeParScoresEntry* mpaChallengeParScores;  // +0x20 (console +0x14)
        int32_t                  miStreetCount;          // +0x28 (console +0x18)
        int32_t                  miJunctionCount;        // +0x2C (console +0x1C)
        int32_t                  miRoadCount;            // +0x30 (console +0x20)
    };

    // ====================================================================
    // Inline accessor bodies (X360 emitted these out-of-line; the bounds
    // asserts use the project CGS_ASSERT macro -- the X360-baked file/line
    // are discarded per project convention).
    // ====================================================================

    inline const Road*
    StreetData::GetRoad( RoadIndex liIndex ) const
    {
        CGS_ASSERT( liIndex < miRoadCount && liIndex >= 0, "liIndex < miRoadCount && liIndex >= 0" );
        return &mpaRoads[ liIndex ];
    }

    inline const Street*
    StreetData::GetStreet( SpanIndex liIndex )
    {
        CGS_ASSERT( IsStreet( liIndex ), "IsStreet( liIndex )" );
        return &mpaStreets[ liIndex ];
    }

    inline const ChallengeParScoresEntry*
    StreetData::GetChallengeParScore( ChallengeIndex liIndex ) const
    {
        CGS_ASSERT( liIndex < miRoadCount && liIndex >= 0, "liIndex < miRoadCount && liIndex >= 0" );
        return &mpaChallengeParScores[ liIndex ];
    }

    // ---- ADDITIVE (street-data load wave, 2026-08-11) -------------------
    // Three plain field reads that were DECLARED here and defined NOWHERE in the tree
    // (grep: no out-of-line definition in any .cpp), so every caller was a latent
    // unresolved external -- invisible under the per-TU `cl /c` gate, and exactly the
    // "gate-green != linkable" trap. The X360 inlines all three at every call site (no
    // standalone symbol in the ledger for any of them), so a header inline IS the
    // faithful shape; same treatment as GetRoad/GetStreet/GetChallengeParScore above.
    // They read the members the class already pins (Road::mId, Road::macDebugName,
    // StreetData::miRoadCount) by name, so the x64 relayout carries them for free.
    inline CgsID       Road::GetId() const          { return mId; }
    inline const char* Road::GetDebugName() const   { return macDebugName; }
    inline int32_t     StreetData::GetRoadCount() const { return miRoadCount; }

    // ---- element relocation (X360-inlined into StreetData::FixDown/FixUp) ------
    // Each element owns exactly one relocatable slot: Road::mpaSpans and
    // Junction::mpaExits (the console's `lwz r8, 0xC(r9); add/sub delta; stw`). They
    // live here so the StreetData loops address the slot by NAME over the element's
    // own stride, instead of by the console's byte offset and hard-coded 64/36 stride.
    inline void Road::FixDown( uintptr_t luDelta )
    {
        mpaSpans = reinterpret_cast<SpanIndex*>(
            reinterpret_cast<uintptr_t>( mpaSpans ) - luDelta );
    }

    inline void Road::FixUp( uintptr_t luDelta )
    {
        mpaSpans = reinterpret_cast<SpanIndex*>(
            reinterpret_cast<uintptr_t>( mpaSpans ) + luDelta );
    }

    inline void Junction::FixDown( uintptr_t luDelta )
    {
        mpaExits = reinterpret_cast<Exit*>(
            reinterpret_cast<uintptr_t>( mpaExits ) - luDelta );
    }

    inline void Junction::FixUp( uintptr_t luDelta )
    {
        mpaExits = reinterpret_cast<Exit*>(
            reinterpret_cast<uintptr_t>( mpaExits ) + luDelta );
    }

    // ---- x64 layout pins ------------------------------------------------------
    // STREETDATA.DAT is consumed in place, so these offsets are a WIRE CONTRACT with
    // tools/assets/bundles/nonapt_transcode.py::_street_data. Anything that shifts a
    // member here must shift the converter's emitted image in the same step; the two
    // silently disagreeing is what produced the boot-time road-count assert.
    inline void Junction::_AssertLayout()
    {
        static_assert( sizeof( SpanBase ) == 12, "SpanBase is 12 bytes on both targets" );
        static_assert( sizeof( Street )   == 16, "Street stride is 16 (pointer-free)" );
        static_assert( sizeof( Exit )     ==  8, "Junction::Exit stride is 8" );
        static_assert( offsetof( Junction, mpaExits )    == 0x10, "Junction::mpaExits @0x10" );
        static_assert( offsetof( Junction, miExitCount ) == 0x18, "Junction::miExitCount @0x18" );
        static_assert( offsetof( Junction, macName )     == 0x1C, "Junction::macName @0x1C" );
        static_assert( sizeof( Junction ) == 48, "Junction stride is 48 on x64 (console 36)" );
    }

    inline void Road::_AssertLayout()
    {
        static_assert( offsetof( Road, mpaSpans )       == 0x10, "Road::mpaSpans @0x10" );
        static_assert( offsetof( Road, mId )            == 0x18, "Road::mId @0x18" );
        static_assert( offsetof( Road, miRoadLimitId0 ) == 0x20, "Road::miRoadLimitId0 @0x20" );
        static_assert( offsetof( Road, miRoadLimitId1 ) == 0x28, "Road::miRoadLimitId1 @0x28" );
        static_assert( offsetof( Road, macDebugName )   == 0x30, "Road::macDebugName @0x30" );
        static_assert( offsetof( Road, mChallenge )     == 0x40, "Road::mChallenge @0x40" );
        static_assert( offsetof( Road, miSpanCount )    == 0x44, "Road::miSpanCount @0x44" );
        static_assert( sizeof( Road ) == 72, "Road stride is 72 on x64 (console 64)" );
    }

    inline void StreetData::_AssertLayout()
    {
        static_assert( sizeof( ChallengeParScoresEntry ) == 40,
                       "ChallengeParScoresEntry stride is 40 (pointer-free)" );
        static_assert( offsetof( StreetData, mpaStreets )            == 0x08, "mpaStreets @0x08" );
        static_assert( offsetof( StreetData, mpaJunctions )          == 0x10, "mpaJunctions @0x10" );
        static_assert( offsetof( StreetData, mpaRoads )              == 0x18, "mpaRoads @0x18" );
        static_assert( offsetof( StreetData, mpaChallengeParScores ) == 0x20, "mpaChallengeParScores @0x20" );
        static_assert( offsetof( StreetData, miStreetCount )   == 0x28, "miStreetCount @0x28" );
        static_assert( offsetof( StreetData, miJunctionCount ) == 0x2C, "miJunctionCount @0x2C" );
        static_assert( offsetof( StreetData, miRoadCount )     == 0x30, "miRoadCount @0x30" );
        static_assert( sizeof( StreetData ) == 56, "StreetData header is 56 bytes on x64 (console 36)" );
    }
}

#endif // BRN_STREET_DATA_H
