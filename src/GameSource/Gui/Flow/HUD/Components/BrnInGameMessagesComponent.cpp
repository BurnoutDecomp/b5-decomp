#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"        // BrnFlapt::FileRef::FindComponent (Prepare)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // BrnFlapt::MovieClipInstance::ResetTimeline

// Thin wrappers over the platform high-resolution timer (CgsTimeUtils.cpp); declared
// locally per the house style rather than through a header (there is none).
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

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

    // @ 0x824111B0 -- h:328. The fixed set of HUD message ids that may be refreshed in place
    // (rather than re-started) when the same id is re-queued while already showing. The X360
    // builds each 64-bit CgsID literal with lis/ori + insrdi and compares with cmpld against
    // the arg; reproduced as the full 64-bit hashes. Touches no members.
    bool InGameMessagesComponent::IsMessageUpdatable(CgsID lMessageId) const
    {
        if (lMessageId == 0xB9390CF6BDFC2D84ULL)
            return true;
        if (lMessageId == 0xB9390CF6BDFC65B5ULL)
            return true;
        if (lMessageId == 0xB9390CEDADB37000ULL)
            return true;
        return false;
    }

    // @ 0x8241F190 -- h:197. Resolve lacName within lFile, bind the located component's
    // MovieClipRef into the base mAptRef, rewind its timeline, and install the
    // transition-complete frame-trigger callback (passing `this` as the user data).
    void InGameMessagesComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        CGS_ASSERT(lacName != NULL, "lacName != NULL");

        BrnFlapt::MovieClipRef lComponentRef;
        lFile.FindComponent(&lComponentRef, lacName);
        mAptRef.mpMovieClipInst = lComponentRef.mpMovieClipInst;
        mAptRef.mpTransform     = lComponentRef.mpTransform;

        CGS_ASSERT(mAptRef.mpMovieClipInst != NULL, "mpMovieClipInst");
        mAptRef.mpMovieClipInst->ResetTimeline();

        mAptRef.SetFrameTriggerCallback(
            reinterpret_cast<void*>(&InGameMessagesComponent::TransitionCompleteCallback), this);
    }

    // @ 0x8243DDE0 -- h:207. Drive the live slot each frame: a WAITING message is started;
    // a VISIBLE one is ended once the current base time reaches its latched end-time.
    void InGameMessagesComponent::Update()
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");

        const MessageState leState =
            mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex];
        if (leState == E_MESSAGESTATE_WAITING)
        {
            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            StartMessage(&mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex]);
        }
        else if (leState == E_MESSAGESTATE_VISIBLE)
        {
            if (static_cast<u64>(CgsSystem::GetSystemTimerBaseTime()) >=
                mpInGameMessagesQueue->muCurrentEventEndTime)
            {
                EndMessage();
            }
        }
    }

    // @ 0x82411058 -- h:211. Apt frame-trigger: a TRANSIN slot is latched VISIBLE with its
    // end-time computed from the message duration (base time + duration * timer frequency);
    // a TRANSOUT slot is cleared to NOMESSAGE and the double buffer flipped.
    void InGameMessagesComponent::EndTransition()
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");

        const MessageState leState =
            mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex];
        if (leState == E_MESSAGESTATE_TRANSIN)
        {
            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            const u8 luIndex = mpInGameMessagesQueue->muCurrentMessageIndex;
            const f32 lfDuration = mpInGameMessagesQueue->maMessages[luIndex].mfDuration;
            const f32 lfTicks =
                static_cast<f32>(static_cast<f64>(CgsSystem::GetSystemTimerFrequency())) * lfDuration;
            const u64 luEndTime =
                static_cast<u64>(CgsSystem::GetSystemTimerBaseTime()) + static_cast<s64>(lfTicks);
            mpInGameMessagesQueue->muCurrentEventEndTime = luEndTime;

            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] =
                E_MESSAGESTATE_VISIBLE;
        }
        else if (leState == E_MESSAGESTATE_TRANSOUT)
        {
            mpInGameMessagesQueue->maeMessageState[GetCurrentIndex()] = E_MESSAGESTATE_NOMESSAGE;
            SwitchCurrentIndex();
        }
    }

    // @ 0x82410F18 -- h:302. Fill the NEXT double-buffer slot with lpEvent. If the slot is
    // free (NOMESSAGE) the event is copied in and marked WAITING; if it already holds a
    // WAITING message the incoming one only replaces it when it is higher priority.
    void InGameMessagesComponent::QueueMessage(BrnResource::HudMessageEvent* lpEvent)
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");

        const u8 luNextIndex = static_cast<u8>(1 - mpInGameMessagesQueue->muCurrentMessageIndex);
        CGS_ASSERT(mpInGameMessagesQueue->maeMessageState[luNextIndex] == E_MESSAGESTATE_NOMESSAGE
                       || mpInGameMessagesQueue->maeMessageState[luNextIndex] == E_MESSAGESTATE_WAITING,
                   "Invalid state for filling out the next message");

        BrnResource::HudMessageEvent* lpNext = &mpInGameMessagesQueue->maMessages[luNextIndex];
        if (mpInGameMessagesQueue->maeMessageState[luNextIndex] != E_MESSAGESTATE_NOMESSAGE)
        {
            if (lpEvent->miPriority > lpNext->miPriority)
            {
                std::memcpy(lpNext, lpEvent, sizeof(BrnResource::HudMessageEvent));
            }
        }
        else
        {
            std::memcpy(lpNext, lpEvent, sizeof(BrnResource::HudMessageEvent));
            mpInGameMessagesQueue->maeMessageState[luNextIndex] = E_MESSAGESTATE_WAITING;
        }
    }

    // @ 0x82411360 -- h:337. Apt frame-trigger callback shape (void*, u16). Forwards to
    // EndTransition on the InGameMessagesComponent supplied through lpUserData; luArg is
    // the trigger frame id, unused here.
    void InGameMessagesComponent::TransitionCompleteCallback(void* lpUserData, u16 /*luArg*/)
    {
        CGS_ASSERT(lpUserData != NULL, "lpUserData");
        static_cast<InGameMessagesComponent*>(lpUserData)->EndTransition();
    }
}
