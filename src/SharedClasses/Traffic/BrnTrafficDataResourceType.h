#ifndef BRN_TRAFFIC_DATA_RESOURCE_TYPE_H
#define BRN_TRAFFIC_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "BrnCommonTypes.h"                                // Vector4
#include "SharedClasses/Traffic/BrnTrafficKillZone.h"      // KillZone, KillZoneRegion
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"   // VehicleTypeData, VehicleTypeUpdateData
#include "SharedClasses/Traffic/BrnTrafficVehicleTraits.h" // VehicleTraits
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"  // VehicleAsset
#include "SharedClasses/Traffic/BrnTrafficLightCollection.h" // TrafficLightCollection (by value)
#include <cstddef>                                         // offsetof

namespace BrnTraffic
{
// Forward decls: TrafficData hands these out only by pointer, and their real layouts live
// in their own owning headers (BrnTrafficHull.h / BrnTrafficPvs.h / BrnTrafficFlowType.h),
// which the .cpp files that dereference the results #include.
struct Hull;
class  Pvs;
struct FlowType;

// =============================================================================
// BrnTraffic::TrafficData -- the serialised lane graph ("BaseTraffic", resource type
// 65538, shipped in B5TRAFFIC.BNDL). DWARF home BrnTrafficData.h:54.
//
// SERIALISED FORM / RELOCATION (X360-attested, 2026-07-29 lane-data wave):
//   Every pointer member below is stored on disc as a BYTE OFFSET from the start of
//   this header, and CgsResource::Pool::FixUpEntry @0x828EB860 turns them into real
//   pointers by calling vtable slot 4 -- TrafficDataResourceType::FixUp @0x82763E70, an
//   8-byte thunk (`mr r3,r4; b 0x827637D8`) that hands TrafficData::FixUp the resource
//   block as BOTH this and the relocation base.
//   ⚠️ That thunk is ABSENT from .ida-exports/ because it has no .pdata unwind record,
//   so IDA never promoted it to a function -- which is the whole reason this type looked
//   like it "had no FixUp" and why TrafficData::FixUp @0x827637D8 looks xref-less (it is
//   reached only through the vtable). It is a real, registered virtual: the vtable at
//   0x820A1520 holds {GetTypeID 0x82752560, GetSerialisedResourceDescriptor 0x82760660,
//   DeSerialise, FixDown 0x82763E68, FixUp 0x82763E70, ...}.
//
// x64 PORT: the shipped data is transcoded to platform 4 with WIDENED 64-bit pointer
// slots by tools/assets/bundles/lane_transcode.py, so every member below is a REAL host
// pointer -- no Ptr32<T>, no PointerFromU32. The static_asserts at the bottom pin the
// host offsets that transcoder writes; they and the porter's own CONSOLE_PIN table are
// the two ends of the same contract.
// =============================================================================
struct TrafficData
{
    // KU_DATA_VERSION -- TrafficData::FixUp asserts muDataVersion == 44
    // (BrnTrafficData.cpp:65). The shipped B5TRAFFIC.BNDL payload reads 44.
    static const u8 KU_DATA_VERSION = 44;

    u8  muDataVersion;            // +0x00 (:56)
    u16 muNumHulls;               // +0x02 (:59)
    u32 muSizeInBytes;            // +0x04 (:60)  total serialised byte size

    Pvs*       mpPvs;             // (:62)  X360 +0x08  the hull lookup grid
    Hull**     mpapHulls;         // (:63)  X360 +0x0C  muNumHulls entries
    FlowType** mpapFlowTypes;     // (:65)  X360 +0x10  muNumFlowTypes entries

    u16 muNumFlowTypes;           // (:67)  X360 +0x14
    u16 muNumVehicleTypes;        // (:68)  X360 +0x16
    u8  muNumVehicleAssets;       // (:69)  X360 +0x18
    u8  muNumVehicleTraits;       // (:70)  X360 +0x19
    u16 muNumKillZones;           // (:72)  X360 +0x1A
    u16 muNumKillZoneRegions;     // (:73)  X360 +0x1C

