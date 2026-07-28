#pragma once

// ===================================================================================
// BrnGui::GuiEventNetworkGameParams  -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h
//
// The network "game parameters" GUI event: the 10-round event list (one
// SpecificGameModeEventInterface::Event per round) followed by the scalar match
// options. AddGuiEvent<GuiEventNetworkGameParams> @0x823CFA88 wires it as
// AddEvent(&event, 257, 480) -- the payload is exactly 480 bytes.
//
// LAYOUT (corrected in the OnlineGameRoomPlayerInfo keystone, wave H). The Dec-07
// DWARF forward-declares this type, but the DWARF of its Create-flow twin
// (GuiEventNetworkCreateGame, BrnGuiEventTypeDefs.h:3222) carries the identical field
// row, and the X360 arbitrates every offset:
//   * Construct @0x824820C8's record loop anchors r3+0x24 with stride 0x2C: each
//     44-byte Event's mTrafficLightTriggerId(+0x20)=-1, miNumLandmarks(+0x24)=0,
//     miEventID(+0x28)=0 -- i.e. the inlined Event::Construct(0, -1, 0, 0) with the
//     constant-0 landmark copy and bounds assert compiled out. maEvents[0] is at +0,
//     NOT at +0x20 (an earlier reconstruction misread the +0x24 anchor as a 32-byte
//     event header; that layout could not reach its own +0x1B8 scalar tail).
//   * The scalar tail offsets are pinned by OnlineGameRoomPlayerInfo, which memcpy's
//     the payload to GuiCache+0xA800 and reads meGameMode@+440 / meSecurity@+448 /
//     miNumRounds@+464 / mbRanked@+479 (and pokes meGameMode/meSecurity before
//     re-outputting the event in PerformPauseOption @0x824A5F70).
//   * Construct's tail defaults: meGameMode=10 (E_MODE_ONLINE_RACE),
//     mePreviousGameMode=18 (the ARTIST E_MODE_COUNT sentinel -- the ARTIST enum has
//     18 modes; the Dec-07 DWARF ends at 17), security/boost/vehicle-choice/time=0,
//     miNumRounds=1, miVehicleClass=9, miNumRunnerCrashes=3, all four bools true.
// ===================================================================================

#include "types.hpp"
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // SpecificGameModeEventInterface::Event (44-byte round record)

#include <cstddef>   // offsetof (layout pin in _AssertLayout)

namespace BrnGui
{
    struct GuiEventNetworkGameParams
    {
        // One Event per online round (== GuiCache's 10-round options mirror).
        static const s32 KI_NUM_EVENTS = 10;

        // AddGuiEvent<GuiEventNetworkGameParams> @0x823CFA88 -> AddEvent(&event, 257, 480).
        s32 GetEventType() const { return 257; }

        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event Event;

        Event maEvents[KI_NUM_EVENTS];   // +0x000 (stride 44)

        // Scalar match-option tail (X360 +0x1B8..+0x1DF; names from the
        // GuiEventNetworkCreateGame DWARF field row, offsets X360-attested).
        s32  meGameMode;           // +0x1B8 (440) GsmIO::EGameModeType    (Construct: 10)
        s32  mePreviousGameMode;   // +0x1BC (444) GsmIO::EGameModeType    (Construct: 18 == ARTIST E_MODE_COUNT sentinel)
        s32  meSecurity;           // +0x1C0 (448) BrnNetwork::EBrnGameSecurity (0 public / 1 private / 2 closed; Construct: 0)
        s32  meBoostType;          // +0x1C4 (452) BrnNetwork::EBoostType  (Construct: 0)
        s32  meVehicleChoice;      // +0x1C8 (456) BrnNetwork::EVehicleChoice (Construct: 0)
        s32  miTimeLimit;          // +0x1CC (460)                          (Construct: 0)
        s32  miNumRounds;          // +0x1D0 (464)                          (Construct: 1)
        s32  miVehicleClass;       // +0x1D4 (468)                          (Construct: 9)
        s32  miNumRunnerCrashes;   // +0x1D8 (472)                          (Construct: 3)
        bool mbInfiniteBoost;      // +0x1DC (476)                          (Construct: true)
        bool mbTrafficOn;          // +0x1DD (477)                          (Construct: true)
        bool mbTrafficCheckingOn;  // +0x1DE (478)                          (Construct: true)
        bool mbRanked;             // +0x1DF (479)                          (Construct: true)

        // Owned by THIS TU: default-construct the event (@0x824820C8).
        void Construct();

    private:
        // Never called -- complete-class context. The whole record is pointer-free, so
        // the host layout must equal the X360 payload byte-for-byte (the game-room
        // screen memcpy's 480 bytes between this type and the GuiCache mirror).
        static void _AssertLayout()
        {
            static_assert(sizeof(Event) == 44,                                       "Event round-record stride");
            static_assert(offsetof(GuiEventNetworkGameParams, meGameMode) == 440,    "meGameMode @+0x1B8");
            static_assert(offsetof(GuiEventNetworkGameParams, meSecurity) == 448,    "meSecurity @+0x1C0");
            static_assert(offsetof(GuiEventNetworkGameParams, miNumRounds) == 464,   "miNumRounds @+0x1D0");
            static_assert(offsetof(GuiEventNetworkGameParams, mbRanked) == 479,      "mbRanked @+0x1DF");
            static_assert(sizeof(GuiEventNetworkGameParams) == 480,                  "the 480-byte wire payload");
        }
    };
}
