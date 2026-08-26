// =============================================================================
// BrnTrafficHull.cpp -- owning .cpp for the BrnTraffic::Hull accessor family.
// The layout and the bounds-checked element accessors are in BrnTrafficHull.h;
// this file forces them out-of-line so the TU has a real object-code presence.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::Hull::GetNeighbour      @ 0x821F5358
//   BrnTraffic::Hull::GetStaticVehicle  @ 0x82705C90
//   BrnTraffic::Hull::GetStopLine       @ 0x82705C20
// GetSection @ 0x821F52E0 stays inline in the header and is not re-emitted here.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficSection.h"   // real Section/LaneRung (must precede Hull.h)
#include "SharedClasses/Traffic/BrnTrafficHull.h"
// [stuntrace waveB CLOSURE round] the two start-grid leaves below dereference both of these, so
// the forward declarations in BrnTrafficHull.h are not enough here.
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"  // JunctionLogicBox (+0x3C/+0x40)
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"         // LightTriggerStartData (0x1A0)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include <cstdint>                                     // uintptr_t

namespace BrnTraffic
{
    // -------------------------------------------------------------------------
    // Hull::FixUp @0x827620A0 / Hull::FixDown @0x827622E0.
    //
    // Twelve consecutive pointer slots (console +0x10 .. +0x3C) rebased against the
    // resource block base, unconditionally -- an empty per-hull array serialises as a
    // one-past-the-end offset, never as null, so there is nothing to guard.
    //
    // The console walks NO sub-elements: Section, LaneRung, Neighbour, SectionSpan,
    // SectionFlow, StopLine, StaticTrafficVehicle, LightTrigger, LightTriggerStartData
    // and JunctionLogicBox hold no pointers, and Section::muRungOffset /
    // muNeighbourOffset / muStopLineOffset are INDICES into the hull's own tables, so
    // they must not be relocated or widened. That also means nothing here byte-swaps a
    // record, which is correct: the shipped PC payload is already little-endian
    // (BrnTrafficStaticTraffic.cpp). Adding a StaticTrafficVehicle::FixUp call here
    // would corrupt correct data.
    //
    // Asserts, in the asm's order: muNumJunctions < 16 (baked BrnTrafficHull.cpp:143),
    // then the two 16-byte rung-table alignment guards (cpp:146 / :147). FixDown checks
    // the same two on the offset form (cpp:259 / :260), same slot order.
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

