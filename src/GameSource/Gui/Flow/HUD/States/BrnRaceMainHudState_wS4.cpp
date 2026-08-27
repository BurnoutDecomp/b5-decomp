// ===================================================================================
// wave-S4 partfile of BrnRaceMainHudState.cpp -- the five RACE_MAIN helper bodies the
// mounted sibling part-files CALL but that were bodied NOWHERE in the tree (the RMH
// verifier's link list). Every one is declared in BrnRaceMainHudState.h:200/201/205/210/211.
//
//   BrnGui::RaceMainHudState::ProcessBoostInfo                      @0x82474550 (DWARF .cpp:2937)
//   BrnGui::RaceMainHudState::ProcessAptEvents                      @0x82474638 (DWARF .cpp:2964)
//   BrnGui::RaceMainHudState::UpdateSatNav                          @0x82474830 (DWARF .cpp:3261)
//   BrnGui::RaceMainHudState::StartFreeburnChallengeTicker          @0x8247A9C0 (DWARF .cpp:4085)
//   BrnGui::RaceMainHudState::StartFreeburnChallengeNotActiveTicker @0x8247AF38 (DWARF .cpp:4185)
//
// Each JSON's `name` field was checked against the symbol claimed above before a line was
// written, and every body is transcribed from the raw DISASSEMBLY -- Hex-Rays is arbitrated
// against wherever the two disagree. It disagrees three times here and each is called out
// at its site:
//   (1) UpdateSatNav / ProcessAptEvents: Hex-Rays renders the SatNavComponent receiver as
//       `v3 + 424` in one function and `v4 + 1696` in the other for the SAME member. The asm
//       is `addi r3, r<this>, 0x6A0` in BOTH -- 424 is the dropped `(_DWORD *)` cast. The
//       member is mSatNavComponent (header PINNED +0x6A0) either way.
//   (2) ProcessAptEvents' mbEventInfo arm calls a function IDA names
//       `CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct`. That address
//       (0x8284CB38) is a bare `blr` with hundreds of xrefs -- the linker's ICF pool for
//       every empty method in the image. The RECEIVER is `this + 0x170` == mEventInfoComponent
//       and the asm sets THREE argument registers (r3 = &mEventInfoComponent, r4 = lpEvent,
//       r5 = mpCache @0x824747C4), so the call is EventInfoComponent::HandleTrigger
//       (DWARF BrnEventInfo.h:429), folded to empty in retail. See its site.
//   (3) The two tickers: Hex-Rays renders the four identical post blocks as straight-line
//       code and hides the `for` they came from; the asm is four byte-identical
//       memcpy+AddEvent groups (0x8247AE58..0x8247AF2C / 0x8247B004..0x8247B0D8).
//
// THE TICKER WIRE. Both tickers post through OutputGuiEvent<T>, whose X360 body stack-builds
// a GuiEventWrapper<T,40> -- { sizeof(T), T's id, 12 } then a byte copy of T -- and queues
// THAT on channel 40. The in-tree StateInterface::OutputGuiEvent template still direct-passes
// (the divergence FLAGged at CgsGuiStateInterface.h:131), so both records are built here and
// posted through GetOutputEventQueue()->AddEvent at their true wire size. That is the
// standing accommodation for this family and it is what the two closest siblings already do:
// BrnRaceMainHudState.cpp:113 (its OnLeave GuiEvent536) and BrnJunctionInfoComponent.cpp:41
// (the 2072-byte id-537 custom-message payload, whose layout attestation -- AddString
// @0x823A6940, types stride 4 @+0, strings stride 512 @+0x10, count @+0x810 -- this file
// reuses verbatim).
// ⚠ NOTE for the conductor: the sibling wS2.cpp:630 / wS3.cpp:658 ticker-CLEAR arms post the
// same id-536 record through `mpStateInterface->OutputGuiEvent(lClear)` instead, which lands
// it on channel 536 at 2 bytes rather than channel 40 at 16. Not edited here (not this file's
// partfile) -- reported instead.
//
// COMPONENT DEFERRALS. Same rule and same one-shot helper as the sibling
// BrnRaceMainHudState_wS3.cpp:183 / BrnFBurnMainHudState.cpp:216: a call whose callee is not
// reachable from the build keeps the console's gate and control flow verbatim and logs the
// gap once instead of inventing a body. FOUR here, each named at its site:
//   * EventInfoComponent::HandleTrigger -- undeclared on the component, and ICF-folded EMPTY
//     in retail, so the deferral costs no behaviour at all.
//   * ChallengeSelector::HandleLoadNotification and
//     PaybackComponent::RespondToTransitionComplete -- both TUs are on disk but NOT on the
//     build (only their BrnHudStatesLinkStubs.cpp Construct scaffolds are), and wS3 already
//     defers every arm of both for exactly this reason. Deferring them TU-wide is what keeps
//     the RACE_MAIN mount linkable.
//   * the local player's completed-challenge bit -- the GUI FreeburnChallengeManager's
//     mCompletedData tail is deliberately unmodelled (BrnGuiFreeburnChallengeManager.h:148,
//     "HONEST BOUNDARY").
//
// ⚠ TWO LINK RESIDUALS THIS FILE ADDS (reported, not papered over -- both are real data
// accessors whose values would be visibly wrong if stood in):
//   * BrnResource::ChallengeListEntry::GetDescriptionStringID() const -- declared-only, NO
//     body anywhere. BrnHudStatesLinkStubs.cpp:107 already names it as the ChallengeSelector
//     mount's residual, and ChallengeListEntry.h:427 documents the fix: it is the identical
//     shape to the already-inline GetTitleStringID, over macDescriptionStringID (+0xA0).
//   * BrnResource::ChallengeListEntryAction::GetTargetValue(s32) const -- FULLY bodied at
//     SharedClasses/DataLists/ChallengeListEntry.cpp:71; that TU is simply not in
//     tools/build/build_game_exe.bat yet.
// (A third, BrnResource::ChallengeListEntry::GetNumPlayers(), links TODAY only to the
// BrnFriendsListLinkGates.cpp:116 gate, which returns 0 and logs -- so the ticker's player
// -count parameter renders "0" until the DataLists body lands. Not this file's gate.)
// ===================================================================================

