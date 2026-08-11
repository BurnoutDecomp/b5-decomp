#include "SharedClasses/StreetData/BrnStreetData.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnStreetData::StreetData::FixDown @ 0x8267EDE0
//   BrnStreetData::StreetData::FixUp   @ 0x8267EE90
//
// Both relocate the serialised resource against the load-base delta (asm: r4 / a2).
// FixDown converts in-memory POINTERS to file OFFSETS by subtracting the delta;
// FixUp converts file OFFSETS back to POINTERS by adding it and additionally
// re-initialises the per-challenge dirty/valid bitfields.
//
// ⚠️ x64 WIDENING (fixed 2026-08-11 -- this is what fired the boot assert
// "The number of roads in the design data doesn't match the code const").
// The console walks these tables with literal byte strides (`+= 0x40` roads,
// `+= 0x24` junctions, `+= 0x28` challenges) and a literal `0xC(r9)` element
// pointer slot. Those are 32-BIT-POINTER numbers. On the host every pointer
// member widens to 8 bytes, so Junction is 48 bytes (not 36), Road is 72 (not
// 64), Road::mpaSpans/Junction::mpaExits sit at +0x10 (not +0xC) and the
// StreetData header's own miRoadCount moves from +0x20 to +0x30. Transcribing
// the console constants made every walk here address the wrong bytes.
//
// So the loops below index the arrays BY ELEMENT (`mpaRoads[i]`) and delegate the
// element's own relocation to Road::FixUp / Junction::FixUp -- the helpers the
// X360 compiler inlined into this very function (neither has a standalone symbol
// in the ledger). Same stores, addressed by name; the strides can no longer drift
// from the layout, and BrnStreetData.h::_AssertLayout pins that layout against the
// bundle converter that produces the file.
//
// The delta is a uintptr_t for the same reason: it is the resource load base, and
// on x64 that does not fit in the console's int (see CgsResource::GetLoadBase64).

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

    // Asserted road count baked into the code (asm: `cmpwi r7, 0x40`, and the assert
    // message itself prints the same literal via `li r4, 0x40`). Console-faithful.
    static const s32 KI_DESIGN_ROAD_CONST = 64;

    // @ 0x8267EDE0 — in-memory pointers -> file offsets (subtract the delta).
    void StreetData::FixDown( uintptr_t luDelta )
    {
        // Road loop: relocate each road's span-pointer slot.
        for ( s32 liRoad = 0; liRoad < miRoadCount; ++liRoad )
        {
            mpaRoads[ liRoad ].FixDown( luDelta );
        }

        // Junction loop: relocate each junction's exit-pointer slot.
        for ( s32 liJunction = 0; liJunction < miJunctionCount; ++liJunction )
        {
            mpaJunctions[ liJunction ].FixDown( luDelta );
        }

        // Relocate the four array-base pointers (mpaStreets/mpaJunctions/mpaRoads/
        // mpaChallengeParScores). The asm rebases mpaRoads (+0x10) last among the
        // loads but the stores are independent; net effect is each -= the delta.
        mpaStreets            = reinterpret_cast<Street*>(
            reinterpret_cast<uintptr_t>( mpaStreets ) - luDelta );
        mpaJunctions          = reinterpret_cast<Junction*>(
            reinterpret_cast<uintptr_t>( mpaJunctions ) - luDelta );
        mpaRoads              = reinterpret_cast<Road*>(
            reinterpret_cast<uintptr_t>( mpaRoads ) - luDelta );
        mpaChallengeParScores = reinterpret_cast<ChallengeParScoresEntry*>(
            reinterpret_cast<uintptr_t>( mpaChallengeParScores ) - luDelta );
    }

    // @ 0x8267EE90 — file offsets -> in-memory pointers (add the delta).
    void StreetData::FixUp( uintptr_t luDelta )
    {
        CGS_ASSERT( miVersion == KI_VERSION, "StreetData version mismatch" );

        // Relocate the four array-base pointers (+= the delta) before the element loops.
        mpaStreets            = reinterpret_cast<Street*>(
            reinterpret_cast<uintptr_t>( mpaStreets ) + luDelta );
        mpaJunctions          = reinterpret_cast<Junction*>(
            reinterpret_cast<uintptr_t>( mpaJunctions ) + luDelta );
        mpaRoads              = reinterpret_cast<Road*>(
            reinterpret_cast<uintptr_t>( mpaRoads ) + luDelta );
        mpaChallengeParScores = reinterpret_cast<ChallengeParScoresEntry*>(
            reinterpret_cast<uintptr_t>( mpaChallengeParScores ) + luDelta );

        CGS_ASSERT( miRoadCount == KI_DESIGN_ROAD_CONST,
                    "The number of roads in the design data doesn't match the code const" );

        // Road loop: relocate each road's span-pointer slot.
        for ( s32 liRoad = 0; liRoad < miRoadCount; ++liRoad )
        {
            mpaRoads[ liRoad ].FixUp( luDelta );
        }

        // Junction loop: relocate each junction's exit-pointer slot.
        for ( s32 liJunction = 0; liJunction < miJunctionCount; ++liJunction )
        {
            mpaJunctions[ liJunction ].FixUp( luDelta );
        }

        // Challenge-par-score loop: re-initialise each entry's dirty/valid bitfields.
        // The asm writes (per 40-byte entry): mValidScores(+8)=0, mDirty(+0)=0,
        // mValidScores(+8)=-1 — i.e. mDirty cleared, all score slots flagged valid.
        //
        // ⚠️ The bound really is miJunctionCount (`lwz r11, 0x1C(r31)` at 0x8267F06C and
        // 0x8267F0A0), NOT the challenge/road count — while the table itself holds
        // miRoadCount entries (StreetData::GetChallengeParScore @0x8230ED48 bounds-checks
        // against miRoadCount). The shipped X360 data proves the console really runs off
        // the end of the table: in the retail STREETDATA.DAT the bytes at
        // challenges + 64*40, +65*40, +66*40 … are already the 8 zero / 8 0xFF pattern
        // these three stores leave behind, baked in by the data compiler's own load pass.
        // Reproduced verbatim rather than "corrected" — see the report note; correcting it
        // would be inventing behaviour the binary does not have.
        for ( s32 liEntry = 0; liEntry < miJunctionCount; ++liEntry )
        {
            ChallengeParScoresEntry& lrEntry = mpaChallengeParScores[ liEntry ];
            lrEntry.mValidScores.UnSetAll();
            lrEntry.mDirty.UnSetAll();
            lrEntry.mValidScores.SetAll();
        }
    }
}
