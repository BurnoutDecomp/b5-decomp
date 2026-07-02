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

}
