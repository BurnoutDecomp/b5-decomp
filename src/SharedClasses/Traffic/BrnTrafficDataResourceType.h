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
// [stuntrace waveB fix round] returned by GetStartDataForTrafficLight below; real layout is
// SharedClasses/Traffic/BrnTrafficLightTrigger.h:32.
struct LightTriggerStartData;
// [stuntrace waveD D1] returned by GetJunctionLogicBoxForTrafficLight below; real layout is
// SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h (0x120, DWARF BrnJunctionLogicBox.h:77).
// Pointer-only here, exactly like the four above -- the .cpp that dereferences it includes that
// header. Spelled `class` because that is what its owning header declares it as.
class JunctionLogicBox;

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

    // [stuntrace waveB fix round, 2026-08-26] X360 0x8231CC48 -- a REAL standalone export, not an
    // inline. Resolves a packed LightTriggerId ({ hull index = bits 8..23, light-trigger index =
    // bits 0..7 }) to that junction's start-grid block. Callers: ModeManager::
    // GetStartDataForTrafficLight @0x82327310 (passes lbUseAlternateStartData = FALSE, `li r5,0`
    // @0x82327360) and GameStateModule::SendSetUpAllEventStartsMessage.
    // Console body, walked instruction for instruction this pass (asserts verbatim, all baked
    // against "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficData.h"):
    //   :265 "lTriggerId.IsValid()"                        -- (id>>8) != 0xFFFF && (id&0xFF) != 0xFF
    //   :268 "luHull < muNumHulls"                         -- muNumHulls read at +0x02
    //   :271 "lpHull"                                      -- mpapHulls[luHull]
    //   :274 "luLightTrigger < lpHull->muNumLightTriggers" -- Hull+0x0E
    //   :278 "lpJunction"                                  -- Hull::GetJunctionForLightTrigger
    //   tail: return Hull::GetLightTriggerStartDataForJunction(lpHull, lpJunction, lbAlternate)
    // [x] BODIED 2026-08-26 (wave-B CLOSURE round) in BrnTrafficData.cpp -- this banner used to
    // read "DECLARE-ONLY: the body needs BrnTraffic::JunctionLogicBox (288-byte stride, fields at
    // +60/+64), which is UN-HOMED in this tree". That was accurate and is now stale: the type has
    // a real header home at SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h (promoted out of
    // GameSource/.../BrnTrafficHullRuntime.cpp, where it had been declared under its own
    // "RETIRE THIS BLOCK when BrnJunctionLogicBox.h lands" note). Both callees landed with it:
    // Hull::GetJunctionForLightTrigger @0x82752870 and
    // Hull::GetLightTriggerStartDataForJunction @0x82752900, in BrnTrafficHull.cpp.
    // [!] The validity test is an ASSERT, not a guard -- see the body's banner before adding any
    // early return; SetStartingGrid @0x82328678 is the caller that tests the handle itself.
    const LightTriggerStartData* GetStartDataForTrafficLight(u32 luLightTriggerId,
                                                             bool lbUseAlternateStartData) const;

    // [stuntrace waveD, agent D1] X360 0x82207F90 (DWARF BrnTrafficData.h:99) -- a REAL standalone
    // export. THE ONLY LightTriggerId -> JunctionLogicBox MAP IN THE IMAGE, and therefore the only
    // way anything reaches JunctionLogicBox::muEventJunctionID, the key into PROGRESSION.DAT's
    // EventJunction table. Its two console callers are the two halves of the offline event-start
    // chain: GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418 (the junction
    // prompt) and GameStateModule::StartModeAtLights @0x82396CF8 (the actual start).
    //
    // Same packed handle as GetStartDataForTrafficLight above and the SAME four bounds checks in
    // the same order, only with a different tail -- the console really does write the pair out
    // twice rather than share a helper. Walked instruction for instruction (assert strings and
    // lines verbatim, all baked against "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficData.h"):
    //   0x82207FA4  extrwi r30, r28, 16,8    ; luHull = (id >> 8) & 0xFFFF -- the 0x39 owner tag
    //                                        ;   sits in bits 24..31 and this mask drops it
    //   0x82207FB0  clrlwi r11, r28, 24      ; luLightTrigger = id & 0xFF
    //   0x82207FE0  li r5, 0x12C             ; :300 "lTriggerId.IsValid()"
    //   0x82207FF4  lhz  r11, 2(r29)         ; muNumHulls (+0x02)
    //   0x82208008  li r5, 0x12F             ; :303 "luHull < muNumHulls"
    //   0x8220801C  lwz  r11, 0xC(r29)       ; mpapHulls (console +0x0C)
    //   0x82208024  lwzx r29, r10, r11       ; lpHull = mpapHulls[luHull]
    //   0x82208038  li r5, 0x132             ; :306 "lpHull"
    //   0x8220804C  lbz  r11, 0xE(r29)       ; lpHull->muNumLightTriggers (+0x0E)
    //   0x82208064  li r5, 0x135             ; :309 "luLightTrigger < lpHull->muNumLightTriggers"
    //   0x82208080  bl   Hull::GetJunctionForLightTrigger   ; TAIL CALL -- no null check on the
    //                                        ;   result here (its caller does that itself)
    // [!] LIKE ITS SIBLING, THE VALIDITY TEST IS AN ASSERT, NOT A GUARD -- read the
    // GetStartDataForTrafficLight banner above before adding any early return.
    const JunctionLogicBox* GetJunctionLogicBoxForTrafficLight(u32 luLightTriggerId) const;
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
