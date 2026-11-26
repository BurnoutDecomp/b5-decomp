#include "GameSource/Gui/Flow/HUD/States/BrnCrashedStuntHudState.h"
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"
#include "GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.h"
#include "GameShared/GameClasses/Containers/CgsHash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Gui/BrnGuiShared.h"   // gGuiResourceIdentifier / EGuiResourceId
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <xboxmath.h>
namespace BrnGui {
namespace {
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
    struct GuiEventCache : CgsModule::Event { GuiCache* mpGuiCache; };
    const char* const KAPC_CRASH_RUNNING_STATES[] = {"default", "transin", "tallyscore", "transout"};
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnCrashedStuntHudState.cpp"
    inline f32 FusedMultiplyAdd(f32 lfA, f32 lfB, f32 lfC)
    {
        return __vmaddfp(XMVectorReplicate(lfA), XMVectorReplicate(lfB),
                         XMVectorReplicate(lfC)).x;
    }
}
const s32 CrashedStuntHudState::maiEventToObserve[12] =
{
      5,   6,   7,  21,  64, 377, 154, 156, 148, 320, 291, 140,
};
const s32 CrashedStuntHudState::miNumEventsObserved = 12;

// =======================================================================
//  The static .rdata resource table @0x82F26488 (count @0x82F264A8)
// =======================================================================
// Read straight out of the XEX image; the four 8-byte tuples end exactly where the count word
// begins, and that word reads 4, so the table's extent is self-confirming. Each id is named via
// off_82F278E0[id] from the same image -- the name table the FBurnMainHudState 42-entry and
// RaceMainHudState 21-entry recoveries used (re-checked here against RaceMain's published names:
// 192 -> "B5RaceHud", 32 -> "Timer", 24 -> "B5CompassComponent" all reproduce).
//
// All four are type 7 == E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE. Note the first entry names this
// state's own movie, B5CrashedStuntHud -- the crash screen the stub could never load.
CgsGui::sResourceTuple       CrashedStuntHudState::maResourcesToLoad[] =
{
    { 194u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedStuntHud
    {  38u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHudMessages
    {  63u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HelperComponents
    {  61u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ControllerButtons
};
u32       CrashedStuntHudState::muNumResourcesToLoad = 4;
// ARTIST 0x82476318.
void CrashedStuntHudState::OnEnter()
{
    meInternalState = E_CRASHINTERNALSTATE_LOADING;
    meRunningState = E_CRASHRUNNINGSTATE_NONE;
    mpCache = 0;
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->GetAccessPointers()->GetSerialiser()->GetStaticLayout()->EndMessage();
    CgsGui::GuiAccessPointers* lpAccess = mpStateInterface->GetAccessPointers();
    BrnFlapt::FlaptManager* lpFlapt = lpAccess->GetFlaptManager();
    BrnFlapt::FileRef lFile = lpFlapt->GetFile(BrnFlapt::E_FLAPTFILE_HUD);
    mCrashHudAnimator = lFile.GetRootMovieClip().FindChildMovieClip("CrashHUD_mc");
    mCrashHudAnimator.ResetTimeline();
    mHudMessageComponent.Construct("crashHudMessages_mc", mpStateInterface, 0);
    GuiCache* lpCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
    mHudMessageComponent.SetInGameMessagesQueue(lpCache->GetInGameMessagesQueue());
    mHudMessageComponent.Prepare("crashHudMessages_mc", lFile);
    mStuntScoreAnimator.Construct("StuntScore_anim", mpStateInterface, 0);
    mStuntMultiplierAnimator.Construct("MultiplierScore_anim", mpStateInterface, 0);
    mScoreTallyAnimator.Construct("ScoreTally_cpt", mpStateInterface, 0);
    mStuntRunScoreText.Construct("StuntScore_mc", mpStateInterface, 0);
    mStuntRunScoreText.SetAutoSize(true);
    mStuntRunMultiplierText.Construct("MultiplierScore_mc", mpStateInterface, 0);
    mbHudMessages = true;
    mbBoostBar = true;
    miStartScore = 0;
    miStartMultiplier = 0;
    miFinishScore = 0;
    miFinishMultiplier = 0;
    miCurrentScore = 0;
    miCurrentMultiplier = 0;
}

// ARTIST 0x82481CF0.
void CrashedStuntHudState::Update()
{
    switch (meInternalState)
    {
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
        CGS_FAIL("Should never call update in the following state");
        break;
    }
    UpdatePermenant();
    reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
}

// ARTIST 0x8247D918 / 0x82475890.
bool CrashedStuntHudState::UpdateLoading()
{
    bool lbReturnValue = false;   // DWARF local

    if (mpCache != 0)
    {
        if (mbHudMessages)
        {
            mHudMessageComponent.SetController(mpCache->GetHudMessageController());
            mHudMessageComponent.SetDirector(mpCache->GetHudMessageDirector());
            mHudMessageComponent.SetGameMode(static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(mpCache->GetGameMode()));
        }

        if (mpCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            // The movie name comes from the resource-name table (ARTIST `lwz r4, 0x308(r11)` off
            // gGuiResourceIdentifier).
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[E_GUI_RESOURCEID_APT_HUD_CRASHED_STUNT], 1);
            SetExpectedAptComponentList();
            lbReturnValue = true;
        }
    }

    return lbReturnValue;
}
bool CrashedStuntHudState::UpdateWFInit()
{
    bool lbReturnValue = false;   // DWARF local

    CGS_ASSERT_AT(mpCache != 0, "mpCache", CGS_ARTIST_FILE, 272);

    if (mpCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
    {
        lbReturnValue = true;
    }

    return lbReturnValue;
}

// ARTIST 0x82475910, inline helper sub_824F87C0 hashes and appends each name.
void CrashedStuntHudState::SetExpectedAptComponentList()
{
    CGS_ASSERT_AT(mpCache != 0, "mpCache", CGS_ARTIST_FILE, 762);
    mpCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);
    const char* lapcNames[] = { mStuntScoreAnimator.GetName(), mStuntMultiplierAnimator.GetName(),
        mScoreTallyAnimator.GetName(), mStuntRunScoreText.GetName(), mStuntRunMultiplierText.GetName() };
    for (u32 luIndex = 0; luIndex < sizeof(lapcNames) / sizeof(lapcNames[0]); ++luIndex)
    {
        const char* lpcName = lapcNames[luIndex];
        const u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName), static_cast<s32>(std::strlen(lpcName)));
        mpCache->AppendExpectedAptComponentList(E_GUIFLOW_HUD, &luHash, 1);
    }
}

// ARTIST 0x8247D9E0.
bool CrashedStuntHudState::UpdateSetupState()
{
    CGS_ASSERT(mpCache != 0, "Cache pointer should be valid by now as its used in the WFInit stage");
    mbBoostBar = false;
    mbHudMessages = true;
    struct BoostVisibility : CgsGui::GuiEvent<214>
    {
        u8 mbVisible;   // the one payload byte: {1, 214, 12} on channel 41
        BoostVisibility() : mbVisible(false) {}
    } lBoost;
    mpStateInterface->OutputViewState(lBoost);
    miStartScore = mpCache->miLastStuntScore;
    miStartMultiplier = mpCache->miGameFlowResetWord_9FD8;
    miFinishMultiplier = 1;
    miFinishScore = static_cast<s32>(static_cast<u32>(miStartScore) * static_cast<u32>(miStartMultiplier));
    char lacValue[16];
    if (miStartScore > 0 && miFinishScore > 0)
    {
        CgsCore::SnPrintf(lacValue, 15, "%d", miStartScore);
        mStuntRunScoreText.SetLocalisedText("STUNT_RUN_COMBO_SCORE", static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9), 1, lacValue, 11);
        if (miStartMultiplier > 1)
        {
            CgsCore::SnPrintf(lacValue, 15, "%d", miCurrentMultiplier);
            mStuntRunMultiplierText.SetLocalisedText("STUNTRUN_MULT_FORMAT", static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9), 1, lacValue, 11);
        }
        else mStuntRunMultiplierText.SetText("");
    }
    else
    {
        mStuntRunScoreText.SetText("");
        mStuntRunMultiplierText.SetText("");
    }
    meRunningState = E_CRASHRUNNINGSTATE_TRANSIN;
    mScoreTallyAnimator.AddOutputAptViewState("apt_Transition", KAPC_CRASH_RUNNING_STATES[meRunningState], false);
    mpCache->SetGameplayHudActive(true);
    return true;
}

