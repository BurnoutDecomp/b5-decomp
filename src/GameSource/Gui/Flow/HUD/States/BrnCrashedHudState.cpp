// Crash HUD lifecycle and offline presentation, recovered from ARTIST.
// Online mugshot and road-rule-shot dispatch remain un-reconstructed.
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedHudState.h"
#include "GameShared/GameClasses/Containers/CgsHash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventTickerClearMessages / GuiEventTickerCustomMessage
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed asserts)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager
#include "SharedClasses/DataLists/ChallengeList.h"                        // BrnResource::ChallengeList
#include "SharedClasses/DataLists/ChallengeListEntry.h"                   // BrnResource::ChallengeListEntry(Action)
#include <cstring>
namespace BrnGui {
namespace {
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // The ticker records go out on the GUI-out channel, boxed in the 12-byte wrapper header.
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // Clear the ticker's challenge lines: {force fade-out 0, delete challenge messages 1}.
    void PostTickerClear(CgsGui::StateInterface* lpInterface)
    {
        GuiEventTickerClearMessages lClear;
        lClear.maData[0] = 0;
        lClear.maData[1] = 1;
        CgsGui::GuiEventWrapper<GuiEventTickerClearMessages, KI_CHANNEL_GUI_OUT> lRecord(lClear);
        static_assert(sizeof(lRecord) == 16, "the ticker-clear record is 16 bytes");
        lpInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_GUI_OUT, sizeof(lRecord));
    }

    // StartFreeburnChallengeTicker's four positional-parameter slots (64 characters each).
    const s32 KI_TICKER_MAX_PARAMS     = 4;
    const u32 KU_TICKER_PARAM_TEXT_LEN = 64;

    // The console queues the finished ticker message four times.
    const s32 KI_TICKER_MESSAGE_POST_COUNT = 4;

    // The completed-challenge bit store is a FastBitArray<2000>.
    const s32 KI_MAX_CHALLENGE_BITS = 2000;
}
const s32 CrashedHudState::maiEventToObserve[21] =
{
      5,   6,   7,  21,  64, 377, 154, 156, 148, 320, 291,
    140, 325, 574, 576, 578, 581, 573, 579, 547, 309,
};
const s32 CrashedHudState::miNumEventsObserved = 21;

