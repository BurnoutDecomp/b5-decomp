#pragma once

// =============================================================================
// BrnJunctionLogicBox.h -- OWNING HEADER for the BrnTraffic junction-signal value types.
//
// [stuntrace waveB CLOSURE round, 2026-08-26] PROMOTED OUT OF A .cpp. Until this file existed,
// BrnTraffic::TrafficLightController and BrnTraffic::JunctionLogicBox were declared inside
// GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.cpp, under a banner
// that read "RETIRE THIS BLOCK when BrnJunctionLogicBox.h lands: delete both types and include
// that header instead". This is that header; the .cpp now includes it and its local copies are
// gone. Everything below is the SAME reconstruction, moved verbatim -- no layout was re-derived
// in the move, and the static_asserts that pinned it moved with it.
//
// The promotion is what unblocked BrnTraffic::Hull::GetLightTriggerStartDataForJunction
// @0x82752900 and, above it, BrnTraffic::TrafficData::GetStartDataForTrafficLight @0x8231CC48 --
// the leaf ModeManager::SetStartingGrid needs to seat a single car on a stunt-race start grid.
// Both read this record (miOfflineStartDataIndex / miOnlineStartDataIndex), so neither could be
// written while the type had no header home.
//
// LAYOUT PROVENANCE (unchanged; the two independent sources agree):
//   * ARTIST, HullRuntime::Prepare's junction loop @0x82751438 --
//         0x82751528  lwz  r8, 0x2C(r31)   ; lpHull->mpaJunctions
//         0x82751534  addi r9, r9, 0x120   ; ELEMENT STRIDE = 288
//         0x82751538  lbz  r8, 0x34(r8)    ; muNumStates at +0x34
//     and Hull::GetLightTriggerStartDataForJunction @0x82752950/@0x82752998 --
//         lwz r30, 0x40(lpJunction)        ; miOnlineStartDataIndex  (alternate arm)
//         lwz r31, 0x3C(lpJunction)        ; miOfflineStartDataIndex (default arm)
//   * The DWARF member list (BrnJunctionLogicBox.h :128..:141) sums to exactly 0x120 with
//     muNumStates at 0x34, miOfflineStartDataIndex at 0x3C and miOnlineStartDataIndex at 0x40.
//
// Absolute offsets are legitimate here: the record holds NO POINTERS (integer scalars and
// arrays, eight 24-byte pointer-free TrafficLightControllers, one Vector3 lane), so console
// offsets are host offsets. The two holes are natural alignment holes and _AssertLayout proves
// it. A member-less stand-in would be actively unsafe -- it completes
// the forward declaration at sizeof == 1, so mpaJunctions[] would advance one byte per junction
// and every hull with more than one junction would read a plausible wrong value, silently.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // GetLight's bound assert (ADDITIVE, FX-NETCRASH)

#include <cstddef>            // offsetof (the layout pins below)

namespace BrnTraffic
{

// BrnJunctionLogicBox.h:51 -- the signal group driving one approach of a junction.
// 24-byte pointer-free record; eight of them sit inline in JunctionLogicBox.
struct TrafficLightController
{
    u16 mauTrafficLightIds[2];   // +0x00  (:53)
    u8  mauStopLineIds[6];       // +0x04  (:54)
    u16 mauStopLineHulls[6];     // +0x0A  (:55)
    u8  muNumStopLines;          // +0x16  (:56)
    u8  muNumTrafficLights;      // +0x17  (:57)
                                 //  sizeof == 0x18

    // :61 -- streamed-data byte swap; its own (not-yet-reconstructed) TU. Declaration
    // only, matching how the traffic directory already handles LaneRung::EndianSwap.
    void EndianSwap();
};

// BrnJunctionLogicBox.h:77 -- one junction's signal-phase program plus its start-grid links.
class JunctionLogicBox
{
public:
    // ---- bodied: each of these three is pinned by an ARTIST read of the exact byte ----------
    // :84. HullRuntime::Prepare @0x82751538 `lbz r8, 0x34(r8)`.
    u8  GetNumStates() const              { return muNumStates; }

    // :91 / :92. Hull::GetLightTriggerStartDataForJunction @0x82752900 reads BOTH, and the
    // choice between them IS that function's bool parameter: the alternate/online arm loads
    // +0x40 (`lwz r30, 0x40(r31)` @0x82752950) and the default/offline arm loads +0x3C
    // (`lwz r31, 0x3C(r31)` @0x82752998 and @0x827529E0). -1 means "this junction has no start
    // grid of that flavour" -- see that body for the two DIFFERENT sentinel tests.
    s32 GetOfflineStartDataIndex() const  { return miOfflineStartDataIndex; }
    s32 GetOnlineStartDataIndex() const   { return miOnlineStartDataIndex; }

