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
#include "SharedClasses/Traffic/BrnTrafficSection.h" // Section/LaneRung (must precede Hull.h)
#include "SharedClasses/Traffic/BrnTrafficHull.h"    // BrnTraffic::Hull (GetHull return layout, FixUp)
#include "SharedClasses/Traffic/BrnTrafficPvs.h"     // BrnTraffic::Pvs::FixUp / FixDown
#include "SharedClasses/Traffic/BrnTrafficFlowType.h"// BrnTraffic::FlowType::FixUp / FixDown
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"
#include <cstdint>                                   // uintptr_t (relocation arithmetic)

namespace BrnTraffic
{
    // -------------------------------------------------------------------------
    // LOAD-TIME POINTER RELOCATION
    //
    // The shipped lane graph stores every pointer slot as a BYTE OFFSET from the start
    // of the TrafficData header. CgsResource::Pool::FixUpEntry @0x828EB860 calls the
    // registered type's vtable slot 4 -- TrafficDataResourceType::FixUp @0x82763E70, an
    // 8-byte thunk `mr r3,r4; b 0x827637D8` -- so TrafficData::FixUp receives the
    // resource block as BOTH `this` and the relocation base. FixDown @0x82763CB8 is the
    // exact inverse and runs INNER-FIRST (it must read a pointer before un-rebasing it).
    //
    // Pointer relocation is intrinsically address arithmetic, so each slot is rebased
    // through uintptr_t -- the established BrnTrigger::TriggerData::FixDown /
    // BrnProgression::ProgressionData::FixDown idiom. On the x64 port the slots are 64
    // bits wide (tools/assets/bundles/lane_transcode.py writes them that way), so the
    // console's 32-bit delta is widened to the full base pointer; nothing truncates.
    //
    // ⚠️ NO NULL GUARDS. Unlike the AI path, every Traffic slot is rebased
    // unconditionally on the console -- an empty array serialises as a one-past-the-end
    // offset, never as 0. The transcoder preserves that, so a null check here would be
    // both unfaithful and wrong.
    // -------------------------------------------------------------------------
    namespace
    {
        template <typename T>
        inline void Rebase(T*& lrpPointer, uintptr_t luBase)
        {
            lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) + luBase);
        }

        template <typename T>
        inline void UnBase(T*& lrpPointer, uintptr_t luBase)
        {
            lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) - luBase);
        }

        // The X360 asserts several relocated blocks land 16-byte aligned.
        inline bool Is16Aligned(const void* lpAddress)
        {
            return (reinterpret_cast<uintptr_t>(lpAddress) & 0xFu) == 0u;
        }
    }

    // TrafficLightCollection::FixUp / FixDown (DWARF BrnTrafficLightCollection.h:126 / :131).
    // The console has no standalone symbol -- TrafficData::FixUp/FixDown inline these eight
    // slot rebases (X360 offsets +0x44/+0x48/+0x4C/+0x50/+0x54/+0x58/+0x160/+0x164, i.e. this
    // struct's +0x08..+0x1C and +0x124/+0x128). De-inlined here to the named members.
    void TrafficLightCollection::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);
        Rebase(mpaPosAndYRotations, luBase);
        Rebase(mpaInstanceIDs, luBase);
        Rebase(mpauInstanceTypes, luBase);
        Rebase(mpaTrafficLightTypes, luBase);
        Rebase(mpaCoronaTypes, luBase);
        Rebase(mpaCoronaPositions, luBase);
        Rebase(mpauInstanceHashTable, luBase);
        Rebase(mpauInstanceHashToIndexLookup, luBase);
    }

    void TrafficLightCollection::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);
        UnBase(mpaPosAndYRotations, luBase);
        UnBase(mpaInstanceIDs, luBase);
        UnBase(mpauInstanceTypes, luBase);
        UnBase(mpaTrafficLightTypes, luBase);
        UnBase(mpaCoronaTypes, luBase);
        UnBase(mpaCoronaPositions, luBase);
        UnBase(mpauInstanceHashTable, luBase);
        UnBase(mpauInstanceHashToIndexLookup, luBase);
    }

    // FlowType::FixUp / FixDown (DWARF BrnTrafficFlowType.h:56 / :61). Likewise inlined on
    // the console into TrafficData::FixUp's flow-type loop (`p[0] += base; p[4] += base`).
    void FlowType::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);
        Rebase(mpauVehicleTypeIds, luBase);
        Rebase(mpauCumulativeProb, luBase);
    }

    void FlowType::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);
        UnBase(mpauVehicleTypeIds, luBase);
        UnBase(mpauCumulativeProb, luBase);
    }

    // X360 @0x827637D8. Offsets -> pointers. Order is the asm's: the version assert, the 11
    // header slots, Pvs, the per-hull loop, the per-flow-type loop, the embedded light
    // collection, then the four sanity asserts.
    void TrafficData::FixUp(const void* lpBaseData)
    {
        // BrnTrafficData.cpp:65 -- the only version guard on this path.
        CGS_ASSERT(muDataVersion == KU_DATA_VERSION, "Traffic data version mismatch");

        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        Rebase(mpPvs, luBase);
        Rebase(mpapHulls, luBase);
        Rebase(mpapFlowTypes, luBase);
        Rebase(mpaKillZoneIds, luBase);
        Rebase(mpaKillZones, luBase);
        Rebase(mpaKillZoneRegions, luBase);
        Rebase(mpaVehicleTypes, luBase);
        Rebase(mpaVehicleTypesUpdate, luBase);
        Rebase(mpaVehicleAssets, luBase);
        Rebase(mpaVehicleTraits, luBase);
        Rebase(mpaPaintColours, luBase);

        mpPvs->FixUp(lpBaseData);

        for (u32 luHull = 0; luHull < muNumHulls; ++luHull)
        {
            Rebase(mpapHulls[luHull], luBase);
            mpapHulls[luHull]->FixUp(lpBaseData);
        }

        for (u32 luFlowType = 0; luFlowType < muNumFlowTypes; ++luFlowType)
        {
            Rebase(mpapFlowTypes[luFlowType], luBase);
            mpapFlowTypes[luFlowType]->FixUp(lpBaseData);
        }

        mTrafficLights.FixUp(lpBaseData);

        // BrnTrafficData.cpp:147 / :148 -- the two 16-byte alignment guards.
        CGS_ASSERT(Is16Aligned(mpaVehicleTypesUpdate), "Vehicle type update data is not aligned");
        CGS_ASSERT(Is16Aligned(mpaPaintColours), "Paint colours are not aligned");

        // BrnTrafficData.cpp:151 / :152 -- ⚠️ FAITHFUL SHIPPED BUG: both range guards read the
        // halfword at +0x16 (muNumVehicleTypes) and compare it against 96, even though the
        // first one's message talks about hulls and a max of 400. Reproduced literally rather
        // than "corrected", per the reconstruction rules; neither fires on the shipped data
        // (muNumVehicleTypes == 30).
        CGS_ASSERT(muNumVehicleTypes <= 96u, "Too many hulls in traffic data (max 400)");
        CGS_ASSERT(muNumVehicleTypes <= 96u, "Too many vehicle types in traffic data (max 96)");
    }

    // X360 @0x82763CB8. Pointers -> offsets; the exact inverse of FixUp, inner-first so every
    // sub-object is read through a still-valid pointer before that pointer is un-rebased. No
    // version assert on this path (matching the asm).
    void TrafficData::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        mpPvs->FixDown(lpBaseData);

        for (u32 luHull = 0; luHull < muNumHulls; ++luHull)
        {
            mpapHulls[luHull]->FixDown(lpBaseData);
            UnBase(mpapHulls[luHull], luBase);
        }

        for (u32 luFlowType = 0; luFlowType < muNumFlowTypes; ++luFlowType)
        {
            mpapFlowTypes[luFlowType]->FixDown(lpBaseData);
            UnBase(mpapFlowTypes[luFlowType], luBase);
        }

        mTrafficLights.FixDown(lpBaseData);

        UnBase(mpPvs, luBase);
        UnBase(mpapHulls, luBase);
        UnBase(mpapFlowTypes, luBase);
        UnBase(mpaKillZoneIds, luBase);
        UnBase(mpaKillZones, luBase);
        UnBase(mpaKillZoneRegions, luBase);
        UnBase(mpaVehicleTypes, luBase);
        UnBase(mpaVehicleTypesUpdate, luBase);
        UnBase(mpaVehicleAssets, luBase);
        UnBase(mpaVehicleTraits, luBase);
        UnBase(mpaPaintColours, luBase);
    }

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
    // mpapHulls[luHull] by name -- the member is now the real Hull** so no cast is needed.
    const Hull* TrafficData::GetHull(u32 luHull) const
    {
        return mpapHulls[luHull];
    }
}
