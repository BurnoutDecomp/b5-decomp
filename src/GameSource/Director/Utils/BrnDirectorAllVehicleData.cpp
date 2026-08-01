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
// ⛔ GetPlayer @0x82205C58 / GetRaceCar @0x82205DE8 ARE RECOVERED BUT NOT BODIED HERE, and
// the reason is a NAMESPACE FORK, not missing asm. Both are three asserts then one indexed
// read at the 0x4F0 VehicleInfo stride (`mulli rN, rIdx, 0x4F0` + the mpRaceCars base):
//
//   GetPlayer:   assert(mpRaceCars != NULL)                                        // h:157
//                assert(mePlayerRaceCarIndex < 8)   <- SIGNED cmpwi                // h:158
//                assert(mUsedRaceCars.IsBitSet(mePlayerRaceCarIndex))              // h:159
//                return mpRaceCars[mePlayerRaceCarIndex];
//   GetRaceCar:  the same three, on leRaceCarIndex                                 // h:170/:171/:172
//                return mpRaceCars[leRaceCarIndex];
//
// (The third assert in each is IsBitSet's OWN inlined CgsBitArray.h:203 tripwire, which is an
// UNSIGNED cmplwi -- the two index guards are deliberately different comparisons.)
//
// WHY NOT BODIED: `mpRaceCars` is declared `const BrnDirector::VehicleInfo*` -- a
// forward-declaration in the WRONG NAMESPACE. The real type is
// BrnDirector::Camera::VehicleInfo (Camera/SharedIO/BrnPlayerInfo.h), so the declared pointee
// is an incomplete type that will never be completed, and indexing it is a hard C2036. This is
// the SAME fork BrnArbStateCarSelect.cpp already documents for ArbStateSharedInfo::mpPlayerCar
// ("the shared header's forward declaration is in the wrong namespace"), and it has to be
// fixed once, for the whole AllVehicleData / ArbStateSharedInfo surface, rather than papered
// over per call site with a reinterpret_cast.
// DELETE-WHEN: AllVehicleData's VehicleInfo forward declaration is re-pointed at
// BrnDirector::Camera::VehicleInfo -> then transcribe the two bodies above verbatim.
// ============================================================================

// ----------------------------------------------------------------------------
// AllVehicleData::GetNearestRaceCarIndexToPlayer @0x82233380
//
// The rank-th nearest car's active index. Base pointer in the asm is `this + 0xD4`
// (maNearestRaceCarsToPlayer), its count word at +0x60, and `this + 0x138` is
// mbSorteddNearestRaceCarsToPlayer -- all three land exactly on this class's committed
// members, which is what pins the identity.
//
// ⚠️ The clamp happens BEFORE the sort, and the range compare is UNSIGNED (`cmplw`), so a
// rank at or past the live count collapses onto the last row rather than wrapping.
// The three extra "Array used before Construct/Clear was called" (CgsArray.h:336) asserts in
// the asm are GetLength()'s OWN inlined tripwire, once per call above -- not separate logic.
// ----------------------------------------------------------------------------
EActiveRaceCarIndex AllVehicleData::GetNearestRaceCarIndexToPlayer(u32 luRank) const
{
    CGS_ASSERT(maNearestRaceCarsToPlayer.GetLength() > 0,
               "maNearestRaceCarsToPlayer.GetLength() > 0");                                // h:186

    if (luRank >= static_cast<u32>(maNearestRaceCarsToPlayer.GetLength()))
    {
        luRank = static_cast<u32>(maNearestRaceCarsToPlayer.GetLength()) - 1u;
    }

    if (!mbSorteddNearestRaceCarsToPlayer)
    {
        CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
        mbSorteddNearestRaceCarsToPlayer = true;
    }

    return maNearestRaceCarsToPlayer.GetItem(luRank).meRaceCarIndex;
}

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
