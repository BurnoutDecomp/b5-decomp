#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"

#include <cfloat>   // FLT_MAX (the no-opponent sentinel, XEX rodata @0x8200173C)

// BrnDirector::AllVehicleData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, class:BrnDirector::AllVehicleData):
//   AllVehicleData::GetSqDistanceOfNearestCarToPlayer     @0x82233488
//   AllVehicleData::SqDistanceOfNearestOpposingTeamMember @0x822334E0
// Both lazily bubble-sort the nearest-car table on first use (the
// BubbleSort<NearestCarInfo,8> instantiation is its own ledger function); the
// element accesses go through the committed Array::GetItem (the X360 de-inlines
// it per instantiation, with the CgsArray.h construct/bounds tripwires).

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// Out-of-line anchor KEPT from the earlier slice of this file (the
// BrnDirectorAllVehicleData.cpp FILE TU): forces the Array<NearestCarInfo,8>::
// Append instantiation @0x821FBA48 (Append only -- the X360 instantiated just
// that member for this element type; a whole-class explicit instantiation would
// drag in Contains/FindFirstInstanceOf, which need NearestCarInfo::operator==).
// Append is called by AllVehicleData::Update @0x8221D938 to push each computed
// nearest-car record into the snapshot's fixed 8-slot array.
// ----------------------------------------------------------------------------
void AllVehicleData_AppendNearestCarAnchor(
    AllVehicleData::NearestCarInfoArray& lrArray,
    const AllVehicleData::NearestCarInfo& lrInfo)
{
    lrArray.Append(lrInfo);
}

// @ 0x82233488 -- ensure sorted, then row 1 squared distance (row 0 == the player).
f32 AllVehicleData::GetSqDistanceOfNearestCarToPlayer() const
{
    if (!mbSorteddNearestRaceCarsToPlayer)
    {
        CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
        mbSorteddNearestRaceCarsToPlayer = true;
    }
    return maNearestRaceCarsToPlayer.GetItem(1u).mfDistance;
}

// @ 0x822334E0 -- ensure sorted, then scan rows 1..count-1 for the first car on a
// different team (the count is re-read -- and its construct tripwire re-fired --
// each iteration, exactly as the X360 inlines the Array count access). FLT_MAX
// (the XEX rodata sentinel @0x8200173C) when no opposing car is listed.
f32 AllVehicleData::SqDistanceOfNearestOpposingTeamMember(s32 liMyTeam) const
{
    if (!mbSorteddNearestRaceCarsToPlayer)
    {
        CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
        mbSorteddNearestRaceCarsToPlayer = true;
    }

    u32 luRow = 1;
    while (luRow < static_cast<u32>(maNearestRaceCarsToPlayer.GetCount()))
    {
        if (maNearestRaceCarsToPlayer.GetItem(luRow).miTeam != liMyTeam)
            return maNearestRaceCarsToPlayer.GetItem(luRow).mfDistance;
        ++luRow;
    }
    return FLT_MAX;
}

// ============================================================================
// ✅ RESOLVED (2026-08-01) -- GetPlayer @0x82205C58, GetRaceCar @0x82205DE8 and
// GetNearestRaceCarIndexToPlayer @0x82233380 now live INLINE IN THE HEADER, which is where
// the console had them: every assert in all three cites BrnDirectorAllVehicleData.h
// (:157/:158/:159, :170/:171/:172, :186), and a function whose asserts cite a header was
// defined in that header. Moving GetNearestRaceCarIndexToPlayer out of this .cpp also fixes a
// real defect: this TU is NOT on the build list, so every consumer of it (BehaviourIceAnim,
// the arbitrator states) saw an unresolved external for a function that was already written.
//
// The blocker that had kept GetPlayer/GetRaceCar unbodied was a NAMESPACE FORK, not missing
// asm: `mpRaceCars` was declared `const BrnDirector::VehicleInfo*`, a forward declaration of a
// type that does not exist (the real one is BrnDirector::Camera::VehicleInfo), so the pointee
// was permanently incomplete and indexing it was a hard C2036. The header now names the real
// type. See BrnDirectorAllVehicleData.h for the bodies and their asm provenance.
// ============================================================================

}

// ---- Array<NearestCarInfo,8>::operator[] const (X360 0x821FF4E0) --------------------
// The two const distance queries above index the nearest-car table through
// maNearestRaceCarsToPlayer.GetItem(i) (== const operator[]); the X360 de-inlines the const
// checked accessor out of line. Count word @+0x60 (8*12), the unconstructed tripwire
// (CgsArray.h:556) + dynamic out-of-bounds check (CgsArray.h:557), then &maElements[index] at
// the 12-byte NearestCarInfo stride (slwi/add/slwi == 12*i). Lines 556/557 == the CONST
// operator[] overload (CgsArray.h line map). Force just the const overload the GetItem const
// bodies reach; the non-const copy is not this TU's symbol.
// (Explicit instantiation of the global-namespace Array<T,N> template must sit at global
// scope, not inside namespace BrnDirector.)
template
const BrnDirector::AllVehicleData::NearestCarInfo&
Array<BrnDirector::AllVehicleData::NearestCarInfo, 8>::operator[](u32) const;
