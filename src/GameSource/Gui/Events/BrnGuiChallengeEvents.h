#pragma once

// ===================================================================================
// BrnGui freeburn-challenge GUI event payloads -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiChallengeEvents.h
//
// The three freeburn-challenge GUI events consumed by BrnGui::FreeburnChallengeManager:
//   * GuiChallengeStartEvent       -- start a freeburn challenge (host/local-host flag)
//   * GuiChallengeTriggerResponse  -- trigger an already-known challenge
//   * GuiChallengeUpdateEvent      -- a live progress update for the running challenge
//
// No prior source carries these structs' member layouts: the DecFIGS DWARF only
// forward-declares them (GameSource/Gui/BrnGuiEventTypes.h:63/77/81/83 -- declarations
// with no member list). Every field below is recovered from the X360 BURNOUT_X360_ARTIST
// load sequence of the three manager handlers that read them:
//   StartChallenge   @0x82509D60  (reads GuiChallengeStartEvent)
//   TriggerChallenge @0x8250A160  (reads GuiChallengeTriggerResponse)
//   HandleNewData    @0x824F3FC8  (reads GuiChallengeUpdateEvent)
//
// All three are GUI events, so they derive from CgsModule::Event (an empty base, so the
// first data member sits at object offset 0 -- which is exactly what the X360 reads:
// `ld r28, 0(r30)` for the challenge id, `lbz r,8(r30)` for the local-host flag, and the
// `lwz r,0(r29)` / XMemCpy runs in HandleNewData all index from offset 0).
// ===================================================================================

#include "types.hpp"                                              // s32/f32/u8 widths
#include "BrnCommonTypes.h"                                       // CgsID (typedef u64)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event base

namespace BrnGui
{
    // GuiChallengeStartEvent (DWARF: BrnGuiEventTypes.h:77, forward-declared only).
    // X360 StartChallenge @0x82509D60 reads exactly two fields:
    //   `ld r28, 0(r30)`  -> the challenge id (passed to ChallengeList::GetChallengeIndex,
    //                        whose argument is a CgsID, proving an 8-byte id at +0x00)
    //   `lbz r10, 8(r30)` -> a one-byte local-host flag (stored into the manager's
    //                        mbIsLocalHost at +0x18)
    struct GuiChallengeStartEvent : public CgsModule::Event
    {
        CgsID mChallengeID;     // @+0x00  (ld; 8-byte id)
        bool  mbIsLocalHost;    // @+0x08  (lbz; one-byte host flag)
    };

    // GuiChallengeTriggerResponse (DWARF: BrnGuiEventTypes.h:81, forward-declared only).
    // X360 TriggerChallenge @0x8250A160 reads the identical two-field shape as
    // GuiChallengeStartEvent: `ld r28, 0(r30)` (challenge id) + `lbz r11, 8(r30)`
    // (local-host flag). Modelled as its own type (distinct DWARF symbol) with the same
    // recovered layout.
    struct GuiChallengeTriggerResponse : public CgsModule::Event
    {
        CgsID mChallengeID;     // @+0x00  (ld; 8-byte id)
        bool  mbIsLocalHost;    // @+0x08  (lbz; one-byte host flag)
    };

    // GuiChallengeUpdateEvent (DWARF: BrnGuiEventTypes.h:83, forward-declared only).
    // X360 HandleNewData @0x824F3FC8 reads this event with two scalar loads at the front
    // followed by three XMemCpy runs sized 4/32/32 bytes * miNumTargetsUsed:
    //   `lwz r11, 0(r29)`                          -> miCurrentAction        @+0x00
    //   `lwz r11, 4(r29)` (asserted == miTargetsCount) -> miNumTargetsUsed   @+0x04
    //   XMemCpy(this+0x68, r29+0x08, 0x20*count)   -> per-ARCI contributions @+0x08
    //   XMemCpy(this+0xA8, r29+0x48, 0x04*count)   -> per-target remaining   @+0x48
    //   XMemCpy(this+0x28, r29+0x50, 0x20*count)   -> per-ARCI completion     @+0x50
    // Element widths follow the manager's destination arrays (f32 contributions, s32
    // remaining, 4-byte completion enum). KI_MAX_TARGETS/KI_MAX_ARCI mirror the manager's
    // maafIndividualTargetContributions[2][8] / maaeComplete[2][8] so the source blocks
    // are exactly the size of the destination copies.
    struct GuiChallengeUpdateEvent : public CgsModule::Event
    {
        static const s32 KI_MAX_TARGETS = 2;   // == FreeburnChallengeManager::KI_MAX_TARGETS
        static const s32 KI_MAX_ARCI    = 8;   // == FreeburnChallengeManager::KI_MAX_ARCI

        s32 miCurrentAction;                                              // @+0x00
        s32 miNumTargetsUsed;                                             // @+0x04
        f32 maafIndividualTargetContributions[KI_MAX_TARGETS][KI_MAX_ARCI]; // @+0x08 (0x40)
        s32 maiOverallTargetRemaining[KI_MAX_TARGETS];                   // @+0x48 (0x08)
        // Per-ARCI completion state. Real element type is
        // BrnGameState::GameStateModuleIO::EFreeburnChallengeSuccess (mirrored by
        // FreeburnChallengeManager::EFreeburnChallengeSuccess); modelled here as the
        // 4-byte s32 it copies byte-for-byte into, to keep this event header free of the
        // GameState-IO / manager header graph. @+0x50 (0x40).
        s32 maaeComplete[KI_MAX_TARGETS][KI_MAX_ARCI];                   // @+0x50 (0x40)
    };
}
