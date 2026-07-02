#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Matrix44Affine
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<8>
#include "GameShared/GameClasses/Algorithms/CgsBubbleSort.h" // CgsAlgorithms::BubbleSort

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
    struct VehicleInfo;   // pointer/reference-only here (own home)

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

            bool operator>(const NearestCarInfo& lrOther) const;   // :111 (own ledger fn; the sort order)
        };

        // The 8-slot nearest-car container (inline buffer 8*12 = 0x60 bytes, live
        // count word at +0x60 -- the Append @0x821FBA48 instantiation's element
        // math). Kept from the earlier slice for the .cpp's instantiation anchor.
        typedef Array<NearestCarInfo, 8u> NearestCarInfoArray;

        // ---- DWARF :54-:103 -- declared-only (their own ledger functions) ----
        void Construct();                                                        // :54
        const VehicleInfo& GetPlayer() const;                                    // :57
        const VehicleInfo& GetRaceCar(EActiveRaceCarIndex leIndex) const;        // :61 (@0x82205DE8; the race-intro consumer reads the car's +0x220 position lane)
        void Update(CgsContainers::BitArray<8u> lUsedRaceCars, const VehicleInfo* lpRaceCars,
                    EActiveRaceCarIndex lePlayerIndex,
                    const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* lpTraffic); // :68
        const VehicleInfo& GetNearestRaceCarToPlayer(u32 luRank) const;          // :75
        EActiveRaceCarIndex GetNearestRaceCarIndexToPlayer(u32 luRank) const;    // :82 (@0x82233380)
        Matrix44Affine GetPlayerImpactSpace() const;                             // :85
        Matrix44Affine GetPlayerHeadingSpace() const;                            // :88
        Matrix44Affine GetPlayerLooseHeadingSpace() const;                       // :91
        const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* GetTraffic() const; // :94
        const VehicleInfo* GetRaceCars() const;                                  // :97
        const CgsContainers::BitArray<8u>& GetUsedRaceCarsBitArray() const;      // :100
        EActiveRaceCarIndex GetPlayerRCIndex() const;                            // :103

        // @0x82233488 (this class TU; body in BrnDirectorAllVehicleData.cpp) -- the
        // squared distance of the nearest OTHER car (sorted row 1; row 0 is the
        // player itself). CONST + mutable sort state: the committed consumers reach
        // these through a `const AllVehicleData*` (ArbStateSharedInfo +0x38), so
        // the lazy first-use sort is modelled with mutable members.
        f32 GetSqDistanceOfNearestCarToPlayer() const;

        // @0x822334E0 (this class TU) -- the squared distance of the nearest car on
        // a different team, or FLT_MAX when every listed car shares liMyTeam.
        f32 SqDistanceOfNearestOpposingTeamMember(s32 liMyTeam) const;

    private:
        // DWARF :121-:139 order (Matrix44Affine members keep the class 16-aligned).
        Matrix44Affine mPlayerImpactSpace;         // :121  X360 +0x00
        Matrix44Affine mPlayerHeadingSpace;        // :122  +0x40
        Matrix44Affine mPlayerLooseHeadingSpace;   // :123  +0x80
        const VehicleInfo* mpRaceCars;             // :125  +0xC0
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
