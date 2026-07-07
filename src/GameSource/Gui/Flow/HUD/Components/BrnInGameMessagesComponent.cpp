#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"        // BrnFlapt::FileRef::FindComponent (Prepare)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // BrnFlapt::MovieClipInstance::ResetTimeline
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"     // BrnGui::HudMessageDirector::IsMessageAllowed (TerminateMessages)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers (+0x14 serialiser slot)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface::GetAccessPointers
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"          // BrnReplays::GuiModuleStaticLayout (UpdateInPlace tail)

// Thin wrappers over the platform high-resolution timer (CgsTimeUtils.cpp); declared
// locally per the house style rather than through a header (there is none).
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

// Compile-only slice of the GUI-module replay serialiser reached from the GUI
// access-pointer block (mirrors BrnReplayHudMessageComponent.cpp). The full type + its
// GetStaticLayout body live in GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.cpp
// (no shared header). UpdateInPlace's serialiser tail calls GetStaticLayout() to reach the
// GUI-module static layout and re-run its StartMessage flags.
namespace BrnReplays
{
    class GuiModuleSerialiser
    {
    public:
        GuiModuleStaticLayout* GetStaticLayout();
    };
}

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

    // @ 0x8243DC68 -- h:203. Ask the message controller to format lpEvent into a live
    // HudMessageEvent, then dispatch it against the current slot: an already-showing entry
    // whose id matches and is updatable is refreshed in place; otherwise the incoming
    // message replaces a lower-priority pending one (kept via QueueMessage) or, when it wins
    // (or the slot is free), starts immediately. The X360 reads the incoming event's
    // miForceRemoveThreshold (+0xC) against the slot's miPriority (+8) for the keep/start
    // decision; reproduced verbatim.
    void InGameMessagesComponent::AddMessage(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL, "Invalid event passed in");

        if (mpMessageController != NULL)
        {
            BrnResource::HudMessageEvent lOutMessage;
            if (reinterpret_cast<const BrnResource::HudMessageController*>(mpMessageController)
                    ->GetMessage(reinterpret_cast<const GuiHudMessage*>(lpEvent), &lOutMessage))
            {
                const u8 luIndex = GetCurrentIndex();
                if (mpInGameMessagesQueue->maeMessageState[luIndex] != E_MESSAGESTATE_NOMESSAGE)
                {
                    if (lOutMessage.mHudMessageId ==
                            mpInGameMessagesQueue->maMessages[GetCurrentIndex()].mHudMessageId
                        && IsMessageUpdatable(lOutMessage.mHudMessageId))
                    {
                        UpdateInPlace(&lOutMessage);
                        return;
                    }
                    if (lOutMessage.miForceRemoveThreshold <
                            mpInGameMessagesQueue->maMessages[GetCurrentIndex()].miPriority)
                    {
                        QueueMessage(&lOutMessage);
                        return;
                    }
                }
                StartMessage(&lOutMessage);
            }
        }
    }

    // @ 0x8241F530 -- h:323. Refresh the live slot's message in place. The incoming event
    // must carry the same id as the slot (else the "different message type" tripwire) and the
    // slot must be occupied. Copy the new event in; if the slot is past WAITING, re-latch the
    // end-time (base time + duration * timer frequency), mark it VISIBLE and re-run the update
    // animation. Finally re-raise the GUI-module static-layout StartMessage flags.
    void InGameMessagesComponent::UpdateInPlace(BrnResource::HudMessageEvent* lpEvent)
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");

        CGS_ASSERT(lpEvent->mHudMessageId ==
                       mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex].mHudMessageId,
                   "Trying to update a different message type");

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        CGS_ASSERT(mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] !=
                       E_MESSAGESTATE_NOMESSAGE,
                   "Invalid state to update a message in");

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        if (&mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex] != lpEvent)
        {
            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            std::memcpy(&mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex],
                        lpEvent, sizeof(BrnResource::HudMessageEvent));
        }

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        if (mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] !=
            E_MESSAGESTATE_WAITING)
        {
            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            const u8 luIndex = mpInGameMessagesQueue->muCurrentMessageIndex;
            const f32 lfDuration = mpInGameMessagesQueue->maMessages[luIndex].mfDuration;
            const f32 lfTicks =
                static_cast<f32>(static_cast<f64>(CgsSystem::GetSystemTimerFrequency())) * lfDuration;
            mpInGameMessagesQueue->muCurrentEventEndTime =
                static_cast<u64>(CgsSystem::GetSystemTimerBaseTime()) + static_cast<s64>(lfTicks);

            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] =
                E_MESSAGESTATE_VISIBLE;

            SendGameMessage("updateAnim", false);
        }

        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344

        // The GUI-module replay serialiser lives in the access-pointer block (guest +0x14);
        // reached by attested offset (see BrnReplayHudMessageComponent.cpp).
        BrnReplays::GuiModuleSerialiser* lpSerialiser =
            *reinterpret_cast<BrnReplays::GuiModuleSerialiser**>(
                reinterpret_cast<u8*>(lpAccessPointers) + 0x14);
        CGS_ASSERT(lpSerialiser != 0, "mpSerialiser");                  // CgsGuiShared.h:216

        lpSerialiser->GetStaticLayout()->StartMessage();
    }

    // @ 0x824376F0 -- h:248. Retire the director-disallowed messages from each double-buffer
    // slot: if a slot holds a message the director no longer allows, dismiss a non-WAITING one
    // with an "invisible" transition and clear its state to NOMESSAGE. Then, if the live slot
    // is now empty and the other slot is WAITING, flip to it and start its message.
    void InGameMessagesComponent::TerminateMessages()
    {
        CGS_ASSERT(mpDirector != NULL, "mpDirector");

        if (mpInGameMessagesQueue->maeMessageState[0] != E_MESSAGESTATE_NOMESSAGE)
        {
            if (!mpDirector->IsMessageAllowed(mpInGameMessagesQueue->maMessages[0].mHudMessageId))
            {
                if (mpInGameMessagesQueue->maeMessageState[0] != E_MESSAGESTATE_WAITING)
                    SendGameMessage("invisible", false);
                mpInGameMessagesQueue->maeMessageState[0] = E_MESSAGESTATE_NOMESSAGE;
            }
        }

        if (mpInGameMessagesQueue->maeMessageState[1] != E_MESSAGESTATE_NOMESSAGE)
        {
            if (!mpDirector->IsMessageAllowed(mpInGameMessagesQueue->maMessages[1].mHudMessageId))
            {
                if (mpInGameMessagesQueue->maeMessageState[1] != E_MESSAGESTATE_WAITING)
                    SendGameMessage("invisible", false);
                mpInGameMessagesQueue->maeMessageState[1] = E_MESSAGESTATE_NOMESSAGE;
            }
        }

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        if (mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] ==
            E_MESSAGESTATE_NOMESSAGE)
        {
            if (mpInGameMessagesQueue->maeMessageState[GetNextIndex()] == E_MESSAGESTATE_WAITING)
            {
                SwitchCurrentIndex();
                const u8 luIndex = GetCurrentIndex();
                StartMessage(&mpInGameMessagesQueue->maMessages[luIndex]);
            }
        }
    }
}
