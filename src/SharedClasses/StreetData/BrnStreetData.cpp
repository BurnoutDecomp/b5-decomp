#include "SharedClasses/StreetData/BrnStreetData.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnStreetData::StreetData::FixDown @ 0x8267EDE0
//   BrnStreetData::StreetData::FixUp   @ 0x8267EE90
//
// Both relocate the serialised resource against the load-base delta `liDelta`
// (asm: r4 / a2). FixDown converts in-memory POINTERS to file OFFSETS by
// subtracting the delta; FixUp converts file OFFSETS back to POINTERS by adding it
// and additionally re-initialises the per-challenge dirty/valid bitfields.
//
// The StreetData header members are addressed BY NAME (miVersion @+0, miSize @+4,
// mpaStreets @+8, mpaJunctions @+0xC, mpaRoads @+0x10, mpaChallengeParScores @+0x14,
// miStreetCount @+0x18, miJunctionCount @+0x1C, miRoadCount @+0x20 — exactly the asm
// dword offsets). The two element-internal pointer slots the asm also relocates —
// Road::mpaSpans and Junction::mpaExits, both at element byte +0xC — are touched by
// that console byte offset (serialised-payload raw-offset precedent): they are private
// to their element classes and are relocated inline here just as the X360 does, rather
// than routed through the element classes' own Fix* members.

namespace BrnStreetData
{
    // Out-of-line definitions for the namespace-scope sentinels the header declares
    // extern (canonical home = this .cpp, per the version-5 DWARF). Values are the
    // DWARF-attested literals reduced to each typedef's width: KI_INVALID_SPAN_INDEX /
    // KI_INVALID_ROAD_INDEX are the all-ones sentinel (-1 in the signed 16-/32-bit
    // types), KI_JUNCTION_START_INDEX is 15000.
    extern const SpanIndex KI_JUNCTION_START_INDEX = 15000;
    extern const SpanIndex KI_INVALID_SPAN_INDEX   = static_cast<SpanIndex>( 0xFFFFFFFFu );
    extern const RoadIndex KI_INVALID_ROAD_INDEX   = static_cast<RoadIndex>( 0xFFFFFFFFu );

    // Element byte offset of the relocatable pointer slot the loops rewrite (Road
    // +0xC = mpaSpans, Junction +0xC = mpaExits). Asm: `lwz r8, 0xC(r9); +/- delta`.
    static const u32 KU_ELEMENT_PTR_OFFSET = 0x0C;
    // Element strides (asm: road loop `+= 0x40`, junction loop `+= 0x24`, challenge
    // loop `+= 0x28`). These match sizeof(Road)=64, sizeof(Junction)=36,
    // sizeof(ChallengeParScoresEntry)=40.
    static const s32 KI_ROAD_STRIDE      = 64;
    static const s32 KI_JUNCTION_STRIDE  = 36;
    static const s32 KI_CHALLENGE_STRIDE = 40;
    // Asserted road count baked into the code (asm: `cmpwi r7, 0x40`).
    static const s32 KI_DESIGN_ROAD_CONST = 64;

    // @ 0x8267EDE0 — in-memory pointers -> file offsets (subtract liDelta).
    int StreetData::FixDown( int liDelta )
    {
        // Road loop: relocate each road's span-pointer slot (Road +0xC).
        for ( s32 liRoad = 0; liRoad < miRoadCount; ++liRoad )
        {
            u8* lpRoad = reinterpret_cast<u8*>( mpaRoads ) + ( KI_ROAD_STRIDE * liRoad );
            *reinterpret_cast<intptr_t*>( lpRoad + KU_ELEMENT_PTR_OFFSET ) -= liDelta;
        }

        // Junction loop: relocate each junction's exit-pointer slot (Junction +0xC).
        for ( s32 liJunction = 0; liJunction < miJunctionCount; ++liJunction )
        {
            u8* lpJunction = reinterpret_cast<u8*>( mpaJunctions ) + ( KI_JUNCTION_STRIDE * liJunction );
            *reinterpret_cast<intptr_t*>( lpJunction + KU_ELEMENT_PTR_OFFSET ) -= liDelta;
        }

        // Relocate the four array-base pointers (mpaStreets/mpaJunctions/mpaRoads/
        // mpaChallengeParScores). The asm rebases mpaRoads (+0x10) last among the
        // loads but the stores are independent; net effect is each -= liDelta.
        mpaStreets            = reinterpret_cast<Street*>(
            reinterpret_cast<intptr_t>( mpaStreets ) - liDelta );
        mpaJunctions          = reinterpret_cast<Junction*>(
            reinterpret_cast<intptr_t>( mpaJunctions ) - liDelta );
        mpaRoads              = reinterpret_cast<Road*>(
            reinterpret_cast<intptr_t>( mpaRoads ) - liDelta );
        mpaChallengeParScores = reinterpret_cast<ChallengeParScoresEntry*>(
            reinterpret_cast<intptr_t>( mpaChallengeParScores ) - liDelta );

        return liDelta;
    }

