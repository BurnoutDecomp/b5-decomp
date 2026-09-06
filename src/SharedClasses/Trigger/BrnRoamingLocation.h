#ifndef BRN_ROAMING_LOCATION_H
#define BRN_ROAMING_LOCATION_H

#include "types.hpp"          // fixed-width ints
#include "BrnCommonTypes.h"   // Vector3

// SharedClasses/Trigger/BrnRoamingLocation.h
//
// Owning header for `BrnTrigger::RoamingLocation` -- one authored "roaming" point in a
// district. The shipped Paradise TriggerData carries 139 of them (TriggerData::
// mpRoamingLocations @0x64 / miRoamingLocationCount @0x68).
//
// [progression wave: lifecycle, 2026-09-06] Created because BrnProgression::
// ProgressionManager::SetupRoamingSections @0x8236FE60 -- one of the five calls the console's
// ProgressionManager::Prepare2 @0x8239DC98 makes -- walks this table by value and needs the
// COMPLETE type; BrnTriggerData.h had only the forward declaration (`struct RoamingLocation;`).
//
// SHAPE is the DecFIGS DWARF's, verbatim (references/DecFIGS/dwarfdump/SharedClasses/Trigger/
// BrnRoamingLocation.h: `Vector3 mPosition; uint8_t muDistrictIndex;` plus Construct /
// GetPosition / GetDistrict / FixDown / FixUp). It is confirmed store-for-store by the X360
// asm of SetupRoamingSections, which is the only reconstructed consumer:
//   0x8236FF48  lbz  r11, 0x10(r31)   -- muDistrictIndex at +0x10, read as an UNSIGNED byte and
//                                        compared against the 0..17 district loop counter
//   0x8236FFA0  lvx128 v127, r0, r31  -- the position is the 16-byte-aligned Vector3 at +0x00,
//                                        loaded WHOLE into a vector register and handed to
//                                        AISectionsData::FindNearestAISection
//   0x8236FFCC  addi r30, r30, 0x20   -- the record stride is 32 bytes, i.e. the Vector3's own
//                                        16-byte alignment padding the {16 + 1} body up to 0x20
// So the two members and the stride are attested; nothing here is inferred.
//
// MINIMAL SCOPE, same rule as the sibling BrnLandmark.h / BrnRegion.h: the accessors the X360
// build INLINES (GetPosition / GetDistrict) get inline bodies; Construct / FixDown / FixUp have
// their own TUs and are declaration-only. GetDistrict's DWARF return type is BrnWorld::EDistrict;
// it is spelled u8 here so this header does not drag in BrnWorldRegion.h -- the only consumer
// compares it against a plain district index. Single owner: grow here, do not fork.

namespace BrnTrigger
{

struct RoamingLocation
{
public:
    // Out-of-line (own TUs) -- declaration only. The DWARF's Construct takes
    // (Vector3, BrnWorld::EDistrict).
    void Construct( Vector3 lPosition, s32 leDistrict );
    void FixDown();
    void FixUp();

    // Inlined in the X360 build (no standalone symbols in the ledger).
    inline Vector3 GetPosition() const;
    inline u8      GetDistrict() const;

private:
    Vector3 mPosition;         // +0x00 (DWARF BrnRoamingLocation.h:65)
    u8      muDistrictIndex;   // +0x10 (DWARF BrnRoamingLocation.h:66)
};

inline Vector3
RoamingLocation::GetPosition() const
{
    return mPosition;
}

inline u8
RoamingLocation::GetDistrict() const
{
    return muDistrictIndex;
}

} // namespace BrnTrigger

#endif // BRN_ROAMING_LOCATION_H
