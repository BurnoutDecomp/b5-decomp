// BrnGuiEventRankProgressResponse.h
// Home of BrnGui::GuiEventRankProgressResponse -- the GUI event payload carrying a
// player's rank-progress query response (GUI event 438, 36 bytes).
//
// X360 ARTIST bodies this slice reconstructs:
//
//   Construct      @ 0x824F62F8 -> repack a GameStateModuleIO::RankInfoResponseAction into
//                                  this event. NINE word copies and ONE ROTATION: the action's
//                                  word 0 (its player rank + the "finished last rank" sentinel)
//                                  lands at +0x20 while its words 1..8 slide down to +0x00..+0x1C.
//   GetPlayerRank  @ 0x8240EA28 -> returns miCurrentRank (s32 @ +0x20) after a non-fatal
//                                  guard that it is not the "finished last rank" sentinel
//                                  (KI_PLAYER_HAS_FINISHED_LAST_RANK == -1). DWARF assert
//                                  site GameSource/Gui/BrnGuiEventTypeDefs.h:7841.
//
// ⭐ LAYOUT IS NOW FULLY NAMED, AND BOTH ENDS AGREE. It used to be `u8 maHeadReserved[0x20]`
// plus miCurrentRank, because the only reconstructed body (GetPlayerRank) touched +0x20 alone.
// Construct pins the other eight words directly out of the image -- `lwz r11, N(r4) / stw r11,
// (N-4)(r3)` for N = 4..0x20, plus `lwz r11, 0(r4) / stw r11, 0x20(r3)` -- and the DecFIGS DWARF
// (BrnGuiEventTypeDefs.h:5089..5104) names them in exactly that order: miRaceRank, miRoadRageRank,
// miStuntAttackRank, miMarkedManRank, then the four *RankWins, then the private miCurrentRank.
// The producer side corroborates member-for-member: RankInfoResponseAction (BrnGameActions.h) is
// { miPlayerRank, miOfflineRace, miRoadRage, miStuntAttack, miMarkedMan, + the same four
// *RankWins }, i.e. the identical eight fields with the rank word at the FRONT instead of the
// back. So the head is no longer an honest-but-opaque reserve; it is attested.
//
// ⓘ THE EVENT ID IS THE X360's, NOT THE DWARF's: the PS3 DWARF declares
// `GuiEventRankProgressResponse : GuiEvent<433>`, while AddGuiEvent<GuiEventRankProgressResponse>
// @0x823D7290 posts 438 -- and 438 is what CrashNavDriverDetails' maiEventToObserve table
// (read from .rdata @0x820664F0) actually registers for. Same +5 GUI-event drift the rest of
// this band carries.

#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event base

namespace BrnGameState { namespace GameStateModuleIO { struct RankInfoResponseAction; } }

namespace BrnGui
{
    // Sentinel returned by the rank system when a player has dropped out of (finished)
    // the last rank; GetPlayerRank asserts the stored rank is not this value. X360
    // compares miCurrentRank against -1.
    //
    // ⓘ The DWARF declares this as a member constant of the struct below
    // (BrnGuiEventTypeDefs.h:5100). It is kept at namespace scope here because the committed
    // GetPlayerRank body and its assert text already spell it unqualified; the value and
    // meaning are the DWARF's.
    const s32 KI_PLAYER_HAS_FINISHED_LAST_RANK = -1;

    // The rank-progress query response GUI event. Member names/order and the private-ness of
    // miCurrentRank are the DecFIGS DWARF's (BrnGuiEventTypeDefs.h:5089..5104); the byte offsets
    // are Construct's and GetPlayerRank's, and they agree.
    struct GuiEventRankProgressResponse : public CgsModule::Event
    {
        // @ 0x824F62F8 -- repack the game action into this event (see the banner). The X360
        // returns `this`; the return value is dropped, per the project convention for
        // Construct-style helpers whose result no call site reads (its ONE caller,
        // BrnGameModule::TranslateGameActionsToGuiEvents case 181 @0x823ECC90, discards r3).
        void Construct(const BrnGameState::GameStateModuleIO::RankInfoResponseAction* lpAction);

