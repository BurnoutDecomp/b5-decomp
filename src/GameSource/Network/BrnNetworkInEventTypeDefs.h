#pragma once

// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent -- owning header
//   b5-decomp/src/GameSource/Network/BrnNetworkInEventTypeDefs.h
//
// The "select scoreboard" network IN-event the GUI raises to drive the scoreboard browser
// (BrnGame::BrnGameModule::TranslateGuiEventsToNetworkEvents and the
// BrnNetwork::ScoreboardDebugComponent forward into these three factory builders).
//
// SHAPE recovered from the X360 builder bodies (GetIndexes @ 0x823A6350, GetVariations
// @ 0x823A63B8, GetScoreboard @ 0x823A6420; asserts cited against
// GameSource/Network/BrnNetworkInEventTypeDefs.h:1031/1048/1065). Each builder takes one
// selection value and a discriminator, storing exactly two 32-bit words:
//
//   +0x00 (4)  miCategory    set by GetIndexes(liCategory)     (asm `stw r30, 0(r31)`)
//   +0x04 (4)  miIndex       set by GetVariations(liIndex)     (asm `stw r30, 4(r31)`)
//   +0x08 (4)  miVariation   set by GetScoreboard(liVariation) (asm `stw r30, 8(r31)`)
//   +0x0C (4)  meSelectType  the level the event selects       (asm `li r11, N; stw r11, 0xC(r31)`)
//
// Each builder writes ONLY its own value field + meSelectType (the other value fields are
// left untouched, matching the two-store-each asm). meSelectType takes 2/3/4 for
// Indexes/Variations/Scoreboard respectively (the asm `li r11, 2|3|4`).
//
// DWARF spells the event BrnNetwork::BrnNetworkModuleIO::NetworkEvent<N> (the empty Event
// spine + a GetEventType() that returns the compile-time tag N). The concrete tag N for this
// leaf is not reachable from this slice (the three builders never store it), so the event is
// modelled here on the empty BrnNetwork::Event base directly -- which is byte-identical to the
// NetworkEvent<N> base (no data members, first field at +0x00). FLAG: re-base onto
// NetworkEvent<N> with the recovered tag when the BrnNetwork module-IO event-type table lands
// (the field offsets must not move).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (builder >= 0 guards)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"      // BrnNetwork::Event (empty event spine base)
#include "GameSource/Network/Shared Server Types/BrnNetworkSharedServerTypes.h" // ServerGeneratedTypes::OfflineProgressionT

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // DWARF (BrnNetworkInEventTypeDefs.h:438) -- the IN-event that delivers a player's
    // accumulated offline-play progress to the network player-stats manager. DWARF spells it
    // `: public NetworkEvent<33>` (tag 33); the empty BrnNetwork::Event base is byte-identical
    // to that NetworkEvent<N> base (no data members, first field at +0x00 -- see the
    // NetworkInSelectScoreboardEvent note below), so it is modelled on Event directly.
    //
    // LAYOUT is X360-AUTHORITATIVE from NetworkPlayerStatsManager::HandleOfflineProgressionEvent
    // (@0x82546BB0): that body memcpy's exactly 0x44 == 68 bytes of this event into its buffered
    // copy, and reads a 32-bit word at event+0x40 == +64 as the freeburn-challenge success count
    // (handed to ServerInterfaceCustomCommands::UploadOfflineProgress as its count argument).
    // The 64-byte OfflineProgressionT at +0 plus that trailing s32 give the 68-byte total. The
    // trailing count field is not present in the (incomplete) DWARF member list but is required
    // by the asm; FLAGGED as asm-recovered.
    struct NetworkInOfflineProgression : public Event
    {
        ServerGeneratedTypes::OfflineProgressionT mOfflineProgression; // +0x00 (64 bytes)
        s32                                       miFreeburnChallengeSuccessCount; // +0x40 (asm-recovered)
    };

    struct NetworkInSelectScoreboardEvent : public Event
    {
        // Which scoreboard browsing level this event selects (the +0xC discriminator).
        enum ESelectType
        {
            E_SELECT_INDEXES    = 2,   // GetIndexes    set category, browse indexes
            E_SELECT_VARIATIONS = 3,   // GetVariations set index, browse variations
            E_SELECT_SCOREBOARD = 4,   // GetScoreboard set variation, show scoreboard
        };

        s32 miCategory;     // +0x00
        s32 miIndex;        // +0x04
        s32 miVariation;    // +0x08
        s32 meSelectType;   // +0x0C (ESelectType)

        // @ 0x823A6350 -- build a "select indexes" event for liCategory (asserts liCategory >= 0).
        static NetworkInSelectScoreboardEvent GetIndexes(s32 liCategory);
        // @ 0x823A63B8 -- build a "select variations" event for liIndex (asserts liIndex >= 0).
        static NetworkInSelectScoreboardEvent GetVariations(s32 liIndex);
        // @ 0x823A6420 -- build a "show scoreboard" event for liVariation (asserts liVariation >= 0).
        static NetworkInSelectScoreboardEvent GetScoreboard(s32 liVariation);
    };
}
} // namespace BrnNetwork