// =======================================================================
//  The static .rdata resource table @0x82F263A0 (count @0x82F263C0)
// =======================================================================
// Read straight out of the XEX image; see the header for the two-instruction address decode and
// for why the table's extent is self-confirming (four 8-byte tuples ending exactly where the
// count word begins, and that word reads 4).
//
// Each id is named via off_82F278E0[id], the same name table the CrashedStuntHudState,
// FBurnMainHudState and RaceMainHudState recoveries used -- re-checked here by resolving the
// stunt state's OWN first entry through it: id 194 comes back "B5CrashedStuntHud", which is
// what that already-committed table says it is. All four entries are type 7 ==
// E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE.
//
// ⭐ The list is the CrashedStunt one with ONE entry different -- 193 B5CrashedHud in place of
// 194 B5CrashedStuntHud -- which is the shape you would expect of two crash screens that share
// their messages, helper components and button glyphs and differ only in their own movie. Two
// independently-decoded tables agreeing on three of four entries is a further check that the
// address decode is right.
const CgsGui::sResourceTuple CrashedHudState::maResourcesToLoad[] =
{
    { 193u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHud
    {  38u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHudMessages
    {  63u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HelperComponents
    {  61u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ControllerButtons
};
const u32 CrashedHudState::muNumResourcesToLoad = 4;

    // @ 0x82473780 -- hash the component name and append it to the expected-apt-component id list.
    // DWARF declares this void; the X360 leaves the hash in r3 (the id it just stored). The
    // observable effect is the store into mauExpectedComponentIds[count] + count++.
    void CrashedHudState::SetExpectedComponent(const char* lpacComponentName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        // X360: inline strlen (char* walk to the NUL) then CalculateHash(name, len).
        const s32 liLength = static_cast<s32>(std::strlen(lpacComponentName));
        const u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacComponentName), liLength);

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
    }

    // @ 0x82473868 -- switch the impact-time apt page to "SteerWreck" and show the LTHUMB glyph.
    void CrashedHudState::EnterSteerWreckScreen()
    {
        mImpactTimePageChanger.AddOutputAptViewState("apt_Transition", "SteerWreck", false); // this+0x4B8
        mImpactTimeButton.SetButton(ButtonIconComponent::E_PADBUTTON_LTHUMB,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);          // this+0x544, (13, 0)
    }

    // @ 0x824738C0 -- switch the impact-time apt page to "ImpactTime" and show the SELECT glyph.
    void CrashedHudState::EnterImpactTimeScreen()
    {
        mImpactTimePageChanger.AddOutputAptViewState("apt_Transition", "ImpactTime", false); // this+0x4B8
        mImpactTimeButton.SetButton(ButtonIconComponent::E_PADBUTTON_SELECT,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);          // this+0x544, (4, 0)
    }

// ARTIST 0x82475DD0.
void CrashedHudState::OnEnter()
{
    meInternalState = E_CRASHINTERNALSTATE_GETCACHE;
    mbInImpactTime = false;
    mpCache = nullptr;
    auto* lpAccess = mpStateInterface->GetAccessPointers();
    CGS_ASSERT(lpAccess != nullptr, "mpAccessPointers != NULL");
    auto* lpFlapt = lpAccess->GetFlaptManager();
    CGS_ASSERT(lpFlapt != nullptr, "NULL != mpFlaptManager");
    BrnFlapt::FileRef lFile;
    lpFlapt->GetFile(&lFile, 0);
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
    // FLAG deferred: replay GuiModuleSerialiser::GetStaticLayout()->EndMessage();
    // GuiAccessPointers does not yet carry the replay serialiser (shared HUD limitation).
    BrnFlapt::MovieClipRef lRoot;
    lFile.GetRootMovieClip(&lRoot);
    lRoot.FindChildMovieClip(&mCrashHudAnimator, "CrashHUD_mc");
    CGS_ASSERT(mCrashHudAnimator.IsValid(), "mpMovieClipInst");
    mCrashHudAnimator.mpMovieClipInst->ResetTimeline();
    mHudMessageComponent.Construct("crashHudMessages_mc", mpStateInterface, nullptr);
    GuiCache* lpCache = lpAccess->GetGuiCache();
    CGS_ASSERT(lpCache != nullptr, "mpGuiCache");
    mHudMessageComponent.SetInGameMessagesQueue(lpCache->GetInGameMessagesQueue());
    mHudMessageComponent.Prepare("crashHudMessages_mc", lFile);
    mImpactTimePageChanger.Construct("ImpactOption_mc", mpStateInterface, nullptr);
    mImpactTimeButton.Construct("ImpactButton_mc", mpStateInterface, nullptr);
    mShowTimeButton1.Construct("ShowTimeButton1_mc", mpStateInterface, nullptr);
    mShowTimeButton2.Construct("ShowTimeButton2_mc", mpStateInterface, nullptr);
    mShowTimeAnimator.Construct("ShowHideComp_mc", mpStateInterface, nullptr);
    mMudAnimator.Construct("DirtAnimator_mc", mpStateInterface, nullptr);
    mMugShotComponent.Construct("CrashMugShot_mc", mpStateInterface, nullptr);
    mMugShotComponent.Prepare("CrashMugShot_mc", lFile, nullptr);
    mMugshotOpponentGamertag = {};
    AttachToTextFieldComponent(&mMugshotOpponentGamertag, "Gamertag_txt", "Gamertag_mc", "CrashMugShot_mc", lFile);
    // FLAG deferred: online RoadRuleShotComponent name/data and mugshot event handlers.
    mSkipPromptAnimator.Construct("skipAnim_cpt", mpStateInterface, nullptr);
    mSkipPromptButton.Construct("skipButton", mpStateInterface, nullptr);
    mSkipPromptAnimator.Prepare("skipAnim_cpt", lFile, nullptr);
    mSkipPromptButton.Prepare("skipButton", lFile);
    mbSkipPrompt = false;
    mbCrashIsSkippable = false;
    mbHudMessages = true;
    mbImpactTimer = true;
    mbShowTime = true;
    mbBoostBar = true;
}

// ARTIST 0x8247D308.
void CrashedHudState::OnLeave()
{
    CGS_ASSERT(mCrashHudAnimator.IsValid(), "mpMovieClipInst");
    mCrashHudAnimator.mpMovieClipInst->ResetTimeline();
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->PlayAptMovie("", 1);
    if (mpCache)
    {
        InGameMessagesQueue* lpMessages = mpCache->GetInGameMessagesQueue();
        lpMessages->muCurrentEventEndTime = 0;
        for (s32 liSlot = 0; liSlot < 2; ++liSlot)
            if (lpMessages->maeMessageState[liSlot] == E_MESSAGESTATE_WAITING ||
                lpMessages->maeMessageState[liSlot] == E_MESSAGESTATE_TRANSIN)
                lpMessages->maeMessageState[liSlot] = E_MESSAGESTATE_NOMESSAGE;
    }
}

// ARTIST 0x82481B88: advance through ready phases within the same update.
void CrashedHudState::Update()
{
    mbInImpactTime = false;
    switch (meInternalState)
    {
    case E_CRASHINTERNALSTATE_GETCACHE:
        UpdateGetCache();
    case E_CRASHINTERNALSTATE_LOADING:
        meInternalState = E_CRASHINTERNALSTATE_LOADING;
        if (!UpdateLoading()) break;
    case E_CRASHINTERNALSTATE_WF_INIT:
        meInternalState = E_CRASHINTERNALSTATE_WF_INIT;
        if (!UpdateWFInit()) break;
    case E_CRASHINTERNALSTATE_SETUPSTATE:
        meInternalState = E_CRASHINTERNALSTATE_SETUPSTATE;
        if (!UpdateSetupState()) break;
    case E_CRASHINTERNALSTATE_RUNNING:
        meInternalState = E_CRASHINTERNALSTATE_RUNNING;
        UpdateRunning();
        break;
    case E_CRASHINTERNALSTATE_IDLE:
        meInternalState = E_CRASHINTERNALSTATE_IDLE;
        break;
    default:
        CGS_ASSERT(false, "Should never call update in the following state");
        break;
    }
    UpdatePermenant();
    reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
}

// ARTIST 0x82476698.
void CrashedHudState::UpdateGetCache()
{
    auto* lpQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    CGS_ASSERT(mpCache == nullptr, "mpCache == NULL");
    for (s32 liId = lpQueue->GetFirstEvent(&lpEvent, &liSize); lpEvent;
         liId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liId == 64)
        {
            GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
            CGS_ASSERT(lpCache != nullptr, "Invalid cache in CrashedHudState::Update");
            mpCache = lpCache;
            break;
        }
    }
    CGS_ASSERT(mpCache != nullptr, "mpCache != NULL");
}

// ARTIST 0x8247CE98.
bool CrashedHudState::UpdateLoading()
{
    if (!mpCache) return false;
    if (mbHudMessages)
    {
        mHudMessageComponent.SetController(mpCache->GetHudMessageController());
        mHudMessageComponent.SetDirector(mpCache->GetHudMessageDirector());
        mHudMessageComponent.SetGameMode(static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(mpCache->GetGameMode()));
    }
    if (!mpCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad)) return false;
    mpStateInterface->PlayAptMovie("B5CrashedHud", 1);
    SetExpectedAptComponentList();
    return true;
}