// ARTIST 0x82481650.
bool CrashedStuntHudState::UpdateRunning()
{
    bool lbFinished = false;
    StateInputQueue* lpQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liId = lpQueue->GetFirstEvent(&lpEvent, &liSize); lpEvent;
         liId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liId == 21)
        {
            const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
            if (lpTrigger->meEventType == 4 && mbHudMessages)
            {
                if (std::strcmp(lpTrigger->mpacComponentName, "ScoreTally_cpt") == 0)
                {
                    if (miStartScore > 0 && miFinishScore > 0)
                    {
                        meRunningState = static_cast<CrashRunningState>(meRunningState + 1);
                        if (meRunningState == E_CRASHRUNNINGSTATE_COUNT)
                        {
                            lbFinished = true;
                            meRunningState = E_CRASHRUNNINGSTATE_NONE;
                        }
                        else
                        {
                            mScoreTallyAnimator.AddOutputAptViewState("apt_Transition", KAPC_CRASH_RUNNING_STATES[meRunningState], false);
                            if (meRunningState == E_CRASHRUNNINGSTATE_TALLYSCORE)
                                mfTallyScoreStartTime = mpCache->GetTime();
                        }
                    }
                }
                else if (std::strcmp(lpTrigger->mpacComponentName, "crashHudMessages_mc") == 0)
                    mHudMessageComponent.EndTransition();
            }
        }
        else if (liId == 154)
        {
            // ARTIST 0x824816F0..0x82481704 builds the full 64-bit Drive Away message ID.
            if (mbHudMessages && *reinterpret_cast<const CgsID*>(lpEvent) !=
                ((static_cast<u64>(0x5BBA2B74u) << 32) | 0x4F8C0000u))
                mHudMessageComponent.AddMessage(lpEvent);
        }
        else if (liId == 156 && mbHudMessages) mHudMessageComponent.TerminateMessages();
    }
    if (mbHudMessages) mHudMessageComponent.Update();
    UpdateCrashRunningState();
    return lbFinished;
}