        inline bool Is16Aligned(const void* lpAddress)
        {
            return (reinterpret_cast<uintptr_t>(lpAddress) & 0xFu) == 0u;
        }
    }

    // KU_MAX_JUNCTIONS_PER_HULL comes from its canonical home,
    // BrnTrafficSharedConstants.h, reached through BrnTrafficHull.h -> BrnTrafficSection.h.

    void Hull::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        Rebase(mpaSections, luBase);
        Rebase(mpaRungs, luBase);
        Rebase(mpafCumulativeRungLengths, luBase);
        Rebase(mpaNeighbourData, luBase);
        Rebase(mpaSectionSpans, luBase);
        Rebase(mpaStaticTrafficVehicles, luBase);
        Rebase(mpaSectionFlows, luBase);
        Rebase(mpaJunctions, luBase);
        Rebase(mpaStopLines, luBase);
        Rebase(mpaLightTriggers, luBase);
        Rebase(mpaLightTriggerStartData, luBase);
        Rebase(mpaLightTriggerJunctionLookup, luBase);

        CGS_ASSERT(muNumJunctions < KU_MAX_JUNCTIONS_PER_HULL, "Hull has too many junctions");
        CGS_ASSERT(Is16Aligned(mpaRungs), "Rungs are not aligned");
        CGS_ASSERT(Is16Aligned(mpafCumulativeRungLengths), "Cumulative rung lengths are not aligned");
    }

    void Hull::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        CGS_ASSERT(Is16Aligned(mpaRungs), "Rungs are not aligned");
        CGS_ASSERT(Is16Aligned(mpafCumulativeRungLengths), "Cumulative rung lengths are not aligned");

        UnBase(mpaSections, luBase);
        UnBase(mpaRungs, luBase);
        UnBase(mpafCumulativeRungLengths, luBase);
        UnBase(mpaNeighbourData, luBase);
        UnBase(mpaSectionSpans, luBase);
        UnBase(mpaStaticTrafficVehicles, luBase);
        UnBase(mpaSectionFlows, luBase);
        UnBase(mpaJunctions, luBase);
        UnBase(mpaStopLines, luBase);
        UnBase(mpaLightTriggers, luBase);
        UnBase(mpaLightTriggerStartData, luBase);
        UnBase(mpaLightTriggerJunctionLookup, luBase);
    }

    // =========================================================================================
    // Hull::GetJunctionForLightTrigger -- X360 0x82752870 (DWARF BrnTrafficHull.h:121)
    // =========================================================================================
    // One indirection: a light trigger names a junction through the per-hull byte lookup table,
    // and that junction index selects a 0x120-byte JunctionLogicBox. Both bounds are asserted.
    //
    //   0x82752884  lbz  r11, 0xE(r30)        ; muNumLightTriggers               (+0x0E)
    //   0x82752888  cmplw r31, r11            ; luIndex < muNumLightTriggers
    //   0x827528A0  li   r5, 0x112            ; assert line 274, BrnTrafficHull.cpp
    //   0x827528B4  lwz  r11, 0x3C(r30)       ; mpaLightTriggerJunctionLookup    (+0x3C)
    //   0x827528B8  lbz  r10, 2(r30)          ; muNumJunctions                   (+0x02)
    //   0x827528BC  lbzx r31, r11, r31        ; lu8JunctionIndex = lookup[luIndex]
    //   0x827528D4  li   r5, 0x114            ; assert line 276
    //   0x827528E4  slwi r11, r31, 3          ; \
    //   0x827528E8  lwz  r10, 0x2C(r30)       ;  |  mpaJunctions                 (+0x2C)
    //   0x827528EC  add  r11, r31, r11        ;  |  idx*9  ...
    //   0x827528F0  slwi r11, r11, 5          ;  |  ... * 32 == idx * 0x120
    //   0x827528F4  add  r3, r11, r10         ; /   &mpaJunctions[lu8JunctionIndex]
    //
    // The literal stride 0x120 IS sizeof(JunctionLogicBox) (pinned by that type's _AssertLayout),
    // so indexing the array by name reproduces the asm exactly -- the same rule GetStaticVehicle
    // (stride 80) and GetStopLine (stride 2) already follow in this file.
    // =========================================================================================
    const JunctionLogicBox* Hull::GetJunctionForLightTrigger(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < muNumLightTriggers, "luIndex < muNumLightTriggers");

        const u8 lu8JunctionIndex = mpaLightTriggerJunctionLookup[luIndex];
        CGS_ASSERT(lu8JunctionIndex < muNumJunctions, "lu8JunctionIndex < muNumJunctions");

        return &mpaJunctions[lu8JunctionIndex];
    }

    // =========================================================================================
    // Hull::GetLightTriggerStartDataForJunction -- X360 0x82752900 (DWARF BrnTrafficHull.h:124)
    // =========================================================================================
    // Picks the junction's start-grid block. A junction carries TWO independent start-data
    // indices -- JunctionLogicBox::miOfflineStartDataIndex (+0x3C) and miOnlineStartDataIndex
    // (+0x40) -- and `lbUseAlternateStartData` selects the ONLINE one. (That is what "alternate"
    // means here, and it is why ModeManager::GetStartDataForTrafficLight @0x82327310 passes FALSE
    // with `li r5,0`: an offline stunt race wants the offline grid.)
    //
    // [!] THE ALTERNATE ARM FALLS BACK, AND THE TWO SENTINEL TESTS ARE DIFFERENT. Reproduced as
    // shipped -- do NOT tidy them into one predicate:
    //   * online index, tested `cmpwi -1 / beq` @0x82752954 -- EQUALITY ONLY. -1 means "no online
    //     grid on this junction" and falls through to the offline index.
    //   * offline index, tested `cmpwi -1 / ble` @0x827529A0 and @0x827529E8 -- <= -1, i.e. ANY
    //     negative value means "no grid", and the function returns NULL (`li r3,0` @0x82752A28).
    // The console emits the offline arm TWICE (once as the alternate arm's fallback at
    // loc_82752998, once as the default arm at loc_827529E0) with the SAME code but DIFFERENT
    // baked assert lines -- 309 and 319 -- which is how we know they are two source statements
    // and not one shared tail. Written as one helper lambda here would erase that; written as the
    // straight-line form below it stays visible.
    //
    // Tail on every success arm: `lwz r10, 0x38(this)` (mpaLightTriggerStartData) plus
    // `mulli r11, idx, 0x1A0` -- and 0x1A0 == sizeof(LightTriggerStartData), so the array is
    // indexed by name.
    //
    // [!] DATA NOTE, so nobody reads an empty block as a load failure: the wave's traffic-data
    // survey (the B5TRAFFIC.BNDL transcode, tools/assets/bundles/lane_transcode.py, whose
    // LightTriggerStartData record is declared at :249-252 and console-pinned at 0x1A0 at :302)
    // reports muNumDestinations == 0 in all 443 shipped records -- only the start POSITIONS are
    // authored, the destination table is empty everywhere in the retail data. CARRIED FROM THAT
    // SURVEY, not re-measured in this pass; re-run the transcode before relying on the count. The
    // point that matters either way: a returned block with no destinations is normal.
    // =========================================================================================
    const LightTriggerStartData* Hull::GetLightTriggerStartDataForJunction(const JunctionLogicBox* lpJunction,
                                                                          bool lbUseAlternateStartData) const
    {
        CGS_ASSERT(lpJunction != nullptr, "lpJunction");                             // :293

        if (lbUseAlternateStartData)
        {
            const s32 liOnlineStartIndex = lpJunction->GetOnlineStartDataIndex();
            if (liOnlineStartIndex != -1)
            {
                CGS_ASSERT(liOnlineStartIndex < muNumLightTriggersStartData,
                           "liStartIndex < muNumLightTriggersStartData");            // :301
                return &mpaLightTriggerStartData[liOnlineStartIndex];
            }

            // Fall back to the offline grid (loc_82752998).
            const s32 liStartIndex = lpJunction->GetOfflineStartDataIndex();
            if (liStartIndex <= -1)
            {
                return nullptr;
            }
            CGS_ASSERT(liStartIndex < muNumLightTriggersStartData,
                       "liStartIndex < muNumLightTriggersStartData");                // :309
            return &mpaLightTriggerStartData[liStartIndex];
        }

        // loc_827529E0 -- the default (offline) arm.
        const s32 liStartIndex = lpJunction->GetOfflineStartDataIndex();
        if (liStartIndex <= -1)
        {
            return nullptr;
        }
        CGS_ASSERT(liStartIndex < muNumLightTriggersStartData,
                   "liStartIndex < muNumLightTriggersStartData");                    // :319
        return &mpaLightTriggerStartData[liStartIndex];
    }

    // Out-of-line forwarders pin the three inline accessors into this TU.
    const void* Hull_GetNeighbour(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetNeighbour(luIndex);
    }

    const StaticTrafficVehicle* Hull_GetStaticVehicle(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetStaticVehicle(luIndex);
    }

    const StopLine* Hull_GetStopLine(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetStopLine(luIndex);
    }
}
