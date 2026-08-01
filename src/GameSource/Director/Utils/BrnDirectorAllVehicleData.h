#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Matrix44Affine
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<8>
#include "GameShared/GameClasses/Algorithms/CgsBubbleSort.h" // CgsAlgorithms::BubbleSort
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the inline accessors' tripwires)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h" // BrnDirector::Camera::VehicleInfo (mpRaceCars' real pointee)

#include <cfloat>   // FLT_MAX -- the no-opposing-car sentinel (XEX rodata flt_8200173C,
                    // read as 0x7F7FFFFF == 3.4028235e+38 == FLT_MAX)

// BrnDirector::AllVehicleData - the director's per-frame view of every live
// vehicle (player spaces, the race-car table, traffic, and the sorted
// nearest-cars-to-player list). Class shape / member names / method set verbatim
// from the DecFIGS DWARF (BrnDirectorAllVehicleData.h:45/:108/:121-:139); gated
// on the X360 ledger. This TU bodies the two nearest-car distance queries; the
// rest of the surface is declared-only (their own ledger functions).
//
// RECONCILED (2026-07) with the earlier minimal slice this header replaced: the
// slice's NearestCarInfo field names (miVehicleIndex/mfDistanceSquared/
// muTeamValue, roles read off the Update @0x8221D938 caller) are superseded by
// the DWARF names below (same 12-byte element, pinned by the static_asserts);
// its NearestCarInfoArray typedef and the Append-instantiation anchor
// (@0x821FBA48, the BrnDirectorAllVehicleData.cpp FILE TU) are KEPT; its
// GetNearestRaceCarIndexToPlayer (@0x82233380) / GetRaceCar (@0x82205DE8)
// decls fold into the DWARF-typed ones below (the RaceIntro consumer reads the
// race car's +0x220 position lane through the returned record).
namespace BrnTraffic { namespace BrnTrafficIO { struct TrafficDirectorEntity; } }

namespace BrnDirector
{
    // ⛔ RETIRED (2026-08-01) -- A NAMESPACE FORK, not a missing type. This header used to
    // forward-declare `BrnDirector::VehicleInfo`, but no such type exists: the real one is
    // BrnDirector::Camera::VehicleInfo (Camera/SharedIO/BrnPlayerInfo.h, included above).
    // The declared pointee was therefore an incomplete type that could never be completed, so
    // indexing mpRaceCars was a hard C2036 and GetPlayer/GetRaceCar could not be bodied at all.
    // Same fork BrnArbStateCarSelect.cpp documents for ArbStateSharedInfo::mpPlayerCar; fixed
    // once here for the whole surface rather than per call site with a reinterpret_cast.
    // (Spelled out as Camera::VehicleInfo everywhere below rather than typedef'd back into
    // BrnDirector -- a namespace-scope typedef of that name would clash with the surviving
    // `struct VehicleInfo;` forward declarations in the sibling director headers.)

    struct AllVehicleData
    {
        // DWARF :108 -- one sorted nearest-car row. X360-DIVERGENCE NOTE: the PS3
        // DWARF lists only {meRaceCarIndex, mfDistance}, but the X360 element
        // stride is 12 (the Array<...,8> count word sits at +0x60 == 8*12) and
        // SqDistanceOfNearestOpposingTeamMember compares a third word @+8 against
        // the caller-supplied team -- the X360 record carries the row car team id.
        struct NearestCarInfo
        {
            EActiveRaceCarIndex meRaceCarIndex;   // :116  +0x0
            f32                 mfDistance;       // :117  +0x4 (squared distance)
            s32                 miTeam;           // X360 +0x8 (FLAG: name inferred; absent from the PS3 DWARF)

            // :111 -- the sort order BubbleSort uses. BODIED INLINE 2026-08-01: the sort
            // (CgsAlgorithms::BubbleSort<T,N> @0x82213F80) compares element +0x04, i.e.
            // mfDistance, and produces the ASCENDING nearest-first order both distance
            // queries above rely on. It has no standalone X360 symbol -- the console inlines
            // the compare straight into the sort's inner loop -- so a one-line member
            // comparison IS the console shape.
            bool operator>(const NearestCarInfo& lrOther) const
            {
                return mfDistance > lrOther.mfDistance;
            }
        };

        // The 8-slot nearest-car container (inline buffer 8*12 = 0x60 bytes, live
        // count word at +0x60 -- the Append @0x821FBA48 instantiation's element
        // math). Kept from the earlier slice for the .cpp's instantiation anchor.
        typedef Array<NearestCarInfo, 8u> NearestCarInfoArray;

        // ---- DWARF :54-:103 -- declared-only (their own ledger functions) ----
        void Construct();                                                        // :54