#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (deferral gap log)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface + the out-queue
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload (event 21, typed)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // BrnGui::FreeburnChallengeManager
#include "SharedClasses/DataLists/ChallengeList.h"                        // BrnResource::ChallengeList
#include "SharedClasses/DataLists/ChallengeListEntry.h"                   // BrnResource::ChallengeListEntry(Action)

#include <cstdio>    // std::snprintf (the one-shot deferral log)
#include <cstring>   // std::strstr / std::strcmp / std::strncpy / std::memset

namespace BrnGui
{
    namespace
    {
        // The GUI output channels (the console spells this one `li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_OUT = 40;   // 0x28 -- GuiEventOut

        // ---- the id-536 ticker-clear wire record ------------------------------------
        // { 2, 536, 12 } + the {0,1} byte pair, 16 bytes, channel 40. Identical shape to
        // the sibling BrnRaceMainHudState.cpp:113 (OnLeave posts the same record); kept
        // file-local per partfile, exactly as the GuiCommandEvent16 twins are.
        // Both ticker bodies open with it: @0x8247AA18..0x8247AA44 (active) and
        // @0x8247AF90..0x8247AFBC (not-active) build the pair as two `stb`s into a scratch
        // half-word and store it with one `sth`, so the payload is a {0, 1} BYTE PAIR at
        // +0x0C, not a 16-bit 256. Named after the DWARF's own two fields
        // (BrnGuiEventTypeDefs.h:301/:302 GuiEventTickerClearMessages).
        struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbForceFadeOut;            // +0x0C == 0
            u8 mbDeleteChallengeMessages; // +0x0D == 1
            u8 mau8Pad[2];
            GuiTickerClearWire536()
                : CgsGui::GuiEvent<536>(2, 12), mbForceFadeOut(0), mbDeleteChallengeMessages(1)
            { mau8Pad[0] = mau8Pad[1] = 0; }
        };