    typedef u64 KillZoneId;                             // BrnTrafficKillZone.h:37
    KillZoneId*            mpaKillZoneIds;              // (:76)  X360 +0x20
    KillZone*              mpaKillZones;                // (:77)  X360 +0x24
    KillZoneRegion*        mpaKillZoneRegions;          // (:78)  X360 +0x28
    VehicleTypeData*       mpaVehicleTypes;             // (:80)  X360 +0x2C
    VehicleTypeUpdateData* mpaVehicleTypesUpdate;       // (:81)  X360 +0x30 (16-aligned)
    VehicleAsset*          mpaVehicleAssets;            // (:82)  X360 +0x34
    VehicleTraits*         mpaVehicleTraits;            // (:83)  X360 +0x38

    TrafficLightCollection mTrafficLights;              // (:85)  X360 +0x3C..+0x168

    u8       muNumPaintColours;   // (:87)  X360 +0x168
    Vector4* mpaPaintColours;     // (:89)  X360 +0x16C (16-aligned)

    // Load-time pointer relocation (X360 @0x827637D8 / @0x82763CB8). The base is the
    // resource block itself; the DWARF signature is `void FixUp(const void*)` (:95/:100).
    void FixUp(const void* lpBaseData);
    void FixDown(const void* lpBaseData);

    // Runtime getters bodied in BrnTrafficData.cpp. X360 entry points:
    //   GetKillZoneRegions             @ 0x82705D08  (DWARF :111)
    //   GetVehicleTraitsForVehicleType @ 0x82705DF0  (DWARF :116)
    //   GetNumPaintColours             @ 0x82705F58  (DWARF :126)
    const KillZoneRegion* GetKillZoneRegions(u32 luRegion) const;
    const VehicleTraits*  GetVehicleTraitsForVehicleType(u32 luVehicleType) const;
    s32                   GetNumPaintColours() const;

    // Thin hull-array accessor over mpapHulls. The X360 reaches it inline as
    // `mpapHulls[luHull]` (e.g. OnlineStuntRunMode::GetBestStartGridID @0x82331708);
    // de-inlined here and bodied in BrnTrafficData.cpp.
    const Hull* GetHull(u32 luHull) const;
};

// ---- host layout contract with tools/assets/bundles/lane_transcode.py ---------------
static_assert(offsetof(TrafficData, muSizeInBytes)         == 0x004, "TrafficData::muSizeInBytes");
static_assert(offsetof(TrafficData, mpPvs)                 == 0x008, "TrafficData::mpPvs");
static_assert(offsetof(TrafficData, mpapHulls)             == 0x010, "TrafficData::mpapHulls");
static_assert(offsetof(TrafficData, mpapFlowTypes)         == 0x018, "TrafficData::mpapFlowTypes");
static_assert(offsetof(TrafficData, muNumFlowTypes)        == 0x020, "TrafficData::muNumFlowTypes");
static_assert(offsetof(TrafficData, muNumKillZoneRegions)  == 0x028, "TrafficData::muNumKillZoneRegions");
static_assert(offsetof(TrafficData, mpaKillZoneIds)        == 0x030, "TrafficData::mpaKillZoneIds");
static_assert(offsetof(TrafficData, mpaVehicleTraits)      == 0x060, "TrafficData::mpaVehicleTraits");
static_assert(offsetof(TrafficData, mTrafficLights)        == 0x068, "TrafficData::mTrafficLights");
static_assert(offsetof(TrafficData, muNumPaintColours)     == 0x1B8, "TrafficData::muNumPaintColours");
static_assert(offsetof(TrafficData, mpaPaintColours)       == 0x1C0, "TrafficData::mpaPaintColours");
static_assert(sizeof(TrafficData) == 0x1C8, "TrafficData host sizeof");

class TrafficDataResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