    // ---- declared-only: DWARF-attested shapes with no consumer in this tree yet -------------
    // Kept as declarations (the LightTriggerStartData precedent in BrnTrafficLightTrigger.h) so
    // the class's published surface matches the console's without fabricating bodies. muID and
    // muEventJunctionID are independently corroborated by BrnGameActions.h:1263/:1266, which
    // record JunctionLogicBox+0x00 and +0x38 as the two ids the GUI's junction-info record
    // carries; mPosition is the +0x110 lane the lane transcoder writes.
    // ⭐ [stuntrace wave D, D3] BODIED (was declare-only). Both reads are ARTIST-attested at the
    // same call site -- GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418 reaches
    // the box through TrafficData::GetJunctionLogicBoxForTrafficLight and then does
    //     0x82390970  lwz r8,  0(r31)     ; muID          -> the action-201 record's +0x00
    //     0x823909A4  lwz r30, 0x38(r31)  ; muEventJunctionID -> its +0x04, and the key into
    //                                     ;   ProgressionData's EventJunction table
    // and StartModeAtLights @0x82396CF8 repeats both (`lwz r23, 0x38(r27)` @0x82396E9C,
    // `lwz r11, 0(r27)` @0x8239702C). The X360 emits no standalone symbol for either -- both
    // render as those bare loads -- so they are header-inlines, like GetNumStates above.
    u32     GetID() const                 { return muID; }                 // :83
    f32     GetTimeInState(u32 luState) const;         // :85
    u8      GetNumLights() const;                      // :86
    const TrafficLightController* GetLight(u32 luLight) const;      // :87
    bool    IsLightRed(u32 luState, u32 luLight) const;             // :88
    // ADDITIVE (crash parity FX-TRAFFICLIGHTS, 2026-09-25) [FLAG PC: no console symbol] -- the authored u16
    // behind GetTimeInState, with the same h:165 tripwire. TrafficEntityModule::UpdateJunctions needs the
    // tenths themselves: see the bodies below the class.
    u16     GetStateTiming(u32 luState) const;
    u32     GetEventJunctionID() const    { return muEventJunctionID; }    // :90 (see GetID above)

    // ⭐ [event-starts producer wave 2026-08-27] BODIED (was declare-only), on the same terms as
    // GetID/GetEventJunctionID above: the X360 emits no standalone symbol, every reader renders as
    // a bare load of the +0x110 lane. Attested at GameStateModule::SendSetUpAllEventStartsMessage
    // @0x82375C88 -- `li r16, 0x110` hoisted out of the loop, then `lvx128 v127, r30, r16` with r30
    // == the JunctionLogicBox TrafficData::GetJunctionLogicBoxForTrafficLight just returned, and v1
    // (== v127) is handed straight to SetUpAllEventStartsInterface::AddEventStart as the record's
    // position lane. Which is what puts the event icon on the sat-nav map at the right place.
    Vector3 GetPosition() const           { return mPosition; }            // :94
    void    FixUp(const void* lpBaseData);             // :119
    void    FixDown(const void* lpBaseData);           // :124