        // ⭐ GetPlayer @0x82205C58 / GetRaceCar @0x82205DE8 -- BODIED INLINE HERE, which is
        // where the CONSOLE had them: every assert in both cites this header
        // (BrnDirectorAllVehicleData.h :157/:158/:159 and :170/:171/:172), and a function whose
        // asserts cite a header was defined in that header. Each is three asserts then one
        // indexed read at the VehicleInfo stride (`mulli rN, rIdx, 0x4F0` + the mpRaceCars
        // base). ⚠️ The two index guards are deliberately DIFFERENT comparisons: the explicit
        // range assert is a SIGNED `cmpwi`, while the third is IsBitSet's OWN inlined
        // CgsBitArray.h:203 tripwire, an UNSIGNED `cmplwi`.
        const Camera::VehicleInfo& GetPlayer() const                             // :57
        {
            CGS_ASSERT(mpRaceCars != 0, "mpRaceCars != NULL");                                    // h:157
            CGS_ASSERT(static_cast<s32>(mePlayerRaceCarIndex) < 8, "mePlayerRaceCarIndex < 8");   // h:158
            CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(mePlayerRaceCarIndex)),
                       "mUsedRaceCars.IsBitSet(mePlayerRaceCarIndex)");                           // h:159
            return mpRaceCars[static_cast<s32>(mePlayerRaceCarIndex)];
        }
        const Camera::VehicleInfo& GetRaceCar(EActiveRaceCarIndex leIndex) const  // :61
        {
            CGS_ASSERT(mpRaceCars != 0, "mpRaceCars != NULL");                                    // h:170
            CGS_ASSERT(static_cast<s32>(leIndex) < 8, "leRaceCarIndex < 8");                      // h:171
            CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(leIndex)),
                       "mUsedRaceCars.IsBitSet(leRaceCarIndex)");                                 // h:172
            return mpRaceCars[static_cast<s32>(leIndex)];
        }

        void Update(CgsContainers::BitArray<8u> lUsedRaceCars, const Camera::VehicleInfo* lpRaceCars,
                    EActiveRaceCarIndex lePlayerIndex,
                    const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* lpTraffic); // :68
        const Camera::VehicleInfo& GetNearestRaceCarToPlayer(u32 luRank) const;   // :75

        // ⭐ @0x82233380 -- BODIED INLINE HERE for the same reason: its own assert cites
        // BrnDirectorAllVehicleData.h:186. (It was previously bodied out-of-line in the
        // .cpp, which is NOT mounted, so every consumer saw it as an unresolved external.)
        // ⚠️ The clamp happens BEFORE the sort and the range compare is UNSIGNED (`cmplw`), so
        // a rank at or past the live count collapses onto the LAST row rather than wrapping.
        // The extra "Array used before Construct/Clear was called" (CgsArray.h:336) asserts in
        // the asm are GetLength()'s own inlined tripwire, one per call -- not separate logic.
        EActiveRaceCarIndex GetNearestRaceCarIndexToPlayer(u32 luRank) const      // :82
        {
            CGS_ASSERT(maNearestRaceCarsToPlayer.GetLength() > 0,
                       "maNearestRaceCarsToPlayer.GetLength() > 0");                              // h:186

            if (luRank >= maNearestRaceCarsToPlayer.GetLength())
            {
                luRank = maNearestRaceCarsToPlayer.GetLength() - 1u;
            }

            if (!mbSorteddNearestRaceCarsToPlayer)
            {
                CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
                mbSorteddNearestRaceCarsToPlayer = true;
            }

            return maNearestRaceCarsToPlayer.GetItem(luRank).meRaceCarIndex;
        }
        Matrix44Affine GetPlayerImpactSpace() const;                             // :85
        Matrix44Affine GetPlayerHeadingSpace() const;                            // :88
        Matrix44Affine GetPlayerLooseHeadingSpace() const;                       // :91
        const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* GetTraffic() const; // :94
        const Camera::VehicleInfo* GetRaceCars() const;                          // :97
        // BODIED INLINE (2026-07-30). Neither of these two has a standalone X360 export --
        // the console inlines both at every call site (e.g. VehicleRef::IsValid @0x822336A8
        // reads *(world+196) and the 64-bit used-race-car word at world+200 directly, which is
        // exactly mePlayerRaceCarIndex @+0xC4 and mUsedRaceCars @+0xC8 below). An inline
        // one-line member read IS the console shape; nothing is fabricated.
        const CgsContainers::BitArray<8u>& GetUsedRaceCarsBitArray() const        // :100
        {
            return mUsedRaceCars;
        }
        EActiveRaceCarIndex GetPlayerRCIndex() const                             // :103
        {
            return mePlayerRaceCarIndex;
        }

        // ====================================================================
        // ⭐ MOVED HERE FROM BrnDirectorAllVehicleData.cpp (2026-08-01), for the SAME reason
        // the five siblings above were moved: that .cpp is NOT on the build list, so both
        // bodies -- written, and (re)verified against the X360 asm this wave -- were
        // unreachable, and every consumer of them saw an unresolved external for finished
        // code. (Sixth instance of that pattern in this project.)
        //
        // ⚠️ HONEST PROVENANCE, because rule 12's usual test does NOT decide these two.
        // Neither function contains an assert that cites a source file of its own:
        // GetSqDistanceOfNearestCarToPlayer has no assert at all, and the single assert
        // inside SqDistanceOfNearestOpposingTeamMember is GetLength()'s own inlined
        // CgsArray.h:336 tripwire. The header home rests instead on the DecFIGS DWARF's
        // file split for this class: BrnDirectorAllVehicleData.cpp contains EXACTLY TWO
        // definitions there (Construct @cpp:36 and Update @cpp:59) -- every one of the
        // class's twelve accessors is header-defined. Both queries are accessors of that
        // same shape (lazily sort, then read one row), and neither appears anywhere in
        // the PS3 DWARF (they are X360-revision additions), so no DWARF line cites a
        // .cpp for them either. Inline here is behaviourally identical to out-of-line and
        // costs the link nothing; it needs no build-list change.
        // ====================================================================

        // @0x82233488 -- the squared distance of the nearest OTHER car (sorted row 1; row 0
        // is the player itself). VERIFIED store-for-store against the X360 asm this wave:
        //   lbz +0x138 -> if unsorted: BubbleSort(&maNearest.. @+0xD4), stb 1 @+0x138;
        //   then operator[](array, 1) and `lfs f1, 4(r3)` == the row's mfDistance.
        // CONST + mutable sort state: the committed consumers reach these through a
        // `const AllVehicleData*` (ArbStateSharedInfo +0x38), so the lazy first-use sort is
        // modelled with mutable members.
        f32 GetSqDistanceOfNearestCarToPlayer() const
        {
            if (!mbSorteddNearestRaceCarsToPlayer)
            {
                CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
                mbSorteddNearestRaceCarsToPlayer = true;
            }
            return maNearestRaceCarsToPlayer.GetItem(1u).mfDistance;
        }

        // @0x822334E0 -- the squared distance of the nearest car on a different team, or
        // FLT_MAX when every listed car shares liMyTeam.
        //
        // ⚠️ CORRECTED THIS WAVE: the count was read with GetCount(), which is the RAW
        // count accessor and fires no tripwire. The asm re-reads the count word at array
        // +0x60 EVERY iteration and each read carries the "Array used before
        // Construct/Clear was called" assert at CgsArray.h line 336 (`li r5, 0x150`) --
        // that is GetLength()'s inlined tripwire, not GetCount()'s (which has none). The
        // file's own comment already claimed the tripwire re-fired per iteration while the
        // code did not do it. GetLength() also returns u32, matching the console's UNSIGNED
        // `cmplw` bound test, where the old `static_cast<u32>(GetCount())` was a cast.
        //
        // The rest is VERIFIED as it stood: the team word is read at element +0x08 and
        // compared SIGNED (`cmpw`) against the argument; a mismatch re-enters operator[] a
        // SECOND time to read mfDistance (the console really does index twice); the
        // fall-through loads flt_8200173C.
        f32 SqDistanceOfNearestOpposingTeamMember(s32 liMyTeam) const
        {
            if (!mbSorteddNearestRaceCarsToPlayer)
            {
                CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
                mbSorteddNearestRaceCarsToPlayer = true;
            }

            u32 luRow = 1;
            while (luRow < maNearestRaceCarsToPlayer.GetLength())   // CgsArray.h:336 tripwire per pass
            {
                if (maNearestRaceCarsToPlayer.GetItem(luRow).miTeam != liMyTeam)
                {
                    return maNearestRaceCarsToPlayer.GetItem(luRow).mfDistance;
                }
                ++luRow;
            }
            return FLT_MAX;   // flt_8200173C
        }

    private:
        // DWARF :121-:139 order (Matrix44Affine members keep the class 16-aligned).
        Matrix44Affine mPlayerImpactSpace;         // :121  X360 +0x00
        Matrix44Affine mPlayerHeadingSpace;        // :122  +0x40
        Matrix44Affine mPlayerLooseHeadingSpace;   // :123  +0x80
        const Camera::VehicleInfo* mpRaceCars;     // :125  +0xC0 (stride 0x4F0 == sizeof(VehicleInfo))
        EActiveRaceCarIndex mePlayerRaceCarIndex;  // :126  +0xC4
        CgsContainers::BitArray<8u> mUsedRaceCars; // :127  +0xC8
        const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>*
                       mpTrafficVehicleArray;      // :129  +0xD0
        // mutable: the two const distance queries lazily sort on first use (the
        // consumers hold a const pointer; see the query comment above).
        mutable NearestCarInfoArray
                       maNearestRaceCarsToPlayer;  // :133  +0xD4 (count word @+0x134)
        mutable bool   mbSorteddNearestRaceCarsToPlayer;   // :134  +0x138 (DWARF spelling)
        bool           mbShouldUpdateNearestRaceCars;      // :139
    };

    // Pin the X360 12-byte element (kept from the earlier slice; the Append
    // @0x821FBA48 asm copies exactly three 32-bit words at a 12-byte stride).
    static_assert(sizeof(AllVehicleData::NearestCarInfo) == 12, "NearestCarInfo is a 12-byte element");
}
