#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::InGameMessagesComponent -- the HUD in-game message queue driver, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Seven X360-emitted functions: the double-buffered slot accessors
// (GetCurrentIndex/GetNextIndex/SwitchCurrentIndex), the controller/director/game-mode
// latches (SetController/SetDirector/SetGameMode) and the queue adopt-and-reconcile
// (SetInGameMessagesQueue).

namespace BrnGui
{
    // @ 0x8240EA80 -- h:310. Return the queue's live message slot index.
    u8 InGameMessagesComponent::GetCurrentIndex() const
    {
        CGS_ASSERT(mpInGameMessagesQueue != 0, "mpInGameMessagesQueue != NULL");
        return mpInGameMessagesQueue->muCurrentMessageIndex;
    }

    // @ 0x8240EAE0 -- h:314. The other of the two slots (double-buffered queue).
    u8 InGameMessagesComponent::GetNextIndex() const
    {
        CGS_ASSERT(mpInGameMessagesQueue != 0, "mpInGameMessagesQueue != NULL");
        return static_cast<u8>(1 - mpInGameMessagesQueue->muCurrentMessageIndex);
    }

    // @ 0x8240EB48 -- BrnInGameMessagesComponent.h:429. Toggle the double-buffer slot selector
    // in place: muCurrentMessageIndex = 1 - muCurrentMessageIndex (uint8_t; the asm does
    // subfic r10,r10,1 on the lbz'd byte and stb's it back). The queue pointer is asserted
    // non-NULL; the store follows regardless.
    void InGameMessagesComponent::SwitchCurrentIndex()
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");

        mpInGameMessagesQueue->muCurrentMessageIndex =
            static_cast<u8>(1 - mpInGameMessagesQueue->muCurrentMessageIndex);
    }

    // @ 0x82472BE8 -- h:216. Latch the HUD message controller pointer.
    void InGameMessagesComponent::SetController(const HudMessageController* lpController)
    {
        CGS_ASSERT(lpController != 0, "lpController");
        mpMessageController = lpController;
    }

    // @ 0x82472C48 -- h:221. Latch the HUD message director pointer.
    void InGameMessagesComponent::SetDirector(const HudMessageDirector* lpDirector)
    {
        CGS_ASSERT(lpDirector != 0, "lpDirector");
        mpDirector = lpDirector;
    }

    // @ 0x82472B80 -- h:244. Latch the current game-mode. The X360 range guard uses the guest
    // GsmIO::E_MODE_COUNT (== 18) as the upper bound; reproduced as the literal from the asm
    // (the committed SharedIO enum spells E_MODE_COUNT as 17, so the numeric bound is written
    // out rather than the symbol to stay store-for-store).
    void InGameMessagesComponent::SetGameMode(BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameMode)
    {
        CGS_ASSERT((BrnGameState::GameStateModuleIO::E_MODE_NONE <= leCurrentGameMode) && (leCurrentGameMode < 18),
                   "(GsmIO::E_MODE_NONE <= leCurrentGameMode) && (GsmIO::E_MODE_COUNT > leCurrentGameMode)");
        meCurrentGameMode = leCurrentGameMode;
    }

    // @ 0x82475B80 -- h:239. Adopt the shared message queue and reconcile its live slot: a slot
    // mid-TRANSIN (state 2) is rewound to WAITING (1); any other in-flight state is cleared (0)
    // and the current index flipped so the next Update re-picks the slot. muCurrentMessageIndex
    // indexes maeMessageState (the +0x22C word-offset the asm forms is (556 + idx) words ==
    // &maeMessageState[idx]).
    void InGameMessagesComponent::SetInGameMessagesQueue(InGameMessagesQueue* lInGameInMessagesQueue)
    {
        CGS_ASSERT(lInGameInMessagesQueue != 0, "lInGameInMessagesQueue != NULL");

        mpInGameMessagesQueue = lInGameInMessagesQueue;

        const u8 luIndex = lInGameInMessagesQueue->muCurrentMessageIndex;
        const MessageState leState = lInGameInMessagesQueue->maeMessageState[luIndex];
        if (leState != E_MESSAGESTATE_NOMESSAGE)
        {
            if (leState == E_MESSAGESTATE_TRANSIN)
            {
                lInGameInMessagesQueue->maeMessageState[luIndex] = E_MESSAGESTATE_WAITING;
            }
            else
            {
                lInGameInMessagesQueue->maeMessageState[luIndex] = E_MESSAGESTATE_NOMESSAGE;
                SwitchCurrentIndex();
            }
        }
    }
}