// ARTIST 0x8247DC10: two-second score/multiplier tally, nearest integer via floor(x+0.5).
void CrashedStuntHudState::UpdateCrashRunningState()
{
    if (meRunningState == E_CRASHRUNNINGSTATE_TALLYSCORE && miStartScore > 0 && miFinishScore > 0)
    {
        const f32 lfElapsed = (mpCache->GetTime() - mfTallyScoreStartTime) * 0.5f;
        const f32 lfNonnegative = -lfElapsed >= 0.0f ? 0.0f : lfElapsed;
        const f32 lfProportion = 1.0f - lfNonnegative >= 0.0f ? lfNonnegative : 1.0f;
        // The console uses a fused scalar multiply-add here. Replicating the operands and
        // using XDK's VMX intrinsic keeps one-rounding semantics under the C++03 CRT.
        const s32 liScore = static_cast<s32>(std::floor(
            FusedMultiplyAdd(static_cast<f32>(miFinishScore) - static_cast<f32>(miStartScore),
                             lfProportion, static_cast<f32>(miStartScore)) + 0.5f));
        const s32 liMultiplier = static_cast<s32>(std::floor(
            FusedMultiplyAdd(static_cast<f32>(miFinishMultiplier) - static_cast<f32>(miStartMultiplier),
                             lfProportion, static_cast<f32>(miStartMultiplier)) + 0.5f));
        char lacValue[32];
        if (miCurrentScore != liScore)
        {
            miCurrentScore = liScore;
            CgsCore::SnPrintf(lacValue, 31, "%d", miCurrentScore);
            mStuntRunScoreText.SetLocalisedText("STUNT_RUN_COMBO_SCORE", static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9), 1, lacValue, 11);
        }
        if (miCurrentMultiplier != liMultiplier)
        {
            miCurrentMultiplier = liMultiplier;
            if (miCurrentMultiplier > 1)
            {
                CgsCore::SnPrintf(lacValue, 31, "%d", miCurrentMultiplier);
                mStuntRunMultiplierText.SetLocalisedText("STUNTRUN_MULT_FORMAT", static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9), 1, lacValue, 11);
                mStuntMultiplierAnimator.AddOutputAptViewState("apt_Transition", "bounce", false);
            }
            else
            {
                mStuntRunMultiplierText.SetText("");
                mStuntMultiplierAnimator.AddOutputAptViewState("apt_Transition", "default", false);
            }
        }
    }
    else if (meRunningState < E_CRASHRUNNINGSTATE_NONE || meRunningState >= E_CRASHRUNNINGSTATE_COUNT)
        CGS_FAIL("Unhandled CrashRunning state in CrashedStuntHudState::UpdateCrashRunningState()");
    struct TallyScore : CgsGui::GuiEvent<465>
    {
        s32 miScore; f32 mfTime;
    } lTally;
    lTally.miScore = miCurrentScore;
    lTally.mfTime = 0.0f;
    mpStateInterface->OutputGuiEvent(lTally);
}