    // @ 0x8267EE90 — file offsets -> in-memory pointers (add liDelta).
    int StreetData::FixUp( int liDelta )
    {
        CGS_ASSERT( miVersion == KI_VERSION, "StreetData version mismatch" );

        // Relocate the four array-base pointers (+= liDelta) before the element loops.
        mpaStreets            = reinterpret_cast<Street*>(
            reinterpret_cast<intptr_t>( mpaStreets ) + liDelta );
        mpaJunctions          = reinterpret_cast<Junction*>(
            reinterpret_cast<intptr_t>( mpaJunctions ) + liDelta );
        mpaRoads              = reinterpret_cast<Road*>(
            reinterpret_cast<intptr_t>( mpaRoads ) + liDelta );
        mpaChallengeParScores = reinterpret_cast<ChallengeParScoresEntry*>(
            reinterpret_cast<intptr_t>( mpaChallengeParScores ) + liDelta );

        CGS_ASSERT( miRoadCount == KI_DESIGN_ROAD_CONST,
                    "The number of roads in the design data doesn't match the code const" );

        // Road loop: relocate each road's span-pointer slot (Road +0xC).
        for ( s32 liRoad = 0; liRoad < miRoadCount; ++liRoad )
        {
            u8* lpRoad = reinterpret_cast<u8*>( mpaRoads ) + ( KI_ROAD_STRIDE * liRoad );
            *reinterpret_cast<intptr_t*>( lpRoad + KU_ELEMENT_PTR_OFFSET ) += liDelta;
        }

        // Junction loop: relocate each junction's exit-pointer slot (Junction +0xC).
        for ( s32 liJunction = 0; liJunction < miJunctionCount; ++liJunction )
        {
            u8* lpJunction = reinterpret_cast<u8*>( mpaJunctions ) + ( KI_JUNCTION_STRIDE * liJunction );
            *reinterpret_cast<intptr_t*>( lpJunction + KU_ELEMENT_PTR_OFFSET ) += liDelta;
        }

        // Challenge-par-score loop: re-initialise each entry's dirty/valid bitfields.
        // The asm writes (per 40-byte entry): mValidScores(+8)=0, mDirty(+0)=0,
        // mValidScores(+8)=-1 — i.e. mDirty cleared, all score slots flagged valid.
        // Loop count is miJunctionCount (asm reuses result[7]), not the challenge count.
        // (mDirty/mValidScores are the shared-home CgsContainers::BitArray<2u> members now;
        // UnSetAll/SetAll are each a single whole-word store — the same three stores.)
        for ( s32 liEntry = 0; liEntry < miJunctionCount; ++liEntry )
        {
            ChallengeParScoresEntry& lrEntry =
                *reinterpret_cast<ChallengeParScoresEntry*>(
                    reinterpret_cast<u8*>( mpaChallengeParScores ) + ( KI_CHALLENGE_STRIDE * liEntry ) );
            lrEntry.mValidScores.UnSetAll();
            lrEntry.mDirty.UnSetAll();
            lrEntry.mValidScores.SetAll();
        }

        return liDelta;
    }
}