        void PostTickerClear536(CgsGui::StateInterface* lpInterface)
        {
            GuiTickerClearWire536 lEvent;
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 16);
        }

        // ---- the id-537 custom ticker message ---------------------------------------
        // The 0x818-byte payload + its GuiEvent<537> wire header. TU-local model of
        // BrnGui::GuiEventTickerCustomMessage's on-queue record; the canonical
        // BrnGuiDemangledEventTypes.h:256 entry is an opaque 12B-header shape that does NOT
        // match the wire. Layout attested by BrnJunctionInfoComponent.cpp:41 (from AddString
        // @0x823A6940) and corroborated here by BOTH ticker bodies: the console's Construct
        // is inlined as `std 0 ; std 0` over +0x000..+0x00F (the four string types),
        // `memset(base + 0x10, 0, 0x800)` (the four 512-byte strings) and five tail `stb`s at
        // +0x810..+0x814 (the count then the four flags), total 0x818.
        //
        // ⚠ FLAG DWARF-vs-RETAIL (capacities): the DecFIGS DWARF (BrnGuiEventTypeDefs.h:310/
        // :311) says KI_MAX_NUM_STRINGS 4 / KI_MAX_CUSTOMMESSAGE_LENGTH 256 and declares the
        // strings BEFORE the types. Retail X360 is 4 x 512 with the TYPES FIRST -- 4*512 +
        // 4*4 == 0x810, which is exactly where the console's count byte lands. The retail
        // shape is used. DELETE-WHEN: never (this IS the shipped record).
        // The four tail flags carry the DWARF names (BrnGuiEventTypeDefs.h:358..:362) in
        // DWARF declaration order; only their VALUES are X360-attested here.
        struct GuiTickerCustomMessagePayload537
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;
            static const s32 KI_MAX_STRING_LENGTH = 512;

            // -- BrnGuiEventTypeDefs.h:313 (DWARF) --
            enum EStringType
            {
                E_STRINGTYPE_NONE     = 0,
                E_STRINGTYPE_TEXT     = 1,
                E_STRINGTYPE_STRINGID = 2,
                E_STRINGTYPE_NUM      = 3,
            };

            s32  maeStringTypes[KI_MAX_NUM_STRINGS];                     // +0x000
            char maacMessageStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH]; // +0x010
            s8   mi8NumStrings;                                          // +0x810
            u8   mbLoopMessage;                                          // +0x811
            u8   mbTrainingMessage;                                      // +0x812
            u8   mbAllowDuplicates;                                      // +0x813
            u8   mbIsChallengeMessage;                                   // +0x814
            u8   mau8Pad815[3];                                          // +0x815

            // BrnGuiEventTypeDefs.h:327 (DWARF Construct(bool,bool,bool,bool)) -- inlined at
            // both ticker call sites as the zero-seed plus the five tail stores.
            void Construct(bool lbLoop, bool lbTraining, bool lbAllowDuplicates,
                           bool lbIsChallengeMessage)
            {
                std::memset(maeStringTypes, 0, sizeof(maeStringTypes));
                std::memset(maacMessageStrings, 0, sizeof(maacMessageStrings));
                mi8NumStrings        = 0;
                mbLoopMessage        = static_cast<u8>(lbLoop ? 1 : 0);
                mbTrainingMessage    = static_cast<u8>(lbTraining ? 1 : 0);
                mbAllowDuplicates    = static_cast<u8>(lbAllowDuplicates ? 1 : 0);
                mbIsChallengeMessage = static_cast<u8>(lbIsChallengeMessage ? 1 : 0);
                mau8Pad815[0] = mau8Pad815[1] = mau8Pad815[2] = 0;
            }

            // X360 0x823A6940, transcribed (the console's own bounds asserts, then the
            // 512-byte strncpy + type store + count bump). Same body as the committed
            // BrnJunctionInfoComponent.cpp:54 model.
            void AddString(const char* lpString, EStringType leType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392
                std::strncpy(maacMessageStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maeStringTypes[mi8NumStrings] = static_cast<s32>(leType);
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes on the wire.
        struct GuiTickerCustomMessageWire537 : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload537 mMessage;   // +0x0C
            GuiTickerCustomMessageWire537()
                : CgsGui::GuiEvent<537>(
                      static_cast<u32>(sizeof(GuiTickerCustomMessagePayload537)), 12)
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
            }
        };

        // Both tickers queue the SAME finished record FOUR times (four byte-identical
        // memcpy+AddEvent groups; the console unrolled the loop). Not a Hex-Rays artefact --
        // the asm carries all four, 0x8247AE58..0x8247AF2C and 0x8247B004..0x8247B0D8.
        const s32 KI_TICKER_MESSAGE_POST_COUNT = 4;

        void PostTickerCustomMessage537(CgsGui::StateInterface* lpInterface,
                                        const GuiTickerCustomMessagePayload537& lMessage)
        {
            for (s32 li = 0; li < KI_TICKER_MESSAGE_POST_COUNT; ++li)
            {
                GuiTickerCustomMessageWire537 lWire;
                lWire.mMessage = lMessage;   // the console's `memcpy(dst, src, 0x818)`
                lpInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lWire)));
            }
        }

        // ---- the active ticker's positional-parameter slots -------------------------
        // StartFreeburnChallengeTicker's stack frame carries FOUR (text, format) pairs and
        // hands all four to FormatAndAddText, though liNumParams is only ever 1..3 (one for
        // the challenge's player count plus one per action that has targets, and
        // KI_MAX_ACTIONS_PER_CHALLENGE == 2). The console leaves slot 3 holding whatever the
        // frame last had there -- it aliases the dead id-536 record's payload half-word at
        // sp+0xAC. FLAG PC defensive: the slots are zero-seeded here (FormatTextV never reads
        // past liNumParams, so no queued byte changes); an uninitialised vararg read would be
        // UB on the host. DELETE-WHEN: never.
        const s32 KI_TICKER_MAX_PARAMS    = 4;
        const u32 KU_TICKER_PARAM_TEXT_LEN = 64;   // `li r4, 0x40` into both SnPrintf calls

        // One-shot deferral log: the un-homed callees this file drives. Each call site keeps
        // the console's gate and routing; only the body is deferred, and the gap stays
        // visible in the log instead of silently vanishing. (Same helper, same ONE-SHOT
        // 16-slot shape, as BrnRaceMainHudState_wS3.cpp:183.)
        void LogDeferredComponent(const char* lpacComponent)
        {
            static const char* sapcNames[16];
            for (s32 li = 0; li < 16; ++li)
            {
                if (sapcNames[li] == lpacComponent)
                    return;
                if (sapcNames[li] == 0)
                {
                    sapcNames[li] = lpacComponent;
                    char lac[160];
                    std::snprintf(lac, sizeof(lac),
                                  "[RaceMainHud] %s -- component TU deferred (wave S4).\n",
                                  lpacComponent);
                    CgsDev::Log::WriteToLog(lac);
                    return;
                }
            }
        }
    }

    // =======================================================================
    //  ProcessBoostInfo  @ 0x82474550
    // =======================================================================
    // UpdateRunning's case-206 arm (BrnRaceMainHudState_wS3.cpp:396): hand the boost-type
    // record to the boost-message manager under its own id, which is the latch that tints
    // every message the manager subsequently posts. The mpCache assert is NON-GATING on
    // console (@0x824745FC the store falls through to the call either way).
    void RaceMainHudState::ProcessBoostInfo(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event");                          // cpp:2902

        if (mbBoostMessages)                                                // lbz 0x154
        {
            CGS_ASSERT(mpCache != 0, "mpCache != NULL");                    // cpp:2910
            // @0x8247461C: `li r5, 0xCE` == 206, `addi r3, r27, 0x1078` == &mBoostMessageManager.
            mBoostMessageManager.RecvEvent(lpEvent, 206, mpCache);
        }
    }

    // =======================================================================
    //  ProcessAptEvents  @ 0x82474638
    // =======================================================================
    // UpdatePermenant's case-21 arm (BrnRaceMainHudState_wS2.cpp:531): the apt trigger fan-out.
    // Two typed arms (ONLOAD == 1, TRANSITION_COMPLETE == 4) and then an UNCONDITIONAL tail
    // that runs for EVERY apt event type -- including a SECOND mSatNavComponent.RecvEvent(21)
    // for the type-1 case. That double post is not a transcription slip: the asm issues the
    // identical three-argument call twice, at 0x8247474C (inside the type-1 arm) and again at
    // 0x824747B4 (the tail). Preserved verbatim.
    void RaceMainHudState::ProcessAptEvents(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event passed to RaceMainHudState::ProcessAptEvents");   // cpp:2930

        // The event-21 record on this host IS the native-width GuiEventAptTriggerPayload
        // (CgsAptCommunicator.h) -- the same typed read the sibling FBurn state's
        // ProcessAptEvents already does. The console reads the clip name as "payload word 2"
        // because that is where the 32-bit record's pointer lands; by-name is both the house
        // rule and the x64 fix.
        const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
            reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
        const char* lpacClipName = lpTrigger->mpacComponentName;

        if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            if (mbSatNav)                                                   // lbz 0x150
                mSatNavComponent.RecvEvent(lpEvent, 21);                    // addi r3, +0x6A0

            // @0x82474750: strstr(Str = the clip name, SubStr = "PositionIndicator_mc") --
            // a SUBSTRING test, not a compare, because the apt name arrives parent-qualified.
            if (std::strstr(lpacClipName, macPositionIndicatorName) != 0)
                mPositionIndicatorComponent.SetLoaded();                    // addi r3, +0x1224

            if (mbFreeburnChallengeSelector)                                // lbz 0x166
            {
                // @0x8247477C: `addi r4, r28, 0x67A4` == mChallengeSelectorComponent + 4 ==
                // the GuiComponent base's macName. The component matches on its OWN resolved
                // name, so the by-name spelling is GetName().
                if (std::strstr(lpacClipName,
                                mChallengeSelectorComponent.GetName()) != 0)
                {
                    // FLAG deferred: ChallengeSelector's TU (BrnChallengeSelector.cpp +
                    // BrnChallengeSelector_wL_01.cpp) is NOT on the build -- only the
                    // BrnHudStatesLinkStubs.cpp Construct scaffold is -- so every out-of-line
                    // method of it is deferred TU-wide by this wave; the sibling
                    // BrnRaceMainHudState_wS3.cpp:639/:648 defers Show/Hide for the same
                    // reason. The gate and the name match above are the console's, verbatim.
                    // DELETE-WHEN: the ChallengeSelector pair mounts (and its scaffold dies);
                    // the line then becomes
                    // `mChallengeSelectorComponent.HandleLoadNotification(lpacClipName);`.
                    LogDeferredComponent("ChallengeSelector::HandleLoadNotification");
                }
            }
        }
        else if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE
                 && mbPaybackComponent)                                     // lbz 0x15F
        {
            // @0x824746F0..0x82474724 is an INLINED strcmp against "Payback_mc" (the
            // byte-at-a-time subtract loop Hex-Rays renders open-coded), not a strstr like
            // the two above -- restored as the call it came from, against the class static
            // macPaybackName rather than a second copy of the literal.
            if (std::strcmp(lpacClipName, macPaybackName) == 0)
            {
                // @0x82474728 `addi r3, r28, 0x920` == &mPaybackComponent.
                // FLAG deferred: BrnPaybackComponent.cpp is NOT on the build (its TU still
                // owes SendAwardTriggerableEvent -- BrnHudStatesLinkStubs.cpp:118 -- and only
                // its Construct scaffold is mounted), and the sibling
                // BrnRaceMainHudState_wS3.cpp defers every PaybackComponent arm for the same
                // reason. UpdateSetupState clears mbPaybackComponent on the stunt-race path,
                // so this arm does not execute on the bring-up route either way.
                // DELETE-WHEN: BrnPaybackComponent.cpp mounts; the line then becomes
                // `mPaybackComponent.RespondToTransitionComplete();`.
                LogDeferredComponent("PaybackComponent::RespondToTransitionComplete");
            }
        }

        // ---- the unconditional tail (@0x8247479C..0x82474820) --------------------------
        if (mbSatNav)                                                       // lbz 0x150
            mSatNavComponent.RecvEvent(lpEvent, 21);

        if (mbEventInfo && mpCache != 0)                                    // lbz 0x157 / lwz 0x140
        {
            // @0x824747D0: r3 = &mEventInfoComponent (this+0x170), r4 = lpEvent,
            // r5 = mpCache -- i.e. EventInfoComponent::HandleTrigger(const GuiEventAptTrigger*,
            // GuiCache*) (DWARF BrnEventInfo.h:429). The branch target IDA labels
            // `BaseCollisionGenerator::Destruct` is 0x8284CB38, a bare `blr`: the image's ICF
            // pool for every empty method, so this call does NOTHING in retail.
            // FLAG deferred: HandleTrigger is not declared on BrnGui::EventInfoComponent in
            // the tree (and its GuiEventAptTrigger parameter type has no committed home), so
            // the gate is kept and the call is logged rather than invented. Behaviourally
            // free -- the console body is empty.
            // DELETE-WHEN: BrnEventInfo.h declares HandleTrigger; the line then becomes
            // `mEventInfoComponent.HandleTrigger(lpTrigger, mpCache);`.
            LogDeferredComponent("EventInfoComponent::HandleTrigger");
        }

        if (mbBoostMessages)                                                // lbz 0x154
        {
            CGS_ASSERT(mpCache != 0, "mpCache != NULL");                    // cpp:3134
            // @0x82474810: `li r5, 0x15` == 21. Id 21 is the apt-trigger record the manager's
            // switch deliberately ignores (its jump table starts at 206) -- the call is made
            // and falls through to default, exactly as shipped.
            mBoostMessageManager.RecvEvent(lpEvent, 21, mpCache);
        }
    }

    // =======================================================================
    //  UpdateSatNav  @ 0x82474830
    // =======================================================================
    // UpdateRunning's shared cases 199/200 arm (BrnRaceMainHudState_wS3.cpp:380): forward the
    // record to the sat-nav component under the ORIGINAL event id, so one body serves both.
    void RaceMainHudState::UpdateSatNav(const CgsModule::Event* lpEvent, s32 liEventId)
    {
        CGS_ASSERT(lpEvent != 0, " invalid event passed ");                 // cpp:3226
        // (the assert string's leading and trailing spaces are the console's, verbatim)

        if (mbSatNav)                                                       // lbz 0x150
            mSatNavComponent.RecvEvent(lpEvent, liEventId);                 // addi r3, +0x6A0
    }

    // =======================================================================
    //  StartFreeburnChallengeTicker  @ 0x8247A9C0
    // =======================================================================
    // Publish the ACTIVE freeburn challenge as a scrolling ticker line: clear whatever the
    // ticker is showing, build the challenge's localised description under the
    // "CHALLENGE_TICKER_STRING_DESCRIPTION" dynamic-string id (its %1..%N positional markers
    // filled with the challenge's player count and each action's target value), then queue a
    // two-part custom message -- "<title>: <description>" -- four times.
    //
    // Callers: UpdateWFInit @0x82480200 (wS2.cpp:271) and UpdateRunning/UpdatePermenant
    // cases 573/574/576/581 (wS2.cpp:591/613/618/642).
    void RaceMainHudState::StartFreeburnChallengeTicker()
    {
        CGS_ASSERT(mbFreeburnChallengeTicker,
                   "mbFreeburnChallengeTicker == true");                    // cpp:4038 (lbz 0x167)

        PostTickerClear536(mpStateInterface);

        // Both asserts below belong to the INLINED accessors, not to this function:
        // "mpChallengeManager" is GuiCache::GetFreeburnChallengeManager's (BrnGuiCache.h:2390)
        // and "meInternalState != E_INTERNAL_STATE_OFF" is GetCurrentChallenge's
        // (BrnGuiFreeburnChallengeManager.h:235). Restored as the calls they came from.
        const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
        const BrnResource::ChallengeListEntry* lpChallenge = lpManager->GetCurrentChallenge();

        // ---- the positional parameters (@0x8247AAA8..0x8247AB94) -----------------------
        char lacParamText[KI_TICKER_MAX_PARAMS][KU_TICKER_PARAM_TEXT_LEN];
        CgsLanguage::LanguageManager::ParameterFormatType
             laeParamFormat[KI_TICKER_MAX_PARAMS];
        for (s32 liSlot = 0; liSlot < KI_TICKER_MAX_PARAMS; ++liSlot)
        {
            lacParamText[liSlot][0] = 0;
            laeParamFormat[liSlot]  = CgsLanguage::LanguageManager::E_FORMAT_TEXT;
        }

        // Parameter 0 is always the challenge's player count. @0x8247AAC0 reads the byte at
        // +0xD3 and masks it with `clrlwi r6, r11, 28` (== & 0xF) -- that mask IS
        // GetNumPlayers()'s body (muNumPlayers packs the current count in the low nibble;
        // BrnChallengeManager_wB_03.cpp:118 records the same read).
        CgsCore::SnPrintf(lacParamText[0], KU_TICKER_PARAM_TEXT_LEN, "%d",
                          lpChallenge->GetNumPlayers());
        lacParamText[0][KU_TICKER_PARAM_TEXT_LEN - 1] = 0;
        laeParamFormat[0] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;   // `li r23, 0xB`

        s32 liNumParams = 1;
        for (s32 liActionIndex = 0;
             liActionIndex < lpChallenge->GetNumActions();                  // lbz 0xD4, re-read each pass
             ++liActionIndex)
        {
            // The two loop-body asserts (ChallengeListEntry.h:941/:942) are GetAction's own,
            // inlined; the receiver walks `entry + 0x50 * index` == &maAction[index].
            const BrnResource::ChallengeListEntryAction* lpAction =
                lpChallenge->GetAction(liActionIndex);
            if (lpAction->GetNumTargets() != 0)                             // lbz action+0x30
            {
                CgsCore::SnPrintf(lacParamText[liNumParams], KU_TICKER_PARAM_TEXT_LEN, "%d",
                                  lpAction->GetTargetValue(0));             // lwz action+0x34
                lacParamText[liNumParams][KU_TICKER_PARAM_TEXT_LEN - 1] = 0;
                laeParamFormat[liNumParams] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
                ++liNumParams;
            }
        }

        // @0x8247ABF8 -- the function IDA leaves as sub_82866450 is
        // LanguageManager::FormatAndAddText(id, source, format, count, ...) (its body is
        // FormatTextV into a 1KB local followed by AddString under the id; the tree already
        // homes it at CgsLanguageManager.h:222). r6 == 9 == E_FORMAT_ID_LOOKUP, i.e. the
        // source is resolved as a loc-string id first. All four (text, format) pairs are
        // pushed; only liNumParams of them are read.
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "CHALLENGE_TICKER_STRING_DESCRIPTION",
            lpChallenge->GetDescriptionStringID(),                          // entry + 0xA0
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
            liNumParams,
            lacParamText[0], laeParamFormat[0],
            lacParamText[1], laeParamFormat[1],
            lacParamText[2], laeParamFormat[2],
            lacParamText[3], laeParamFormat[3]);

        // @0x8247ABFC: `ld r31, 0xC0(r20)` -- a FULL 64-bit CgsID load (mChallengeID), then
        // the list lookup that turns it into a dense challenge index.
        const CgsID lChallengeID = lpChallenge->GetChallengeID();
        const s32   liChallengeIndex =
            mpCache->GetFreeburnChallengeList()->GetChallengeIndex(lChallengeID);

        GuiTickerCustomMessagePayload537 lMessage;
        // @0x8247AC14..0x8247AC48: {loop, training, allowDuplicates, isChallengeMessage} ==
        // {1, 0, 1, 1} -- this one IS a challenge message (contrast the not-active twin).
        lMessage.Construct(true, false, true, true);

        // @0x8247AC40..0x8247ADC4 -- the console inlines
        // CgsContainers::FastBitArray<2000>::IsBitSet over the manager's completed-challenge
        // bit store (`addi r26, r19, 0x7F0` == the LOCAL player's CompletedFburnChallenges
        // inside mCompletedData; `srawi 6 / slwi 3 / ldx` then `1ULL << (index & 63)`),
        // including that template's own range assert -- "Index <n> is out of range (max bits:
        // 2000)", CgsFastBitArray.h:396, streamed in hex. A set bit means the local player has
        // ALREADY completed this challenge, and the ticker then prefixes the line with "[~]".
        // FLAG deferred: BrnGuiFreeburnChallengeManager.h:148 deliberately leaves the
        // mCompletedData tail unmodelled ("HONEST BOUNDARY" -- its real home is the GameState
        // IO header graph), so there is no way to read the bit by name from here and no way
        // to reach it at all without growing that header. Deferred to "not completed", which
        // is the common case and the un-prefixed format; the index is still computed above
        // because FormatAndAddText's line above does not depend on it and the lookup is the
        // half that is recoverable.
        // DELETE-WHEN: BrnGuiFreeburnChallengeManager.h models mCompletedData (or publishes
        // `bool HasLocalPlayerCompleted(s32 liChallengeIndex) const`); this becomes that call.
        LogDeferredComponent("FreeburnChallengeManager::mCompletedData (completed-challenge bit)");
        const bool lbAlreadyCompleted = false;
        (void)liChallengeIndex;

        // @0x8247ADC8..0x8247AE34 -- the separator format. FRENCH (ELanguage 10) puts a space
        // BEFORE the colon; every other language does not. The "[~]" prefix marks a challenge
        // the local player has already completed.
        const bool lbFrenchSpacing =
            mpStateInterface->GetLanguageManager()->GetCurrentLanguage()
                == static_cast<s32>(CgsLanguage::E_LANGUAGE_FRENCH);
        const char* lpacSeparatorFormat;
        if (lbAlreadyCompleted)
            lpacSeparatorFormat = lbFrenchSpacing ? "[~] %1 : %2" : "[~] %1: %2";
        else
            lpacSeparatorFormat = lbFrenchSpacing ? "%1 : %2" : "%1: %2";

        lMessage.AddString(lpacSeparatorFormat,
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_TEXT);      // li r5, 1
        lMessage.AddString(lpChallenge->GetTitleStringID(),                           // entry + 0xB0
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_STRINGID);  // li r5, 2
        lMessage.AddString("CHALLENGE_TICKER_STRING_DESCRIPTION",
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_STRINGID);

        PostTickerCustomMessage537(mpStateInterface, lMessage);
    }

    // =======================================================================
    //  StartFreeburnChallengeNotActiveTicker  @ 0x8247AF38
    // =======================================================================
    // The twin of the body above for the "a challenge is running but this machine is not in
    // it" case: clear the ticker, then queue the single fixed "CHALLENGE_IN_PROGRESS" line
    // four times. No challenge record is read, so no manager/list lookup and no parameters.
    //
    // Callers: UpdateWFInit @0x82480200 (wS2.cpp:273) and UpdateRunning case 583
    // (wS3.cpp:653).
    void RaceMainHudState::StartFreeburnChallengeNotActiveTicker()
    {
        CGS_ASSERT(mbFreeburnChallengeTicker,
                   "mbFreeburnChallengeTicker == true");                    // cpp:4138 (lbz 0x167)

        PostTickerClear536(mpStateInterface);

        GuiTickerCustomMessagePayload537 lMessage;
        // @0x8247AFC4..0x8247AFDC: {loop, training, allowDuplicates, isChallengeMessage} ==
        // {1, 0, 1, 0}. The ONLY difference from the active ticker's seed is the last flag --
        // this generic line is not tied to a challenge, so a ticker clear that deletes
        // challenge messages must not delete it.
        lMessage.Construct(true, false, true, false);
        lMessage.AddString("CHALLENGE_IN_PROGRESS",
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_STRINGID);  // li r5, 2

        PostTickerCustomMessage537(mpStateInterface, lMessage);
    }
}
