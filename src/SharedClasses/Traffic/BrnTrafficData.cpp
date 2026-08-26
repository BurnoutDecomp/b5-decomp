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

    // =========================================================================================
    // TrafficData::GetStartDataForTrafficLight -- X360 0x8231CC48 (DWARF BrnTrafficData.h:95)
    // =========================================================================================
    // [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. This was the last link hole on the
    // seat-the-cars path: ModeManager::SetStartingGrid @0x82328608 -> ModeManager::
    // GetStartDataForTrafficLight @0x82327310 -> HERE -> Hull::GetJunctionForLightTrigger ->
    // Hull::GetLightTriggerStartDataForJunction. Its two callees landed in BrnTrafficHull.cpp
    // this pass, and BrnTraffic::JunctionLogicBox got a real header home
    // (SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h), which is what had blocked it.
    //
    // A LightTriggerId packs { hull index = bits 8..23, light-trigger index = bits 0..7 } and
    // BOTH halves carry an all-ones "none" sentinel. Every step is asserted; assert lines are
    // baked against "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficData.h" (path string
    // @0x820063??; the asserts are 265 / 268 / 271 / 274 / 278).
    //
    //   0x8231CC5C  extrwi r30, r28, 16,8    ; luHull        = (id >> 8) & 0xFFFF
    //   0x8231CC64  cmplwi r30, 0xFFFF       ; \ IsValid()
    //   0x8231CC6C  clrlwi r11, r28, 24      ;  |  luLightTrigger = id & 0xFF
    //   0x8231CC70  cmplwi r11, 0xFF         ; /
    //   0x8231CC9C  li r5, 0x109             ; assert 265 "lTriggerId.IsValid()"
    //   0x8231CCB0  lhz  r11, 2(r29)         ; muNumHulls (+0x02)
    //   0x8231CCC4  li r5, 0x10C             ; assert 268 "luHull < muNumHulls"
    //   0x8231CCD8  lwz  r11, 0xC(r29)       ; mpapHulls  (console +0x0C)
    //   0x8231CCE0  lwzx r29, r10, r11       ; lpHull = mpapHulls[luHull]
    //   0x8231CCF4  li r5, 0x10F             ; assert 271 "lpHull"
    //   0x8231CD08  lbz  r11, 0xE(r29)       ; lpHull->muNumLightTriggers (+0x0E)
    //   0x8231CD20  li r5, 0x112             ; assert 274 "luLightTrigger < lpHull->muNumLightTriggers"
    //   0x8231CD3C  bl   Hull::GetJunctionForLightTrigger
    //   0x8231CD54  li r5, 0x116             ; assert 278 "lpJunction"
    //   0x8231CD74  b/bl Hull::GetLightTriggerStartDataForJunction(lpHull, lpJunction, a3)
    //
    // [!] THE VALIDITY TEST IS AN ASSERT, NOT A GUARD. The console asserts and then carries on
    // into the hull lookup with the sentinel index; it never returns early. Callers that can
    // legitimately hold an invalid handle test it THEMSELVES first -- SetStartingGrid @0x82328678
    // does exactly that and skips the whole grid loop, which is the authored path for a mode
    // started away from lights. Do not "harden" this by returning NULL: that would silently
    // change which of the two behaviours a caller gets.
    //
    // [!] `lbUseAlternateStartData` selects the junction's ONLINE start-data index; see the
    // GetLightTriggerStartDataForJunction banner in BrnTrafficHull.cpp for the two different
    // -1 sentinel tests and the NULL return. ModeManager passes FALSE (`li r5,0` @0x82327360).
    // =========================================================================================
    const LightTriggerStartData* TrafficData::GetStartDataForTrafficLight(u32 luLightTriggerId,
                                                                         bool lbUseAlternateStartData) const
    {
        const u32 luHull         = (luLightTriggerId >> 8) & 0xFFFFu;
        const u32 luLightTrigger = luLightTriggerId & 0xFFu;

        // LightTriggerId::IsValid(), inlined by the console exactly as it is at the SetStartingGrid
        // site (BrnModeManager_IntroPlay.cpp's file-local IsLightTriggerIdValid is the same two
        // tests). This assert is what NAMES that predicate.
        const bool lbIsValid = (luHull != 0xFFFFu) && (luLightTrigger != 0xFFu);
        CGS_ASSERT(lbIsValid, "lTriggerId.IsValid()");                        // :265

        CGS_ASSERT(luHull < muNumHulls, "luHull < muNumHulls");               // :268

        const Hull* lpHull = mpapHulls[luHull];
        CGS_ASSERT(lpHull != nullptr, "lpHull");                              // :271

        CGS_ASSERT(luLightTrigger < lpHull->muNumLightTriggers,
                   "luLightTrigger < lpHull->muNumLightTriggers");            // :274

        const JunctionLogicBox* lpJunction = lpHull->GetJunctionForLightTrigger(luLightTrigger);
        CGS_ASSERT(lpJunction != nullptr, "lpJunction");                      // :278

        return lpHull->GetLightTriggerStartDataForJunction(lpJunction, lbUseAlternateStartData);
    }

    // =========================================================================================
    // TrafficData::GetJunctionLogicBoxForTrafficLight -- X360 0x82207F90 (DWARF BrnTrafficData.h:99)
    // =========================================================================================
    // [stuntrace waveD, agent D1] THE LightTriggerId -> JunctionLogicBox MAP. Nothing else in the
    // image performs it, and every consumer of an "event junction" goes through it:
    //   GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418 and
    //   GameStateModule::StartModeAtLights @0x82396CF8
    // both call it, read JunctionLogicBox::muEventJunctionID (+0x38) off the result, and use that
    // id to linear-search ProgressionData's EventJunction table for the offline RaceEventData.
    //
    // It is the STRUCTURAL TWIN of GetStartDataForTrafficLight above -- the same handle decode and
    // the same four bounds checks, in the same order, against the same members, with four DIFFERENT
    // baked assert lines (300 / 303 / 306 / 309 instead of 265 / 268 / 271 / 274). That is not a
    // shared helper the compiler duplicated: the two source statements are 35 lines apart in
    // BrnTrafficData.h, so both are written out here rather than folded into one.
    //
    //   0x82207FA4  extrwi r30, r28, 16,8    ; luHull         = (id >> 8) & 0xFFFF
    //   0x82207FB0  clrlwi r11, r28, 24      ; luLightTrigger = id & 0xFF
    //   0x82207FA8  cmplwi r30, 0xFFFF       ; \ IsValid(): NEITHER half may be all-ones
    //   0x82207FB4  cmplwi r11, 0xFF         ; /
    //   0x82207FE0  li r5, 0x12C             ; assert 300 "lTriggerId.IsValid()"
    //   0x82207FF4  lhz  r11, 2(r29)         ; muNumHulls (+0x02)
    //   0x82208008  li r5, 0x12F             ; assert 303 "luHull < muNumHulls"
    //   0x8220801C  lwz  r11, 0xC(r29)       ; mpapHulls (console +0x0C)
    //   0x82208024  lwzx r29, r10, r11       ; lpHull = mpapHulls[luHull]
    //   0x82208038  li r5, 0x132             ; assert 306 "lpHull"
    //   0x8220804C  lbz  r11, 0xE(r29)       ; lpHull->muNumLightTriggers (+0x0E)
    //   0x82208064  li r5, 0x135             ; assert 309 "luLightTrigger < lpHull->muNumLightTriggers"
    //   0x82208080  bl   Hull::GetJunctionForLightTrigger  (tail call, result returned unchecked)
    //
    // [!] THE 0x39 OWNER TAG DOES NOT NEED MASKING OFF FIRST, and code that "helpfully" strips it
    // is wrong twice over. `extrwi r30, r28, 16, 8` extracts bits 8..23 only, so the tag in bits
    // 24..31 is already dropped by the hull decode; and the SENTINEL test is against the extracted
    // 16-bit hull (0xFFFF), which a pre-masked id would still satisfy. Keep the packed handle
    // exactly as TrafficEntityModule::ManageTriggers @0x827477EC..0x827477FC built it --
    // `(hull << 8) | 0x39000000 | lightTriggerIndex`.
    //
    // [!] NO NULL CHECK ON THE RESULT HERE. GetStartDataForTrafficLight's sibling body asserts
    // "lpJunction" before using it; this one tail-calls and hands the pointer straight back. Its
    // callers do their own test. Reproduced as shipped.
    // =========================================================================================
    const JunctionLogicBox* TrafficData::GetJunctionLogicBoxForTrafficLight(u32 luLightTriggerId) const
    {
        const u32 luHull         = (luLightTriggerId >> 8) & 0xFFFFu;
        const u32 luLightTrigger = luLightTriggerId & 0xFFu;

        // LightTriggerId::IsValid() (DWARF BrnTrafficLightTrigger.h:70), inlined by the console as
        // the two all-ones tests above. Same predicate BrnModeManager_IntroPlay.cpp's file-local
        // IsLightTriggerIdValid spells out.
        const bool lbIsValid = (luHull != 0xFFFFu) && (luLightTrigger != 0xFFu);
        CGS_ASSERT(lbIsValid, "lTriggerId.IsValid()");                        // :300

        CGS_ASSERT(luHull < muNumHulls, "luHull < muNumHulls");               // :303

        const Hull* lpHull = mpapHulls[luHull];
        CGS_ASSERT(lpHull != nullptr, "lpHull");                              // :306

        CGS_ASSERT(luLightTrigger < lpHull->muNumLightTriggers,
                   "luLightTrigger < lpHull->muNumLightTriggers");            // :309

        return lpHull->GetJunctionForLightTrigger(luLightTrigger);
    }
}
