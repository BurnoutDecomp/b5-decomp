#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)
#include <stdlib.h>  // [gateui r5] getenv (the BRN_PROP_DIAG probe in RefreshString)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"           // [gateui r3] CgsIDUnCompress (StartMessage's log line)
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // [gateui r3] gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"      // [gateui r3] CgsGui::GuiEvent<N> (the two output records)
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"        // BrnFlapt::FileRef::FindComponent (Prepare)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // BrnFlapt::MovieClipInstance::ResetTimeline
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"     // BrnGui::HudMessageDirector::IsMessageAllowed (TerminateMessages)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers (+0x14 serialiser slot)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface::GetAccessPointers
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"          // BrnReplays::GuiModuleStaticLayout (UpdateInPlace tail)

// Thin wrappers over the platform high-resolution timer (CgsTimeUtils.cpp); declared
// locally per the house style rather than through a header (there is none).
// [gateui r2] The 64-BIT pair. Both X360 bodies (@0x828D75A0 / @0x828D75C8) end
// `ld r3, var_10(r1)` -- a full 8-byte load of the LARGE_INTEGER -- and muCurrentEventEndTime
// is a u64 that is STORED now and COMPARED several seconds later. The host's u32 entry point
// truncates to the QPC low word, which wraps ~every 7 minutes at 10 MHz: a message whose end
// time is computed just before a wrap gets an end stamp far ABOVE the post-wrap `now`, so
// `now >= end` never holds and that HUD message never ends -- it sits on screen until the
// counter catches up. Same defect class as HudMessageDirector's repeat-rate gate; same fix.
namespace CgsSystem { u64 GetSystemTimerBaseTime64(); u64 GetSystemTimerFrequency64(); }

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
namespace
{
    // ===================================================================================
    // [gateui r3] The four .rodata tables SendGameMessage / SetIcon index. NONE of them is
    // in the JSON export set (they are pointer/word tables, not functions); every value
    // below was DUMPED from a private copy of IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with
    // headless idat, per the wave preamble's rodata rule.
    // ===================================================================================

    // off_82F2482C[3] -- the three per-string container clips SendGameMessage hides before
    // it binds their text fields.
    const char* const KAPC_STRING_CLIP_NAMES[3] =
    {
        "String1_mc", "String2_mc", "String3_mc",
    };

    // off_82F24884[3] -- the text field inside each of those clips' PARENT.
    const char* const KAPC_STRING_FIELD_NAMES[3] =
    {
        "String1_txt", "String2_txt", "String3_txt",
    };

    // off_82F2483C[meCurrentGameMode] -- the icon label per game mode. ⚠ THE CONSOLE
    // INDEXES THIS TABLE FROM -1: SetIcon's own guard is
    // `(GsmIO::E_MODE_NONE <= meCurrentGameMode) && (GsmIO::E_MODE_COUNT > meCurrentGameMode)`
    // with E_MODE_NONE == -1, and `off_82F2483C[-1]` is the pointer at 0x82F24838, which the
    // dump shows is "PosMessIcon" (it is also off_82F2482C[3] -- the two tables are
    // contiguous in .rodata, which is exactly why the -1 index is legal on the console and
    // is NOT an out-of-bounds read). Modelled here as a 19-entry table biased by +1 so the
    // same nineteen values are reachable without a negative subscript.
    const char* const KAPC_GAME_MODE_ICONS[19] =
    {
        /* -1 E_MODE_NONE */ "PosMessIcon",
        /*  0 */ "RaceIcon",       /*  1 */ "invisible",     /*  2 */ "PosMessIcon",
        /*  3 */ "RoadRageIcon",   /*  4 */ "invisible",     /*  5 */ "BurningRouteIcon",
        /*  6 */ "invisible",      /*  7 */ "StuntIcon",     /*  8 */ "MarkedManIcon",
        /*  9 */ "invisible",      /* 10 */ "RaceIcon",      /* 11 */ "RoadRageIcon",
        /* 12 */ "StuntIcon",      /* 13 */ "invisible",     /* 14 */ "StuntIcon",
        /* 15 */ "PosMessIcon",    /* 16 */ "PosMessIcon",   /* 17 */ "StuntIcon",
    };
    const s32 KI_GAME_MODE_ICON_BIAS = 1;   // index = meCurrentGameMode + 1

