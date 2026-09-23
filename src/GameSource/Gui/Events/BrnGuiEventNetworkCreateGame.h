#pragma once

// ===================================================================================
// BrnGui::GuiEventNetworkCreateGame  -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h
//
// The network "create game" GUI event: a fixed array of ten per-event mode-event
// records followed by a block of scalar option / count fields. This is the event the
// online front-end fires when the host finalises a freeburn/online match's setup; the
// ten records carry the per-event landmark/trigger configuration and the scalar tail
// carries the match-level options (max players, game mode, etc.).
//
// Layout proven from BURNOUT_X360_ARTIST.XEX:
//   * Construct        @0x82481F50 - record loop anchors r3+0x24, stride 0x2C (44 bytes),
//                       10 reps, then scalar stores at r3+0x1B8 .. r3+0x1DF.
//   * SetFromGameParams @0x82481FC8 - copies each record through
//                       BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface
//                       ::Event::Construct(eventId, trafficLightTriggerId, landmarks[], n)
//                       which proves the record IS that committed 44-byte Event type
//                       (landmark halfword block @+0x00..+0x1F, trigger@+0x20,
//                       numLandmarks@+0x24, eventID@+0x28), then copies the scalar tail.
//
// The ten Events (10 * 44 == 0x1B8) place the scalar block exactly at +0x1B8, matching
// the Construct stores. All members are accessed by name; the committed Event type is
// reused by name (no parallel re-declaration).
// ===================================================================================

#include "types.hpp"
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event + LandmarkIndex
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"  // BrnGui::GuiEventNetworkGameParams (SetFromGameParams source)

namespace BrnGui
{
    struct GuiEventNetworkCreateGame
    {
        // Number of per-event records the X360 Construct loop initialises (10 reps).
        static const s32 KI_NUM_EVENTS = 10;

        // One per-event record. Reuses the committed mode-event record type by name
        // (BrnGameStateSharedIO.h): a 44-byte Event whose leading 32 bytes are the
        // landmark-index block (set later by SetFromGameParams) and whose 12-byte tail is
        // {mTrafficLightTriggerId@+0x20, miNumLandmarks@+0x24, miEventID@+0x28}.
        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event Event;

        // The ten mode-event records (X360 anchor r3+0x00, stride 0x2C). 10 * 44 == 0x1B8,
        // so the scalar block below lands exactly at +0x1B8 as the binary requires.
        Event maEvents[KI_NUM_EVENTS];

        // Scalar match-option tail seeded by Construct (+0x1B8 .. +0x1DF). The same field row,
        // names and offsets as GuiEventNetworkGameParams (the reference declares the two
        // records alike); the Construct defaults are that record's too.
        s32  meGameMode;           // +0x1B8 GsmIO::EGameModeType (Construct: 10)
        s32  mePreviousGameMode;   // +0x1BC GsmIO::EGameModeType (Construct: 18)
        s32  meSecurity;           // +0x1C0 BrnNetwork::EBrnGameSecurity (Construct: 0)
        s32  meBoostType;          // +0x1C4 (Construct: 0)
        s32  meVehicleChoice;      // +0x1C8 (Construct: 0)
        s32  miTimeLimit;          // +0x1CC (Construct: 0)
        s32  miNumRounds;          // +0x1D0 (Construct: 1)
        s32  miVehicleClass;       // +0x1D4 (Construct: 9)
        s32  miNumRunnerCrashes;   // +0x1D8 (Construct: 3)
        bool mbInfiniteBoost;      // +0x1DC (Construct: true)
        bool mbTrafficOn;          // +0x1DD (Construct: true)
        bool mbTrafficCheckingOn;  // +0x1DE (Construct: true)
        bool mbRanked;             // +0x1DF (Construct: true)

        // @0x82481F50 - default-construct the event: zero each record's trigger/count/id
        // tail (trigger = -1, numLandmarks = 0, eventID = 0) and seed the scalar block with
        // the default online-match parameters. The landmark blocks are left untouched here.
        void Construct();

        // Copy a game-params event: rebuild each of the ten Events via
        // Event::Construct (gathering the source's landmark indices) and copy the scalar
        // option block verbatim.
        void SetFromGameParams(const GuiEventNetworkGameParams* lpGameParams);

        // The queued event-type id. Not GuiEvent<N>-derived, so the id is carried here;
        // X360-attested by StateInterface::OutputGuiEvent<GuiEventNetworkCreateGame>
        // @0x82493ED8 -> AddEvent(&wrapper, 40, ...) with the inner record type 256, size 480.
        s32 GetEventType() const { return 256; }
    };
}
