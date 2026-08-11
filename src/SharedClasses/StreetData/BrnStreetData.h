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
// Member layout pinned to the X360 binary offsets (which match the DecFIGS
// version-5 DWARF for BrnStreetData::StreetData exactly):
//   miVersion +0, miSize +4, mpaStreets +8, mpaJunctions +12, mpaRoads +16,
//   mpaChallengeParScores +20, miStreetCount +24, miJunctionCount +28,
//   miRoadCount +32.
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
        void        FixDown( int liDelta );
        void        FixUp( int liDelta );
    private:
        Exit*   mpaExits;                // +12
        int32_t miExitCount;             // +16
        char    macName[KI_MAX_NAME];    // +20
    };

    // -- Road (64 bytes) -- stride for GetRoad (i<<6 == 64*i) -------------
    // maReferencePosition is rw::math::fpu::Vector3Template<float> in the real
    // build; modelled here as a 12-byte placeholder so Road is byte-exact 64
    // without dragging the RenderWare math header into this slice.
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
        void        FixDown( int liDelta );
        void        FixUp( int liDelta );
    private:
        f32            maReferencePosition[3];   // +0  (Vector3Template<float>)
        SpanIndex*     mpaSpans;                 // +12
        CgsID          mId;                       // +16 (8-byte aligned)
        CgsID          miRoadLimitId0;            // +24
        CgsID          miRoadLimitId1;            // +32
        char           macDebugName[KI_MAX_NAME]; // +40
        ChallengeIndex mChallenge;                // +56
        int32_t        miSpanCount;               // +60  (sizeof == 64)
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

        // X360 BrnStreetDataResourceType.cpp calls FixUp/FixDown with an int
        // delta (rw::Resource load base); keep the committed stub's signature.
        int FixUp( int liDelta );
        int FixDown( int liDelta );

    private:
        JunctionIndex GetJunctionIndexFromSpanIndex( SpanIndex liIndex );
        StreetIndex   GetStreetIndexFromSpanIndex( SpanIndex liIndex );
        SpanIndex     GetSpanIndexFromJunction( JunctionIndex liIndex );
        StreetIndex   GetSpanIndexFromStreet( StreetIndex liIndex );

        int32_t                  miVersion;              // +0
        int32_t                  miSize;                 // +4
        Street*                  mpaStreets;             // +8
        Junction*                mpaJunctions;           // +12
        Road*                    mpaRoads;               // +16
        ChallengeParScoresEntry* mpaChallengeParScores;  // +20
        int32_t                  miStreetCount;          // +24
        int32_t                  miJunctionCount;        // +28
        int32_t                  miRoadCount;            // +32
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
    // Offsets are the ones the class already pins: Road::mId +16, Road::macDebugName +40,
    // StreetData::miRoadCount +32.
    inline CgsID       Road::GetId() const          { return mId; }
    inline const char* Road::GetDebugName() const   { return macDebugName; }
    inline int32_t     StreetData::GetRoadCount() const { return miRoadCount; }
}

#endif // BRN_STREET_DATA_H