    // dword_8204B83C[CgsGui::HudMessageParamTypes] -- the message-parameter type -> the
    // language manager's ParameterFormatType. Seven entries, one per
    // E_HUDMESSAGEPARAMTYPES_* (UNUSED/STRING/INT/FLOAT/MONEY/TIME/STRINGID).
    const s32 KAI_PARAM_FORMAT_TYPES[7] = { 0, 0, 11, 11, 14, 6, 9 };

    // The localised-string id TYPE the component looks its strings up with (the X360 passes
    // the literal 9 at every SetLocalisedText call site in RefreshString).
    const s32 KI_STRING_ID_TYPE = 9;

    // The state's output-event channel (40 == GuiEventOut; the same constant
    // BrnFBurnMainHudState.cpp names KI_CHANNEL_GUI_OUT).
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // The two bounds SetIcon's game-mode guard tests, spelled as the console does
    // (GsmIO::E_MODE_NONE == -1, GsmIO::E_MODE_COUNT == 18).
    const s32 KI_GAME_MODE_NONE  = -1;
    const s32 KI_GAME_MODE_COUNT = 18;

    // The GUI replay serialiser's static layout, reached the way the console reaches it:
    // StateInterface -> GuiAccessPointers -> mpSerialiser (guest +0x14) -> GetStaticLayout().
    // Both asserts are the console's own (CgsGuiStateInterface.h:344 / CgsGuiShared.h:216)
    // and fire in this order in StartMessage/EndMessage/RefreshString/UpdateInPlace.
    BrnReplays::GuiModuleStaticLayout* GetGuiModuleStaticLayout(
        CgsGui::StateInterface* lpStateInterface)
    {
        // ⚠️ [FLAG PC bring-up, gateui r4 boot fix -- REPLAY MIRROR PARKED, and the +0x14
        // derivation is CONTRADICTED]. The raw guest-offset read that stood here
        // (*(GuiAccessPointers + 0x14) as a GuiModuleSerialiser*) is wrong TWICE on this build:
        // (1) the committed DWARF layout of CgsGui::GuiAccessPointers (CgsGuiShared.h) has NO
        //     serialiser member at all -- guest slot +0x14 is mpGDMInput there; the "mpSerialiser
        //     (guest +0x14)" claim needs re-derivation against the real console block;
        // (2) on the host the pointers widen, so a literal +0x14 lands MID-POINTER -- the first
        //     smash-gate HUD message crashed here (AV on garbage `this`, boot-drive
        //     2026-08-20 17:33, InGameMessagesComponent::StartMessage -> GetStaticLayout+0x6).
        // No GuiModuleSerialiser is constructed on this build anyway (replay recording), so the
        // mirror has no consumer. Return NULL; the call sites skip the mirror when null.
        // DELETE-WHEN the replay GUI serialiser is constructed and reachable through a NAMED
        // access-pointer member.
        (void)lpStateInterface;
        return 0;
    }

    // ---- the two output records ------------------------------------------------------
    // The house "consumers that need the exact X360 wire record build it themselves and post
    // it through GetOutputEventQueue()->AddEvent()" accommodation, documented at
    // CgsGuiStateInterface.h:135 and already used by BrnCarSelectMain_wG_02.cpp and
    // BrnFBurnMainHudState.cpp (GuiCommandEvent16<N> / GuiShowHideEvent24).

    // Construct's own post: {1, 157, 12} + one trailing word, 16 bytes, channel 40.
    struct GuiCommandEvent16_157 : public CgsGui::GuiEvent<157>
    {
        u8 mu8Flag;
        u8 maPad[3];
        GuiCommandEvent16_157() : CgsGui::GuiEvent<157>(1, 12), mu8Flag(0)
        { maPad[0] = maPad[1] = maPad[2] = 0; }
    };