// ARTIST 0x824753A8 / 0x82475428.
bool CrashedHudState::UpdateWFInit()
{
    CGS_ASSERT(mpCache != nullptr, "mpCache");
    return mpCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD);
}
void CrashedHudState::SetExpectedAptComponentList()
{
    mpCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);
    std::memset(mauExpectedComponentIds, 0, sizeof(mauExpectedComponentIds));
    muNumExpectedComponents = 0;
    SetExpectedComponent(mImpactTimePageChanger.GetName());
    SetExpectedComponent(mImpactTimeButton.GetName());
    SetExpectedComponent(mShowTimeButton1.GetName());
    SetExpectedComponent(mShowTimeButton2.GetName());
    CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM, "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");
    mpCache->SetExpectedAptComponentList(E_GUIFLOW_HUD, mauExpectedComponentIds, muNumExpectedComponents);
}

// ARTIST 0x8247CF60, offline crash presentation. Online road-rule-shot arm remains deferred.
bool CrashedHudState::UpdateSetupState()
{
    CGS_ASSERT(mpCache != nullptr, "Cache pointer should be valid by now as its used in the WFInit stage");
    mbBoostBar = false;
    mbShowTime = false;
    mbSkipPrompt = false;
    mbHudMessages = true;
    mbImpactTimer = mpCache->miGameFlowState != 2;
    struct BoostVisibility : CgsGui::GuiEvent<214>
    {
        u8 mbVisible;
        u8 maPad[3];
        BoostVisibility(bool lbVisible) : CgsGui::GuiEvent<214>(1, 12), mbVisible(lbVisible), maPad{} {}
    } lBoost(mbBoostBar);
    mpStateInterface->GetOutputEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBoost), 41, sizeof(lBoost));
    if (mbImpactTimer)
    {
        if (mbInImpactTime) EnterSteerWreckScreen();
        else EnterImpactTimeScreen();
    }
    else
    {
        mImpactTimePageChanger.AddOutputAptViewState("apt_Transition", "Invisible", false);
        mImpactTimeButton.SetButton(ButtonIconComponent::E_PADBUTTON_INVISIBLE, ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
    }
    mShowTimeAnimator.AddOutputAptViewState("apt_Transition", mbShowTime ? "Visible" : "Invisible", false);
    mShowTimeButton1.SetButton(mbShowTime ? ButtonIconComponent::E_PADBUTTON_LSHOULDER : ButtonIconComponent::E_PADBUTTON_INVISIBLE, ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
    mShowTimeButton2.SetButton(mbShowTime ? ButtonIconComponent::E_PADBUTTON_RSHOULDER : ButtonIconComponent::E_PADBUTTON_INVISIBLE, ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
    if (mbSkipPrompt && mbCrashIsSkippable) mSkipPromptAnimator.Run("transIn");
    mSkipPromptButton.SetItem("$HUD_END_CRASH", FlaptButtonIconComponent::E_PADBUTTON_SELECT, FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
    // REMOVED 2026-09-17: `mpCache->SetGameplayHudActive(true)` had been carried over from the
    // STUNT sibling (CrashedStuntHudState::UpdateSetupState @0x8247D9E0 stores 1 at cache
    // +0x407C); the plain crash HUD @0x8247CF60 has no such store, and every scan of the
    // image finds only the stunt state, RaceMainHudState::UpdateWFInit and FBurnMainHudState
    // raising it. It mattered: HudMessageAnalyzer defers HandleCrashedEvent until the
    // gameplay HUD is active, so with the flag raised HERE the road-rage "RRDamCrit" (and
    // every other leave-crash message) was triggered while this state was still up, queued
    // behind "CRASHED", and wiped by OnExit's WAITING-slot cancel before the main HUD could
    // start it -- the "Damage Critical" banner never showed.
    return true;
}

// ARTIST 0x824767D8.
void CrashedHudState::UpdateRunning()
{
    auto* lpQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    for (s32 liId = lpQueue->GetFirstEvent(&lpEvent, &liSize); lpEvent;
         liId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
        switch (liId)
        {
        case 154: if (mbHudMessages) mHudMessageComponent.AddMessage(lpEvent); break;
        case 156: if (mbHudMessages) mHudMessageComponent.TerminateMessages(); break;
        case 140:
            if (lpiPayload[0] == 0) mMudAnimator.AddOutputAptViewState("apt_Transition", "transin", false);
            else if (lpiPayload[0] == 1) mMudAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
            break;
        case 6: if (mbImpactTimer && lpiPayload[1] == 49) EnterSteerWreckScreen(); break;
        case 7: if (mbImpactTimer && lpiPayload[1] == 49) EnterImpactTimeScreen(); break;
        case 21:
        {
            const auto* lpTrigger = reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
            if (lpTrigger->meEventType == 4 && mbHudMessages && std::strcmp(lpTrigger->mpacComponentName, "crashHudMessages_mc") == 0)
                mHudMessageComponent.EndTransition();
            break;
        }
        // FLAG deferred: GUI325 online mugshot sequence, original HandleMugshotEvent.
        default: break;
        }
    }
    if (mbHudMessages) mHudMessageComponent.Update();
}

    void CrashedHudState::UpdatePermenant()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            switch (liEventId)
            {
            case 5:
                if (lpiPayload[1] == 49) mbInImpactTime = true;
                break;
            case 547:
                mbCrashIsSkippable = true;
                if (meInternalState > E_CRASHINTERNALSTATE_SETUPSTATE)
                    mSkipPromptAnimator.Run("transIn");
                break;
            case 377:
                // 0x82481C74: `cmpwi r11, 1 / beq` + `cmpwi r11, 3 / beq` -> "END_CRASH".
                // The producer (GameBridgeWorldToGui) posts 1 == E_CRASHBARSTATE_LEAVE_CRASHED on
                // the falling edge; 3 is the showtime-side spelling of the same leave.
                if (lpiPayload[0] == 1 || lpiPayload[0] == 3)
                {
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        // [crash-hud] witness. NOT X360.
                        *CgsDev::Log::gpDebugPrint
                            << "[crash-hud] CrashedHudState: GUI 377 payload=" << lpiPayload[0]
                            << " -> SendStateEvent(\"END_CRASH\")\n";
                    }
                    SendStateEvent("END_CRASH");
                }
                // 0|2 (START_CRASHED) is a no-op here: the state is already crashed. The console
                // has no arm for it either -- its test is `== 1 || == 3` and nothing else.
                break;

            case 320:
            case 291:
                SendStateEvent("PAUSE");
                break;

            case 148:
                // ARTIST 0x82481374 reads the controller flag as one byte.
                if (*reinterpret_cast<const u8*>(lpEvent) == 0)
                    SendStateEvent("PAUSE");
                break;

            case 309:
                // ARTIST: two byte flags and the current game-mode gate.
                if (reinterpret_cast<const u8*>(lpEvent)[0] == 1 &&
                    reinterpret_cast<const u8*>(lpEvent)[1] == 0 &&
                    mpStateInterface->GetAccessPointers()->GetGuiCache()->GetGameMode() == -1)
                    SendStateEvent("PAUSE");
                break;

            // The free-burn challenge arms. 573 carries the selector action word at +8:
            // action 2 restarts the ticker, 0/1/3 do nothing, anything else asserts.
            case 573:
            {
                const s32 liSelectorAction = lpiPayload[2];
                if (liSelectorAction == 2)
                {
                    StartFreeburnChallengeTicker();
                }
                else if (static_cast<u32>(liSelectorAction) > 3u)
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "Unknown freeburn challenge selector action ";
                    lStrStream << liSelectorAction;
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        lacMessage,
                        "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnCrashedHudState.cpp",
                        937);
                    CgsDev::Assert::EndAssert();
                }
                break;
            }

            // 574 (challenge start): the local-host byte at +8; only a zero byte restarts it.
            case 574:
                CGS_ASSERT(lpEvent != 0, "lpChallengeEvent");
                if (reinterpret_cast<const u8*>(lpEvent)[8] == 0)
                    StartFreeburnChallengeTicker();
                break;

            case 576:
                StartFreeburnChallengeTicker();
                break;

            case 578:
            case 579:
                PostTickerClear(mpStateInterface);
                break;

            case 581:
                if (mpCache->GetFreeburnChallengeManager()->IsActive())
                    StartFreeburnChallengeTicker();
                break;

            default:
                // The registered-but-undispatched ids land here. The console has no assert on
                // its default path in this function, so neither does this.
                break;
            }
        }
    }

    // Post the running free-burn challenge to the ticker: clear the ticker's challenge lines,
    // format the challenge description under "CHALLENGE_TICKER_STRING_DESCRIPTION" (its
    // positional markers filled with the player count and each action's first target value),
    // then queue "<title> : <description>" four times, prefixed "--- COMPLETED ---" when the
    // local player has already completed this challenge.
    void CrashedHudState::StartFreeburnChallengeTicker()
    {
        PostTickerClear(mpStateInterface);

        const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
        const BrnResource::ChallengeListEntry* lpChallenge = lpManager->GetCurrentChallenge();

        // The console hands all four (text, format) pairs to FormatAndAddText but fills only
        // liNumParams of them; the rest are zero-seeded here (never read past liNumParams).
        char lacParamText[KI_TICKER_MAX_PARAMS][KU_TICKER_PARAM_TEXT_LEN];
        CgsLanguage::LanguageManager::ParameterFormatType laeParamFormat[KI_TICKER_MAX_PARAMS];
        for (s32 liSlot = 0; liSlot < KI_TICKER_MAX_PARAMS; ++liSlot)
        {
            lacParamText[liSlot][0] = 0;
            laeParamFormat[liSlot]  = CgsLanguage::LanguageManager::E_FORMAT_TEXT;
        }

        CgsCore::SnPrintf(lacParamText[0], KU_TICKER_PARAM_TEXT_LEN, "%d", lpChallenge->GetNumPlayers());
        lacParamText[0][KU_TICKER_PARAM_TEXT_LEN - 1] = 0;
        laeParamFormat[0] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;

        s32 liNumParams = 1;
        for (s32 liActionIndex = 0; liActionIndex < lpChallenge->GetNumActions(); ++liActionIndex)
        {
            const BrnResource::ChallengeListEntryAction* lpAction = lpChallenge->GetAction(liActionIndex);
            if (lpAction->GetNumTargets() != 0)
            {
                CgsCore::SnPrintf(lacParamText[liNumParams], KU_TICKER_PARAM_TEXT_LEN, "%d",
                                  lpAction->GetTargetValue(0));
                lacParamText[liNumParams][KU_TICKER_PARAM_TEXT_LEN - 1] = 0;
                laeParamFormat[liNumParams] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
                ++liNumParams;
            }
        }

        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "CHALLENGE_TICKER_STRING_DESCRIPTION",
            lpChallenge->GetDescriptionStringID(),
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
            liNumParams,
            lacParamText[0], laeParamFormat[0],
            lacParamText[1], laeParamFormat[1],
            lacParamText[2], laeParamFormat[2],
            lacParamText[3], laeParamFormat[3]);

        const CgsID lChallengeID = lpChallenge->GetChallengeID();
        const s32 liChallengeIndex = mpCache->GetFreeburnChallengeList()->GetChallengeIndex(lChallengeID);

        GuiEventTickerCustomMessage lMessage = {};
        lMessage.Construct(true, false, true, true);

        // The local player's completed-challenge bit (FastBitArray<2000>::IsBitSet, inlined
        // with its own streamed range assert).
        if (liChallengeIndex >= KI_MAX_CHALLENGE_BITS)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Index ";
            lStrStream << liChallengeIndex;
            lStrStream << " is out of range (max bits: ";
            lStrStream << KI_MAX_CHALLENGE_BITS;
            lStrStream << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lacMessage,
                "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsFastBitArray.h",
                396);
            CgsDev::Assert::EndAssert();
        }
        const BrnGameState::GameStateModuleIO::CompletedFburnChallenges* lpCompleted =
            lpManager->GetCompletedChallengesData()->GetLocalPlayerCompletionStatus();
        const u64 lu64Word = lpCompleted->maxBits[liChallengeIndex >> 6];
        const bool lbAlreadyCompleted =
            lu64Word != 0 && (lu64Word & (static_cast<u64>(1) << (liChallengeIndex & 63))) != 0;

        lMessage.AddString(lbAlreadyCompleted ? "--- COMPLETED --- %1 : %2" : "%1 : %2", 1);
        lMessage.AddString(lpChallenge->GetTitleStringID(), 2);
        lMessage.AddString("CHALLENGE_TICKER_STRING_DESCRIPTION", 2);

        for (s32 liPost = 0; liPost < KI_TICKER_MESSAGE_POST_COUNT; ++liPost)
        {
            CgsGui::GuiEventWrapper<GuiEventTickerCustomMessage, KI_CHANNEL_GUI_OUT> lRecord(lMessage);
            static_assert(sizeof(lRecord) == 0x824, "the ticker message record is 0x824 bytes");
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lRecord)));
        }
    }
}