// ARTIST 0x8247DF68.
void CrashedStuntHudState::OnLeave()
{
    // DWARF (_compile/BrnGuiHUDUnity.cpp): MovieClipRef::ResetTimeline and
    // InGameMessagesQueue::ClearPending inlined.
    mCrashHudAnimator.ResetTimeline();
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->PlayAptMovie("", 1);
    if (mpCache)
    {
        mpCache->GetInGameMessagesQueue()->ClearPending();
    }
}

    void CrashedStuntHudState::UpdatePermenant()
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
            case 377:
                // 0x82476B38: `cmpwi r11, 1 / beq` + `cmpwi r11, 3 / bne` -> "END_CSTNT".
                // The producer (GameBridgeWorldToGui) posts 1 == E_CRASHBARSTATE_LEAVE_CRASHED on
                // the falling edge; 3 is the showtime-side spelling of the same leave. The lua
                // maps END_CSTNT back to state 2 RACE_MAIN -- the race UI returning.
                if (lpiPayload[0] == 1 || lpiPayload[0] == 3)
                {
                    SendStateEvent("END_CSTNT");
                }
                // 0|2 (START_CRASHED) is a no-op here: the state is already crashed. The console
                // has no arm for it either -- its test is `== 1 || == 3` and nothing else.
                break;

            case 320:
            case 291:
                SendStateEvent("PAUSE");
                break;

            case 148:
                // 0x82476A84 `lwz r11, 0(r21)` -- a WORD here, matching CrashedHudState's reading
                // of the same id (PausedHudState reads it as a BYTE; that difference is the
                // console's, and each is kept as written).
                if (lpiPayload[0] == 0)
                    SendStateEvent("PAUSE");
                break;

            case 64:
                // The per-frame cache event: this is the ONLY producer of mpCache, which is why
                // the deferred phase bodies (all of which assert on it) could never have run
                // without this arm. Assert text transcribed exactly as the console has it --
                // including its reference to the SIBLING class, which is a copy-paste in the
                // original source: the string is "Invalid cache in CrashedHudState::Update" but
                // the file/line the console passes are BrnCrashedStuntHudState.cpp:571
                // (`li r5, 0x23B` at 0x82476B10). The message is the console's; it is not
                // corrected here.
                //
                // The X360 reads the payload WORD (`lwz r11, 0(r21)` / `stw r11, 0x40(r23)`);
                // this reads the same field BY NAME off the typed record, because a guest word
                // cannot carry a host pointer.
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache in CrashedHudState::Update");
                    mpCache = lpCache;
                }
                break;

            default:
                // The seven registered-but-undispatched ids land here. The console has no assert
                // on its default path in this function, so neither does this -- adding one would
                // be an invented arm.
                break;
            }
        }
    }
}