    // StartMessage / EndMessage's post: the 40-byte OutputGuiEvent<BrnGui::GuiAudioEvent>
    // WRAPPER record -- header {sizeof(payload)=24, type=456, payload offset=16} then the
    // 24-byte GuiAudioEvent payload at +0x10. X360 stores, verbatim:
    //   StartMessage @0x824373D8..0x82437424 : payload {1, 0, 0} + maMessages[cur].mHudMessageId
    //   EndMessage   @0x824375F0..0x8243765C : payload {1, 1, 0} + the same id
    // ⚠ FLAG payload field NAMES: BrnGui::GuiAudioEvent is still `u8 maPayload[12]` in
    // BrnGuiDemangledEventTypes.h (fix3gui's lane), so the three leading words have no
    // recovered names -- they are spelled by role here and NOT forked into that header. The
    // decode above is the outstanding shared-header request for that file; it is recorded in
    // scratch/gateui_wave/verify_r3_fix3hud/VERDICT.md (the round-3 banner pointed at a
    // "gateui r3 report" that was never written).
    struct alignas(8) GuiAudioEventRecord40
    {
        s32   miOutEventSize;     // +0x00 = 24  (sizeof the GuiAudioEvent payload)
        s32   miOutEventType;     // +0x04 = 456
        s32   miOutEventOffset;   // +0x08 = 16  (offsetof the payload in this record)
        s32   miHeaderPad;        // +0x0C (the console leaves this word uninitialised)
        s32   miAudioParam0;      // +0x10 = 1 in both callers
        s32   miAudioParam1;      // +0x14 = 0 (start) / 1 (end)
        s32   miAudioParam2;      // +0x18 = 0 in both callers
        s32   miPayloadPad;       // +0x1C (uninitialised on the console)
        CgsID mHudMessageId;      // +0x20

