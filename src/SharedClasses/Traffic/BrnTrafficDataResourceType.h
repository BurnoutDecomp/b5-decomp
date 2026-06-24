#ifndef BRN_TRAFFIC_DATA_RESOURCE_TYPE_H
#define BRN_TRAFFIC_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficKillZone.h"    // KillZoneRegion (6-byte region element)
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h" // VehicleTypeData (8-byte type element)
#include "SharedClasses/Traffic/BrnTrafficVehicleTraits.h"// VehicleTraits (16-byte traits element)

namespace BrnTraffic
{
struct TrafficData
{
    // ADDITIVE GROW for GetSerialisedResourceDescriptor @ 0x82760660: the X360 reads
    // the serialised byte-size word at +4 (`lwz r11, 4(r30)`) — the total size of the
    // traffic-data block — and asserts it is non-zero ("Uninitialised Traffic Data
    // resource"). The +0 word is read by the type's FixDown path (forwarded to
    // TrafficData::FixDown) but its meaning is not part of this slice.
    // FLAG: muReserved0 (+0) is an opaque serialised header word (DEFERRED); the DWARF
    //       (BrnTrafficData.h:56/59) splits this +0..+3 region into muDataVersion (u8 @+0)
    //       and muNumHulls (u16 @+2). Kept as muReserved0 here to preserve the committed
    //       field's offset/identity (non-additive to re-split); only the +4 size word is
    //       X360-attested by this resource-type slice.
    u32 muReserved0;   // +0  opaque serialised header word (DEFERRED; == muDataVersion+muNumHulls)
    u32 muSizeInBytes; // +4  serialised total byte size (asm `*(a3+4)`)

    // ===== ADDITIVE GROW for the runtime TrafficData getters (own TU) =====
    // X360-authoritative runtime layout from the asm of the three getters bodied in this
    // slice (GetKillZoneRegions @ 0x82705D08, GetVehicleTraitsForVehicleType @ 0x82705DF0,
    // GetNumPaintColours @ 0x82705F58) cross-checked against the DWARF (BrnTrafficData.h:54).
    // ALL fields below sit past +4, so this is purely additive over the two committed words.
    //
    // NOTE: the byte offsets in the comments are the X360 (32-bit, 4-byte-pointer) offsets the
    // asm reads. On the PC host pointers are 8 bytes so the absolute offsets differ -- the
    // getters access every field BY NAME, so the compiler resolves the host offsets. Do NOT
    // pin absolute offsets across the pointer members.
    void* mpPvs;                  // DWARF :62  X360 +0x08  BrnTraffic::Pvs*        (opaque here)
    void* mpapHulls;              // DWARF :63  X360 +0x0C  BrnTraffic::Hull**       (opaque here)
    void* mpapFlowTypes;          // DWARF :65  X360 +0x10  BrnTraffic::FlowType**   (opaque here)
    u16   muNumFlowTypes;         // DWARF :67  X360 +0x14
    u16   muNumVehicleTypes;      // DWARF :68  X360 +0x16  (asm `lhz 0x16` bound, GetVehicleTraits)
    u8    muNumVehicleAssets;     // DWARF :69  X360 +0x18
    u8    muNumVehicleTraits;     // DWARF :70  X360 +0x19  (asm `lbz 0x19` bound, GetVehicleTraits)
    u16   muNumKillZones;         // DWARF :72  X360 +0x1A
    u16   muNumKillZoneRegions;   // DWARF :73  X360 +0x1C  (asm `lhz 0x1C` bound, GetKillZoneRegions)
    u64*  mpaKillZoneIds;         // DWARF :76  X360 +0x20  TrafficData::KillZoneId(u64)*
    void* mpaKillZones;           // DWARF :77  X360 +0x24  KillZone*                (opaque here)
    KillZoneRegion*  mpaKillZoneRegions;   // DWARF :78  X360 +0x28  (asm `lwz 0x28` region table)
    VehicleTypeData* mpaVehicleTypes;      // DWARF :80  X360 +0x2C  (asm `lwz 0x2C` type table, stride 8)
    void* mpaVehicleTypesUpdate;  // DWARF :81  X360 +0x30  VehicleTypeUpdateData*   (opaque here)
    void* mpaVehicleAssets;       // DWARF :82  X360 +0x34  BrnTraffic::VehicleAsset*(opaque here)
    VehicleTraits*   mpaVehicleTraits;     // DWARF :83  X360 +0x38  (asm `lwz 0x38` traits table, stride 16)

    // DWARF :85  mTrafficLights (TrafficLightCollection). The X360 places it from +0x3C to
    // +0x168 (== 0x12C == 300 bytes) so that muNumPaintColours lands at +0x168. Its interior is
    // not part of this slice -> kept as an opaque sized buffer sized to the X360-attested span.
    // FLAG: mTrafficLightsOpaque is a DEFERRED opaque sub-aggregate (TrafficLightCollection);
    //       only its X360 footprint (300 bytes) is attested, not its members.
    u8    mTrafficLightsOpaque[300];       // DWARF :85  X360 +0x3C..+0x168

    u8    muNumPaintColours;      // DWARF :87  X360 +0x168 (asm `lbz 0x168`, GetNumPaintColours)
    void* mpaPaintColours;        // DWARF :89  X360 +0x16C Vector4*                 (opaque here)

    int FixDown();

    // Runtime getters bodied in BrnTrafficData.cpp (this slice). X360 entry points:
    //   GetKillZoneRegions             @ 0x82705D08  (DWARF :111)
    //   GetVehicleTraitsForVehicleType @ 0x82705DF0  (DWARF :116)
    //   GetNumPaintColours             @ 0x82705F58  (DWARF :126)
    const KillZoneRegion* GetKillZoneRegions(u32 luRegion) const;
    const VehicleTraits*  GetVehicleTraitsForVehicleType(u32 luVehicleType) const;
    s32                   GetNumPaintColours() const;
};

class TrafficDataResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
