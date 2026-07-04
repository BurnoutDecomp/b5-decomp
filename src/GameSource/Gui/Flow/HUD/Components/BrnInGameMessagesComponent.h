#pragma once

// BrnGui::InGameMessagesComponent -- owning header.
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h
//   class:BrnGui::InGameMessagesComponent
//
// The HUD component that drives the in-game message queue (the double-buffered
// TRANSIN/WAITING message slot machine). This is a MINIMAL-COMPLETE slice: it homes the
// seven X360-emitted accessor/mutator functions the InGameMessagesComponent TU owns
// (GetCurrentIndex/GetNextIndex/SwitchCurrentIndex/SetController/SetDirector/SetGameMode/
// SetInGameMessagesQueue), plus the shared InGameMessagesQueue view and the MessageState
// enum they read. Member list/order + offsets from the DecFIGS DWARF
// (BrnInGameMessagesComponent.h).
//
// FLAG (foreign types): the controller/director pointers reference BrnGui::HudMessageController
// (BrnGuiHudMessageDirector.h forward-decl / SharedClasses/DataLists/BrnHudMessageController.h)
// and BrnGui::HudMessageDirector (BrnGuiHudMessageDirector.h); pointer-only here, so
// forward-declared. The InGameMessagesQueue's full layout is un-homed; it is modelled as
// correctly-sized opaque storage so the two X360-pinned member offsets
// (maeMessageState @ +0x8B0 / muCurrentMessageIndex @ +0x8C0) are exact.

#include "types.hpp"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType

namespace BrnGui
{
    class HudMessageController;   // BrnGuiHudMessageDirector.h (pointer-only)
    struct HudMessageDirector;    // BrnGuiHudMessageDirector.h (pointer-only)

    // The per-slot message lifecycle state (DWARF). The double-buffered queue advances
    // each slot NOMESSAGE -> WAITING -> TRANSIN as messages are queued and shown.
    enum MessageState
    {
        E_MESSAGESTATE_NOMESSAGE = 0,
        E_MESSAGESTATE_WAITING   = 1,
        E_MESSAGESTATE_TRANSIN   = 2,
    };

    // The shared in-game message queue the component drives. MINIMAL SLICE: only the two
    // members the InGameMessagesComponent accessors touch are materialised at their
    // X360-pinned offsets; the rest of the (large) queue layout is opaque padding.
    //   maeMessageState[]      @ +0x8B0 (2224)  -- per-slot lifecycle state (word each; the
    //                                              asm forms &maeMessageState[idx] as
    //                                              (556 + idx) words from the base)
    //   muCurrentMessageIndex  @ +0x8C0 (2240)  -- live slot selector (0 or 1)
    struct InGameMessagesQueue
    {
        // Opaque prefix up to maeMessageState (+0x8B0).
        unsigned char maPre[2224];              // +0x000..+0x8AF (opaque)
        // Two-slot double-buffered state; only [0]/[1] are indexed by muCurrentMessageIndex.
        MessageState  maeMessageState[2];       // +0x8B0 (h:91-adjacent)
        // Padding between the state array and the current-index byte.
        unsigned char maBetween[2240 - 2232];   // +0x8B8..+0x8BF (opaque)
        u8            muCurrentMessageIndex;     // +0x8C0 (h:91)
    };

    // BrnGui::InGameMessagesComponent (DWARF BrnInGameMessagesComponent.h).
    class InGameMessagesComponent
    {
    public:
        // @ 0x82472BE8 -- h:216. Latch the HUD message controller pointer.
        void SetController(const HudMessageController* lpController);
        // @ 0x82472C48 -- h:221. Latch the HUD message director pointer.
        void SetDirector(const HudMessageDirector* lpDirector);
        // @ 0x82472B80 -- h:244. Latch the current game-mode (range-guarded).
        void SetGameMode(BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameMode);
        // @ 0x82475B80 -- h:239. Adopt the shared message queue and reconcile its live slot.
        void SetInGameMessagesQueue(InGameMessagesQueue* lInGameInMessagesQueue);

    private:
        // @ 0x8240EA80 -- h:310. The queue's live message slot index.
        u8   GetCurrentIndex() const;
        // @ 0x8240EAE0 -- h:314. The other slot (double-buffered queue).
        u8   GetNextIndex() const;
        // @ 0x8240EB48 -- h:318. Toggle the double-buffer slot selector in place.
        void SwitchCurrentIndex();

        // ORDER + offsets from the DWARF. mpInGameMessagesQueue is the only offset-pinned
        // member the TU attests (this + 0xC, DWARF h:258); the controller/director/game-mode
        // members follow (their exact offsets are not separately pinned by this slice).
        unsigned char        maPrePad[0xC];         // +0x0..+0xB (base/leading members, opaque)
        InGameMessagesQueue* mpInGameMessagesQueue;  // +0xC (h:258)
        const HudMessageController* mpMessageController;
        const HudMessageDirector*   mpDirector;
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentGameMode;
    };
}
