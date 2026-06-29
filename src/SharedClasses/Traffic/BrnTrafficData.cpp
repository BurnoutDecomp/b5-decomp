// BrnTraffic::TrafficData runtime getters. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetKillZoneRegions             @ 0x82705D08  (TrafficEntityModule::FireKillZone)
//   GetVehicleTraitsForVehicleType @ 0x82705DF0  (TrafficEntityModule::GenerateNewVehicle,
//                                                  UpdateVehicles_CreateNewVehicles)
//   GetNumPaintColours             @ 0x82705F58  (TrafficEntityModule::RenderTrafficCar)
//
// The X360 bounds-checks each index against the matching count member, streaming a dynamic
// CgsDev::Assert message (BeginAssert / StrStream::Append / FireAssert / EndAssert). Our house
// CGS_ASSERT substitutes that whole sequence; the streamed-in count/index values are dropped
// per the project assert convention, the constant assert text is preserved. Each getter then
// returns a pointer/scalar computed exactly as the asm does (member-by-name; the asm's literal
// element strides 6 / 16 and the scalar read are the natural sizeof of the committed element
// types KillZoneRegion(6) / VehicleTraits(16) and the u8 muNumPaintColours).

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"   // BrnTraffic::Hull (GetHull return layout)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"

namespace BrnTraffic
{
    // @0x82705D08. The X360 asserts the index is in range against muNumKillZoneRegions
    // (`lhz r11, 0x1C(r27)`, the BrnTrafficData.h:194 assert) then returns the addressed
    // region (`add r3, mpaKillZoneRegions, index*6`). Element stride 6 == sizeof(KillZoneRegion).
    const KillZoneRegion* TrafficData::GetKillZoneRegions(u32 luRegion) const
    {
        CGS_ASSERT(luRegion < muNumKillZoneRegions, "Attempt to access kill zone region when max exceeded");
        return &mpaKillZoneRegions[luRegion];
    }

    // @0x82705DF0. Two range checks: first the vehicle type id against muNumVehicleTypes
    // (`lhz 0x16`, BrnTrafficData.h:212), then the traits id read from that type record's +6
    // byte (`lbz 6(r11)` within the 8-byte mpaVehicleTypes element) against muNumVehicleTraits
    // (`lbz 0x19`, BrnTrafficData.h:215). Returns the addressed traits record
    // (`add r3, mpaVehicleTraits, traitsIdx*16`). Element strides 8 / 16 == the sizeof of the
    // committed VehicleTypeData / VehicleTraits element types.
    const VehicleTraits* TrafficData::GetVehicleTraitsForVehicleType(u32 luVehicleType) const
    {
        CGS_ASSERT(luVehicleType < muNumVehicleTypes, "Out of range vehicle type");
        const u32 luTraitsId = mpaVehicleTypes[luVehicleType].muTraitsId;
        CGS_ASSERT(luTraitsId < muNumVehicleTraits, "Out of range vehicle traits in data");
        return &mpaVehicleTraits[luTraitsId];
    }

    // @0x82705F58. Asserts the paint-colour count is non-zero ("muNumPaintColours > 0",
    // BrnTrafficData.h:247 -- the X360 `cmplwi r11,0; bne` fires the assert when the count is 0)
    // then returns the u8 count read at +0x168 (`lbz 0x168`). The DWARF return type is int32_t.
    s32 TrafficData::GetNumPaintColours() const
    {
        CGS_ASSERT(muNumPaintColours != 0, "muNumPaintColours > 0");
        return muNumPaintColours;
    }

    // Thin hull-array accessor over mpapHulls (BrnTraffic::Hull**, X360 +0x0C). The X360 indexes the
    // pointer array inline (`*(mpapHulls + 4*luHull)`, stride 4 = one X360 pointer); modelled here as
    // mpapHulls[luHull] by name. mpapHulls is stored as void* in the committed layout (the Hull layout
    // is not needed to PIN the field), so it is cast back to the real Hull** here.
    const Hull* TrafficData::GetHull(u32 luHull) const
    {
        return reinterpret_cast<Hull* const*>(mpapHulls)[luHull];
    }
}