        GuiAudioEventRecord40(s32 liParam1, CgsID lMessageId)
            : miOutEventSize(24), miOutEventType(456), miOutEventOffset(16), miHeaderPad(0)
            , miAudioParam0(1), miAudioParam1(liParam1), miAudioParam2(0), miPayloadPad(0)
            , mHudMessageId(lMessageId)
        {
        }
    };
}

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
    void InGameMessagesComponent::SetController(const BrnResource::HudMessageController* lpController)
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
            if (CgsSystem::GetSystemTimerBaseTime64() >=
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
                static_cast<f32>(static_cast<f64>(CgsSystem::GetSystemTimerFrequency64())) * lfDuration;
            const u64 luEndTime =
                CgsSystem::GetSystemTimerBaseTime64() + static_cast<s64>(lfTicks);
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
            if (mpMessageController
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
                static_cast<f32>(static_cast<f64>(CgsSystem::GetSystemTimerFrequency64())) * lfDuration;
            mpInGameMessagesQueue->muCurrentEventEndTime =
                CgsSystem::GetSystemTimerBaseTime64() + static_cast<s64>(lfTicks);

            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] =
                E_MESSAGESTATE_VISIBLE;

            SendGameMessage("updateAnim", false);
        }

        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344

        // Replay-mirror leg: parked, see the FLAG in GetGuiModuleStaticLayout (the raw
        // guest +0x14 serialiser read was a live host corruption).
        if (BrnReplays::GuiModuleStaticLayout* lpStaticLayout = GetGuiModuleStaticLayout(mpStateInterface))
            lpStaticLayout->StartMessage();
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

    // ====================================================================================
    // [gateui r3] The five bodies this TU was missing. Until they landed the whole TU was
    // unmountable (five LNK2019s reachable from Update / AddMessage / TerminateMessages),
    // which is why BrnFBurnMainHudState::OnEnter could only log "InGameMessagesComponent --
    // component TU deferred" and its RecvEvent case 154 was an empty arm.
    //
    // The four .rodata tables these bodies index were dumped from a private copy of the
    // ARTIST .i64 with headless idat (they are not in the JSON export set) -- see the table
    // block at the top of this file.
    // ====================================================================================

    // @ 0x82437050 -- h:252. Store map, in the console's order:
    //   +0x00  mpStateInterface = lpStateInterface   (assert BrnGuiFlaptComponent.h:113)
    //   +0x04  mAptRef          = invalid            (a1[1] = a1[2] = 0)
    //          post GuiEvent<157> {1,157,12} on the state's output queue, channel 40, 16 B
    //   +0x0C  mpInGameMessagesQueue = 0
    //   +0x440 mpMessageController   = 0
    //   +0x444 mpDirector            = 0
    //   the 3-iteration loop: maiCurrentStringParamCount[i] = 0;
    //          memset(maacCurrentStringStringId[i], 0, 64)
    //   +0x448 meCurrentGameMode = -1  (E_MODE_NONE)
    //   +0x10..+0x43 mAnimationRef / mIconRef / maTextFields[3] zeroed (a1[4..16])
    // What is NOT cleared: maaeCurrentStringParamTypes and maaacCurrentStringParams.
    // Reproduced -- SendGameMessage rewrites both before RefreshString reads them.
    void InGameMessagesComponent::Construct(const char* /*lacName*/,
                                            CgsGui::StateInterface* lpStateInterface,
                                            const char* /*lacParentName*/)
    {
        // The X360 opens with the shared base's tripwire, the interface store and the clip
        // invalidate -- i.e. BrnFlaptComponent::Construct, inlined.
        BrnFlaptComponent::Construct(lpStateInterface);

        {
            GuiCommandEvent16_157 lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 16);
        }

        mpInGameMessagesQueue = NULL;
        mpMessageController   = NULL;
        mpDirector            = NULL;

        for (s32 liIter = 0; liIter < 3; ++liIter)
        {
            maiCurrentStringParamCount[liIter] = 0;
            std::memset(maacCurrentStringStringId[liIter], 0,
                        sizeof(maacCurrentStringStringId[liIter]));
        }

        meCurrentGameMode =
            static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(-1);   // E_MODE_NONE

        mAnimationRef.SetInvalid();
        mIconRef.SetInvalid();
        for (s32 liIter = 0; liIter < 3; ++liIter)
            maTextFields[liIter].SetInvalid();
    }

    // @ 0x82410E30 -- h:292. Drive the icon clip to the named label. The literal
    // "EventSpecific" means "use the icon for the live game mode" -- see the -1 bias note on
    // KAPC_GAME_MODE_ICONS.
    void InGameMessagesComponent::SetIcon(const char* lacIconTypeName)
    {
        CGS_ASSERT(lacIconTypeName != NULL, "NULL != lacIconTypeName");   // cpp:438

        if (std::strcmp(lacIconTypeName, "EventSpecific") != 0)
        {
            mIconRef.GotoAndPlayLabel(lacIconTypeName);
            return;
        }

        CGS_ASSERT(static_cast<s32>(meCurrentGameMode) >= KI_GAME_MODE_NONE &&
                       static_cast<s32>(meCurrentGameMode) < KI_GAME_MODE_COUNT,
                   "(GsmIO::E_MODE_NONE <= meCurrentGameMode) && "
                   "(GsmIO::E_MODE_COUNT > meCurrentGameMode)");          // cpp:444

        mIconRef.GotoAndPlayLabel(
            KAPC_GAME_MODE_ICONS[static_cast<s32>(meCurrentGameMode) + KI_GAME_MODE_ICON_BIAS]);
    }

    // @ 0x82411210 -- h:273. Push the liIndex'th cached string into its bound text field: the
    // parameterised overload when the string takes parameters, the plain id lookup when it
    // does not. Nothing happens at all when that field was never bound (the whole body sits
    // inside `if (maTextFields[liIndex] is valid)`).
    void InGameMessagesComponent::RefreshString(s32 liIndex)
    {
        // [DIAG] NOT IN THE X360 BINARY. [gateui r5] The writer/reader agreement probe for the
        // HUD-message parameter path -- this is the hop the r4 build died on (the format-11
        // integer leaf was a __debugbreak trap). Prints the exact tuple the language manager is
        // about to be handed: which display string, its localised-string id, how many
        // parameters, and each parameter's TEXT plus the ParameterFormatType RefreshString
        // mapped it to. A wrong slot/stride in the GuiHudMessage writer shows up here as an
        // empty or garbage parameter text; a wrong type mapping shows up as an unexpected fmt.
        // Same knob as the rest of the ladder (BRN_PROP_DIAG), first 6 refreshes only.
        {
            static const bool sbUiGateDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siDiagPrinted = 0;
            if ( sbUiGateDiag && siDiagPrinted < 6 && CgsDev::Log::gpDebugPrint != 0 )
            {
                ++siDiagPrinted;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] hud refresh str=" << liIndex
                    << " bound=" << ( maTextFields[liIndex].mpTextFieldInstance != NULL ? 1 : 0 )
                    << " id=\"" << maacCurrentStringStringId[liIndex] << "\""
                    << " params=" << maiCurrentStringParamCount[liIndex];
                for (s32 liDiagParam = 0;
                     liDiagParam < maiCurrentStringParamCount[liIndex] &&
                         liDiagParam < BrnResource::HudMessageEvent::KI_MAX_PARAMS_PER_STRING;
                     ++liDiagParam)
                {
                    *CgsDev::Log::gpDebugPrint
                        << " p" << liDiagParam
                        << "=\"" << maaacCurrentStringParams[liIndex][liDiagParam] << "\""
                        << "/fmt=" << maaeCurrentStringParamTypes[liIndex][liDiagParam];
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }
        }

        if (maTextFields[liIndex].mpTextFieldInstance == NULL)
            return;

        const s32 liNumParams = maiCurrentStringParamCount[liIndex];
        if (liNumParams != 0)
        {
            // The X360 builds the pointer array on its own stack, one entry per parameter,
            // straight off maaacCurrentStringParams[liIndex][liParam].
            const char* lapcParams[BrnResource::HudMessageEvent::KI_MAX_PARAMS_PER_STRING];
            for (s32 liParam = 0; liParam < liNumParams; ++liParam)
                lapcParams[liParam] = maaacCurrentStringParams[liIndex][liParam];

            maTextFields[liIndex].SetLocalisedText(maacCurrentStringStringId[liIndex],
                                                   KI_STRING_ID_TYPE, liNumParams,
                                                   lapcParams,
                                                   maaeCurrentStringParamTypes[liIndex]);
        }
        else
        {
            maTextFields[liIndex].SetLocalisedText(maacCurrentStringStringId[liIndex],
                                                   KI_STRING_ID_TYPE);
        }

        // FLAG deferred (ONE leg, replay-recording only): the console then mirrors the
        // field's rendered text into the GUI replay static layout --
        //   memcpy(&GetStaticLayout()->maMessageSlots[liIndex], GetText(), 128)
        // (@0x82411300..0x82411340; the 128-byte stride the GuiModuleStaticLayout header
        // already models). BrnFlapt::TextFieldRef::GetText @0x8246E0C0 IS in the X360 ledger
        // but is NOT DECLARED in BrnFlaptTextFieldRef.h -- an outstanding shared-header
        // request, recorded in scratch/gateui_wave/verify_r3_fix3hud/VERDICT.md (the round-3
        // banner pointed at a "gateui r3 report" that was never written). This leg feeds
        // replay recording only; the on-screen text set above is the load-bearing half.
    }

    // @ 0x8241F248 -- h:289. Re-bind the whole message banner against the live slot and run
    // the named transition: drive the outer clip to the message's own style label, resolve
    // the SpecificMessage_mc animation clip and play lpcAnimName on it, resolve its Icon_mc,
    // bind the (up to) three string containers + their PARENT's text fields, set the icon,
    // then copy the message's three strings and their parameters into the component's cache
    // and refresh each one.
    // WARNING lbNewIcon is DEAD in the ARTIST build: its parameter register is never read
    // (SetIcon runs unconditionally). Kept because the DWARF declares it (h:289) and both
    // callers pass it; reproduced as written rather than "cleaned up".
    void InGameMessagesComponent::SendGameMessage(const char* lpcAnimName, bool /*lbNewIcon*/)
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");   // h:397

        BrnResource::HudMessageEvent* lpMessage =
            &mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex];

        mAptRef.GotoAndPlayLabel(lpMessage->macMessageStyle);

        mAptRef.FindChildMovieClipOnFrame(&mAnimationRef, "SpecificMessage_mc");
        mAnimationRef.GotoAndPlayLabel(lpcAnimName);
        mAnimationRef.FindChildMovieClip(&mIconRef, "Icon_mc");

        s32 liIter = 0;
        BrnFlapt::MovieClipRef lFound;
        while (liIter < 3 &&
               mAnimationRef.TryFindChildComponentRecursively(KAPC_STRING_CLIP_NAMES[liIter],
                                                              &lFound))
        {
            lFound.SetVisible(false);

            BrnFlapt::MovieClipRef lParent;
            lFound.GetParent(&lParent);
            lParent.FindChildTextField(&maTextFields[liIter], KAPC_STRING_FIELD_NAMES[liIter]);

            if (liIter == 0)
                maTextFields[0].SetAutoSize(true);

            ++liIter;
        }
        // Every container the message does not use leaves its field unbound.
        for (; liIter < 3; ++liIter)
            maTextFields[liIter].SetInvalid();

        SetIcon(lpMessage->macIcon);

        for (s32 liString = 0; liString < 3; ++liString)
        {
            maiCurrentStringParamCount[liString] = lpMessage->maiStringParamCount[liString];

            CGS_ASSERT(maiCurrentStringParamCount[liString] >= 0,
                       "maiCurrentStringParamCount[liIter] >= 0");                    // cpp:403
            CGS_ASSERT(maiCurrentStringParamCount[liString] <=
                           BrnResource::HudMessageEvent::KI_MAX_PARAMS_PER_STRING,
                       "maiCurrentStringParamCount[liIter] <= "
                       "BrnResource::HudMessageEvent::KI_MAX_PARAMS_PER_STRING");     // cpp:404

            std::memcpy(maacCurrentStringStringId[liString], lpMessage->maacString[liString],
                        sizeof(maacCurrentStringStringId[liString]));

            for (s32 liParam = 0; liParam < maiCurrentStringParamCount[liString]; ++liParam)
            {
                const CgsGui::HudMessageParameter& lrParam =
                    lpMessage->maacStringParam[liString][liParam];
                std::memcpy(maaacCurrentStringParams[liString][liParam], lrParam.macParameter,
                            sizeof(maaacCurrentStringParams[liString][liParam]));
                maaeCurrentStringParamTypes[liString][liParam] =
                    KAI_PARAM_FORMAT_TYPES[static_cast<s32>(lrParam.meParamType)];
            }

            RefreshString(liString);
        }
    }

    // @ 0x82437150 -- h:286. Begin transitioning lpEvent IN on the live slot: validate the
    // slot state, log the message name, copy the event into the slot (unless it already IS
    // the slot), mark it TRANSIN, post the audio record, raise the replay static-layout
    // message flags and run the "transin" animation.
    void InGameMessagesComponent::StartMessage(BrnResource::HudMessageEvent* lpEvent)
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");   // h:397

        if (mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] !=
            E_MESSAGESTATE_NOMESSAGE)
        {
            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            // NOTE the asymmetry, and that it is the console's: the INCOMING event's
            // miForceRemoveThreshold (+0x0C) is compared against the RESIDENT message's
            // miPriority (+0x08), not against its own priority.
            CGS_ASSERT(mpInGameMessagesQueue
                               ->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] ==
                               E_MESSAGESTATE_WAITING ||
                           lpEvent->miForceRemoveThreshold >=
                               mpInGameMessagesQueue->maMessages[GetCurrentIndex()].miPriority,
                       "Invalid state to start a message in");                        // cpp:290
        }

        char lacMessageName[24];
        CgsIDUnCompress(lpEvent->mHudMessageId, lacMessageName);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "\nHUD MSGS : STARTING MESSAGE NAMED \""
                                       << lacMessageName << "\"\n\n";
        }

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        if (&mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex] !=
            lpEvent)
        {
            CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
            std::memcpy(
                &mpInGameMessagesQueue->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex],
                lpEvent, sizeof(BrnResource::HudMessageEvent));
        }

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] =
            E_MESSAGESTATE_TRANSIN;

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        {
            GuiAudioEventRecord40 lAudio(
                0,
                mpInGameMessagesQueue
                    ->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex].mHudMessageId);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT, 40);
        }

        if (BrnReplays::GuiModuleStaticLayout* lpStaticLayout = GetGuiModuleStaticLayout(mpStateInterface))
            lpStaticLayout->StartMessage();   // replay mirror parked -- FLAG at the helper

        SendGameMessage("transin", true);
    }

    // @ 0x824374B8 -- h:298. End the visible message: mark the slot TRANSOUT, post the audio
    // record (its param1 == 1 is the ONLY byte that differs from StartMessage's), run the
    // "transout" animation, then drop the replay static-layout message flags.
    void InGameMessagesComponent::EndMessage()
    {
        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");   // h:397
        CGS_ASSERT(
            mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] ==
                E_MESSAGESTATE_VISIBLE,
            "In Game Messages can only be removed when in the visible state.\n");     // cpp:521

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        mpInGameMessagesQueue->maeMessageState[mpInGameMessagesQueue->muCurrentMessageIndex] =
            E_MESSAGESTATE_TRANSOUT;

        CGS_ASSERT(mpInGameMessagesQueue != NULL, "mpInGameMessagesQueue != NULL");
        {
            GuiAudioEventRecord40 lAudio(
                1,
                mpInGameMessagesQueue
                    ->maMessages[mpInGameMessagesQueue->muCurrentMessageIndex].mHudMessageId);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT, 40);
        }

        SendGameMessage("transout", false);

        if (BrnReplays::GuiModuleStaticLayout* lpStaticLayout = GetGuiModuleStaticLayout(mpStateInterface))
            lpStaticLayout->EndMessage();     // replay mirror parked -- FLAG at the helper
    }
}