    // ---- layout pin. NEVER CALLED, and defined in-class so it needs no out-of-line home ------
    // Sources: HullRuntime::Prepare's junction loop (stride 0x120, muNumStates +0x34),
    // Hull::GetLightTriggerStartDataForJunction (+0x3C / +0x40) and the DWARF member list
    // :128..:141. It has to be a MEMBER: the members it measures are private, so a namespace-scope
    // offsetof would not compile (C2248).
    static void _AssertLayout()
    {
        static_assert(sizeof(TrafficLightController) == 0x18, "sizeof(TrafficLightController)");
        static_assert(offsetof(TrafficLightController, mauStopLineIds)   == 0x04, "mauStopLineIds");
        static_assert(offsetof(TrafficLightController, mauStopLineHulls) == 0x0A, "mauStopLineHulls");
        static_assert(offsetof(TrafficLightController, muNumStopLines)   == 0x16, "muNumStopLines");

        static_assert(offsetof(JunctionLogicBox, muID)                  == 0x000, "muID");
        static_assert(offsetof(JunctionLogicBox, mauStateTimings)       == 0x004, "mauStateTimings");
        static_assert(offsetof(JunctionLogicBox, mauStoppedLightStates) == 0x024,
                      "mauStoppedLightStates");
        // The one offset HullRuntime::Prepare states outright: `lbz r8, 0x34(r8)` @0x82751538.
        static_assert(offsetof(JunctionLogicBox, muNumStates)           == 0x034, "muNumStates");
        static_assert(offsetof(JunctionLogicBox, muNumLights)           == 0x035, "muNumLights");
        static_assert(offsetof(JunctionLogicBox, muEventJunctionID)     == 0x038, "muEventJunctionID");
        // The two offsets Hull::GetLightTriggerStartDataForJunction states outright.
        static_assert(offsetof(JunctionLogicBox, miOfflineStartDataIndex) == 0x03C,
                      "miOfflineStartDataIndex");
        static_assert(offsetof(JunctionLogicBox, miOnlineStartDataIndex)  == 0x040,
                      "miOnlineStartDataIndex");
        static_assert(offsetof(JunctionLogicBox, maTrafficLightControllers) == 0x044,
                      "maTrafficLightControllers");
        static_assert(offsetof(JunctionLogicBox, mPosition)             == 0x110, "mPosition");
        // The one size HullRuntime::Prepare states outright: `addi r9, r9, 0x120` @0x82751534.
        static_assert(sizeof(JunctionLogicBox) == 0x120, "sizeof(JunctionLogicBox)");
    }

private:
    u32 muID;                                             // +0x000  (:128)
    u16 mauStateTimings[16];                              // +0x004  (:129)
    u8  mauStoppedLightStates[16];                        // +0x024  (:130)
    u8  muNumStates;                                      // +0x034  (:131)
    u8  muNumLights;                                      // +0x035  (:132)
    u8  maPad0x36[2];                                     // +0x036  natural hole
    u32 muEventJunctionID;                                // +0x038  (:135)
    s32 miOfflineStartDataIndex;                          // +0x03C  (:136)
    s32 miOnlineStartDataIndex;                           // +0x040  (:137)
    TrafficLightController maTrafficLightControllers[8];  // +0x044  (:139)  8 * 0x18
    u8  maPad0x104[12];                                   // +0x104  natural hole: the
                                                          //         Vector3 lane is
                                                          //         16-byte aligned
    Vector3 mPosition;                                    // +0x110  (:141)
};                                                        //  sizeof == 0x120

// ADDITIVE (crash parity FX-NETCRASH, 2026-09-25) -- the two declared-only accessors (:86 / :87, DWARF :68 /
// :71), bodied as the console inlines them in TrafficEntityModule::UpdateEventStarts @0x82743B80: the light
// count is `lbz r11, 0x35(junction)` (0x82743E00 / 0x82743FA4), and each GetLight is the :181 bound tripwire
// "luLight < muNumLights" (li r5, 0xB5 @0x82743E24; the message at 0x820036DC, this file's path at
// 0x820036F8) then junction + 0x44 + 0x18 * luLight (0x82743E10 / 0x82743E38 -- maTrafficLightControllers).
inline u8 JunctionLogicBox::GetNumLights() const
{
    return muNumLights;
}

inline const TrafficLightController* JunctionLogicBox::GetLight(u32 luLight) const
{
    CGS_ASSERT(luLight < muNumLights, "luLight < muNumLights");   // BrnJunctionLogicBox.h:181
    return &maTrafficLightControllers[luLight];
}

// ADDITIVE (crash parity FX-TRAFFICLIGHTS, 2026-09-25) -- the two remaining declared-only accessors the junction
// update inlines (TrafficEntityModule::UpdateJunctions @0x82723EA0; DWARF :85 / :88, bodies at this header's
// lines 165 and 193..194 on both X360 and PS3), and the PC accessor behind the first:
//   GetTimeInState  the h:165 bound tripwire "luState < muNumStates" (li r5, 0xA5 @0x82724208), then the phase's
//                   authored length (lhzx at +0x4 + 2 * luState, 0x8272422C) in tenths of a second, times
//                   flt_82004014 == 0.1f (0x3DCCCCCD, x360rd).
//   GetStateTiming  [FLAG PC] those tenths. The console compiler FUSED GetTimeInState's product into the
//                   caller's accumulate -- `fmadds f0, f13, f29, f0` @0x82724244, one rounding of
//                   tenths * 0.1f + time (ROUNDING_RULE 3) -- which a call returning the rounded product cannot
//                   reproduce; UpdateJunctions reads the tenths and calls std::fma.
//   IsLightRed      the h:193 / h:194 tripwires (li r5, 0xC1 / 0xC2 @0x82724294 / 0x827242B8), then whether the
//                   light's bit is set in the phase's stopped-light mask: lbzx +0x24 + luState, the bit
//                   (u8)(1 << luLight) (li 1 ; slw ; clrlwi 24), and/subf/cntlzw/extrwi == ((mask & bit) == bit)
//                   (0x827242D4..0x827242F4).
static const f32 KF_JUNCTION_STATE_TIMING_UNIT = 0.1f;   // flt_82004014: a phase is authored in tenths of a second

inline u16 JunctionLogicBox::GetStateTiming(u32 luState) const
{
    CGS_ASSERT(luState < muNumStates, "luState < muNumStates");   // BrnJunctionLogicBox.h:165
    return mauStateTimings[luState];
}

inline f32 JunctionLogicBox::GetTimeInState(u32 luState) const
{
    return static_cast<f32>(GetStateTiming(luState)) * KF_JUNCTION_STATE_TIMING_UNIT;
}

inline bool JunctionLogicBox::IsLightRed(u32 luState, u32 luLight) const
{
    CGS_ASSERT(luState < muNumStates, "luState < muNumStates");   // BrnJunctionLogicBox.h:193
    CGS_ASSERT(luLight < muNumLights, "luLight < muNumLights");   // BrnJunctionLogicBox.h:194
    const u8 lu8LightBit = static_cast<u8>(1u << luLight);
    return (mauStoppedLightStates[luState] & lu8LightBit) == lu8LightBit;
}

}