        // @ 0x8240EA28 -- return the current rank, asserting it is not the
        // "finished last rank" sentinel (the X360 returns the value regardless).
        s32 GetPlayerRank() const;

        // DWARF BrnGuiEventTypeDefs.h:5083 -- `bool HasPlayerFinishedLastRank() const`. The X360
        // emits NO standalone symbol for it: its one attested consumer,
        // CrashNavDriverDetails::UpdateSetupLicense @0x824C1C58, inlines the single comparison
        // (`lwz r11, 0x20(r30); cmpwi r11, -1` @0x824C1CE4) rather than calling GetPlayerRank --
        // because the sentinel is exactly what it is testing for and GetPlayerRank asserts
        // against it. Defined inline here for that reason, the same precedent as
        // Profile::GetNumWinsForGameMode. One comparison, no layout change.
        bool HasPlayerFinishedLastRank() const
        {
            return miCurrentRank == KI_PLAYER_HAS_FINISHED_LAST_RANK;
        }

        // AddGuiEvent<GuiEventRankProgressResponse> @0x823D7290 -> AddEvent(&event, 438, 36).
        s32 GetEventType() const { return 438; }

        // MERGE (main-menu wave F1 x the driver-details wave, 2026-08-29 rebase): both
        // sessions independently NAMED the formerly-reserved 0x20-byte head. The DWARF rows
        // (BrnGuiEventTypeDefs.h:5089..5097) win the member names; the accessor faces below
        // are kept because CrashNavPanel::RecEvent's id-438 arm (@0x82442048..0x8244208C)
        // reads the record through them (eight words -> EventPanel::SetModeRanks +
        // SetModeRankWins, the order the consumer types). No member shifts: 8*4 == 0x20 and
        // miCurrentRank stays at its X360-proven +0x20.
        s32 GetRaceRank() const            { return miRaceRank; }
        s32 GetRoadRageRank() const        { return miRoadRageRank; }
        s32 GetStuntAttackRank() const     { return miStuntAttackRank; }
        s32 GetMarkedManRank() const       { return miMarkedManRank; }
        s32 GetRaceRankWins() const        { return miOfflineRaceRankWins; }
        s32 GetRoadRageRankWins() const    { return miRoadRageRankWins; }
        s32 GetStuntAttackRankWins() const { return miStuntAttackRankWins; }
        s32 GetMarkedManRankWins() const   { return miMarkedManRankWins; }

        // The raw +0x20 word WITHOUT GetPlayerRank's sentinel assert -- CrashNavPanel::RecEvent
        // tests it against KI_PLAYER_HAS_FINISHED_LAST_RANK *before* deciding whether to call
        // GetPlayerRank at all (`if (a2[8] == -1)` @0x82441FA0), so it must not trip that assert.
        s32 GetCurrentRankRaw() const      { return miCurrentRank; }

        // Construct's destination words, in its own store order. Names + order: DWARF
        // BrnGuiEventTypeDefs.h:5089..5097.
        s32 miRaceRank;              // @0x00  <- action.miOfflineRace
        s32 miRoadRageRank;          // @0x04  <- action.miRoadRage
        s32 miStuntAttackRank;       // @0x08  <- action.miStuntAttack
        s32 miMarkedManRank;         // @0x0C  <- action.miMarkedMan
        s32 miOfflineRaceRankWins;   // @0x10  <- action.miOfflineRaceRankWins
        s32 miRoadRageRankWins;      // @0x14  <- action.miRoadRageRankWins
        s32 miStuntAttackRankWins;   // @0x18  <- action.miStuntAttackRankWins
        s32 miMarkedManRankWins;     // @0x1C  <- action.miMarkedManRankWins

    private:
        s32 miCurrentRank;           // @0x20  <- action.miPlayerRank (DWARF :5104, private)
    };
}
