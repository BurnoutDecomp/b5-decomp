#include "GameSource/Gui/Flow/HUD/States/BrnBootProfile.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::StrCpy / SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (stage log)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager (no-space formatting)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (resource/watcher surface)

#include <cstdio>   // std::snprintf (the stage log)

// BrnGui::BootProfile -- reconstructed from BURNOUT_X360_ARTIST.XEX
//   (Construct @0x824743F8, OnEnter @0x82478440, OnLeave @0x824784F8,
//    Update @0x8247E500, HandleControllerInput @0x82478610,
//    HandleProfileTaskResult @0x82474468; GetResourcesToLoad inline in the header)
// plus the embedded BrnGui::ProfileMessageComponent (this TU is its DWARF home:
//   Construct @0x82473B30, AppendExpectedAptComponent @0x82473C38,
//   GetNumOptions @0x82473C30, ResendMessageToApt @0x82473CA8,
//   ShowMessage @0x82473DC0, ShowNoSpaceMessage @0x82474200, HideMessage @0x82474370).
//
// BF_PROFILE: after the title screen accepts, this state plays the save/load prompt
// apt movie "SaveLoadComponent" at display level 3, boots the REAL
// BrnGui::ProfileManager (Bootup task -> the module's collision-world swap dance ->
// the save/load system -> BootupResult), surfaces the manager's prompts through the
// embedded ProfileMessageComponent, routes accept/back into HandleMessageChoice, and
// -- when the manager's ReportTaskCompleted calls HandleProfileTaskResult -- signals
// phase-complete (command 70 + the loading screen).
namespace BrnGui
{
    namespace
    {
        const s32 KI_EVENT_CONTROLLER     = 6;    // controller action (sub-id @+4)
        const s32 KI_EVENT_GUI_CACHE      = 64;   // per-frame cache event (GuiCache* payload)
        const s32 KI_EVENT_OVERLAY_RESULT = 189;  // overlay result (the profile message choices)

        const s32 KI_ACTION_ACCEPT = 49;   // X360 HandleControllerInput accept sub-id
        const s32 KI_ACTION_BACK   = 50;   // X360 HandleControllerInput back sub-id
        // FLAG PC-platform input bridge: the console A-button "select" the popup accepts arrives as
        // action 49; the PC input reconstruction (CgsInputPadsPC) maps the accept key (ENTER/SPACE/
        // pad-A) to action 45 -- the same accept-key action the title SelectionMenu consumes -- so
        // recognise 45 as the accept here too, or the autosave-warning prompt could never be
        // dismissed on PC. Behaviour is otherwise identical to the X360 sub-id-49 path.
        const s32 KI_ACTION_ACCEPT_PC = 45;

        const s32 KI_CHANNEL_GUI_OUT    = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;  // GuiOutViewState

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // ARTIST controller event 6 payload. HandleControllerInput reads the second
        // dword (button/action id), after the pad id.
        struct ControllerInputPressedPayload : public CgsModule::Event
        {
            s32 miPadId;
            s32 miButtonId;
        };

        // 16-byte GuiEvent<N> command { 1, N, 12 } (the shared boot-state channel record).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel)
        {
            GuiCommandEvent16<N> lEvent(0);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // 20-byte GuiEvent<25> "set option" command { 8, 25, 12, reserved, 1.0f } on the
        // view channel (the X360 OnEnter posts reserved=1; same record BrnBootLegal posts).
        struct GuiOptionEvent20 : public CgsGui::GuiEvent<25>
        {
            s32 miReserved;   // +0x0C
            f32 mfValue;      // +0x10 (1.0f)
            explicit GuiOptionEvent20(s32 liReserved)
                : CgsGui::GuiEvent<25>(8, 12), miReserved(liReserved), mfValue(1.0f) {}
        };

        // 20-byte GuiEvent<18> "apt name + flag" record { 8, 18, 12, name, 1 } on the view
        // channel -- the OnLeave unload post (empty-name sentinel; same as BrnBootLegal's).
        struct GuiAptNameFlagEvent20 : public CgsGui::GuiEvent<18>
        {
            const char* mpacAptName;   // +0x0C ("" sentinel)
            s32         miFlag;        // +0x10 (1)
            GuiAptNameFlagEvent20(const char* lpacAptName, s32 liFlag)
                : CgsGui::GuiEvent<18>(8, 12), mpacAptName(lpacAptName), miFlag(liFlag) {}
        };

        // The cache watcher is the ARTIST component-initialisation handshake:
        // register the five expected SaveLoad clips by name, then wait until their
        // Apt ONLOAD triggers have marked them initialised.
        void CacheAppendExpectedAptComponent(GuiCache* lpCache, GuiFlow leFlow,
                                             const char* lpacComponentName)
        {
            if (lpCache != 0)
                lpCache->AppendExpectedAptComponent(leFlow, lpacComponentName);
        }

        bool CacheAreAllAptComponentsInitialised(GuiCache* lpCache)
        {
            return lpCache != 0 &&
                   lpCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN);
        }
    }

    // =======================================================================
    //  BrnGui::ProfileMessageComponent
    // =======================================================================

    // DWARF cpp:47..cpp:53 -- the movie clip names (Construct @0x82473B30 string xrefs)
    // + the blank string.
    const char ProfileMessageComponent::macMessageTextName[8]       = "message";
    const char ProfileMessageComponent::macLeftOptionTextName[8]    = "option0";
    const char ProfileMessageComponent::macRightOptionTextName[8]   = "option1";
    const char ProfileMessageComponent::macLeftOptionButtonName[15] = "option0_button";
    const char ProfileMessageComponent::macRightOptionButtonName[15] = "option1_button";
    const char ProfileMessageComponent::macBlankString[1]           = "";

    // @ 0x82473B30 (cpp:76) -- stash the interface, Construct the five sub-components
    // (the X360 dispatches each Construct through the component vtable), clear the
    // message state.
    void ProfileMessageComponent::Construct(CgsGui::StateInterface* lpStateInterface)
    {
        mpStateInterface = lpStateInterface;
        mMessageText.Construct(macMessageTextName, lpStateInterface, 0);
        mLeftOptionText.Construct(macLeftOptionTextName, lpStateInterface, 0);
        mRightOptionText.Construct(macRightOptionTextName, lpStateInterface, 0);
        mLeftOptionButton.Construct(macLeftOptionButtonName, lpStateInterface, 0);
        mRightOptionButton.Construct(macRightOptionButtonName, lpStateInterface, 0);
        miNumOptions     = 0;
        muSaveDataSizeKb = 0;
        muGameDataSizeKb = 0;
        mbHasMessage     = false;
    }

    // @ 0x82473C38 (cpp:121) -- register the five sub-component names on the cache's
    // apt-component watcher (GuiCache::AppendExpectedAptComponent @0x824F87C0 per name;
    // PC boundary no-op, see above).
    void ProfileMessageComponent::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CacheAppendExpectedAptComponent(lpGuiCache, leFlow, mMessageText.GetName());
        CacheAppendExpectedAptComponent(lpGuiCache, leFlow, mLeftOptionText.GetName());
        CacheAppendExpectedAptComponent(lpGuiCache, leFlow, mRightOptionText.GetName());
        CacheAppendExpectedAptComponent(lpGuiCache, leFlow, mLeftOptionButton.GetName());
        CacheAppendExpectedAptComponent(lpGuiCache, leFlow, mRightOptionButton.GetName());
    }

    // @ 0x82473C30 (cpp:108).
    s32 ProfileMessageComponent::GetNumOptions()
    {
        return miNumOptions;
    }

    // @ 0x82473CA8 (cpp:136) -- (re)push the stored prompt to the apt movie: the
    // message text (with the two KB counts as positional parameters on the no-space
    // prompt), the two option texts (loc-id lookups), then the option glyphs per the
    // option count.
    void ProfileMessageComponent::ResendMessageToApt()
    {
        if (muSaveDataSizeKb != 0 && muGameDataSizeKb != 0)
        {
            // X360: the parameterised TextField::SetLocalisedText @0x824E7F78 --
            // macMessageId under E_FORMAT_ID_LOOKUP with the two KB counts as
            // E_FORMAT_INTEGER positional parameters (%1/%2), via the two-slot
            // LanguageManager formatter @0x828659E8. That vararg overload is not
            // committed yet; compose the identical result from the committed formatter
            // surface (the E_FORMAT_INTEGER parameter branch atoi's its text, so the
            // decimal marshalling below reproduces the console's integer formatting),
            // then adopt + push exactly as the overload's tail does (SetDatabaseText +
            // OutputAptData).
            CgsLanguage::LanguageManager* lpLanguageManager =
                mpStateInterface->GetLanguageManager();

            char lacSaveKb[16];
            char lacGameKb[16];
            CgsCore::SPrintf(lacSaveKb, sizeof(lacSaveKb), "%u", muSaveDataSizeKb);
            CgsCore::SPrintf(lacGameKb, sizeof(lacGameKb), "%u", muGameDataSizeKb);

            const char* lapcParams[2] = { lacSaveKb, lacGameKb };
            const CgsLanguage::LanguageManager::ParameterFormatType laeParamTypes[2] =
            {
                CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
            };

            char lacMessage[1120];   // the formatter writes through a 1024 cap
            lpLanguageManager->Obsolete_FormatTextByArray(
                lacMessage, 1024, macMessageId,
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2, lapcParams,
                laeParamTypes);

            mMessageText.SetDatabaseText(lacMessage);
            mMessageText.OutputAptData();
        }
        else
        {
            mMessageText.SetLocalisedText(macMessageId,
                                          CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        }

        mLeftOptionText.SetLocalisedText(macLeftOptionId,
                                         CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        mRightOptionText.SetLocalisedText(macRightOptionId,
                                          CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);

        switch (miNumOptions)
        {
        case 0:
            mLeftOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                        ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
            mRightOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                         ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
            break;

        case 1:
            mLeftOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_SELECT,
                                        ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
            mRightOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                         ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
            break;

        case 2:
            mLeftOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_SELECT,
                                        ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
            mRightOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_BACK,
                                         ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
            break;

        default:
            CGS_ASSERT(false, "Unhandled number of options in profile message displayer\n");   // cpp:177
            break;
        }
    }

    // @ 0x82473DC0 (cpp:195) -- adopt the prompt (message + up to two option loc ids),
    // clear the no-space KB counts, and push it to the movie.
    void ProfileMessageComponent::ShowMessage(const char* lpcMessage, u32 luNumberOfOptions,
                                              const char** lpacOptions)
    {
        CGS_ASSERT(lpcMessage != 0, "lpMessage != NULL");           // cpp:197
        CGS_ASSERT(luNumberOfOptions <= 2, "luOptionCount <= 2");   // cpp:199

        miNumOptions     = static_cast<s32>(luNumberOfOptions);
        mbHasMessage     = true;
        muSaveDataSizeKb = 0;
        muGameDataSizeKb = 0;

        CgsCore::StrCpy(macMessageId, KU_MAX_MESSAGE_ID_LEN, lpcMessage);

        switch (luNumberOfOptions)
        {
        case 0:
            CgsCore::StrCpy(macLeftOptionId, KU_MAX_MESSAGE_ID_LEN, macBlankString);
            CgsCore::StrCpy(macRightOptionId, KU_MAX_MESSAGE_ID_LEN, macBlankString);
            break;

        case 1:
            CgsCore::StrCpy(macLeftOptionId, KU_MAX_MESSAGE_ID_LEN, lpacOptions[0]);
            CgsCore::StrCpy(macRightOptionId, KU_MAX_MESSAGE_ID_LEN, macBlankString);
            break;

        case 2:
            CgsCore::StrCpy(macLeftOptionId, KU_MAX_MESSAGE_ID_LEN, lpacOptions[0]);
            CgsCore::StrCpy(macRightOptionId, KU_MAX_MESSAGE_ID_LEN, lpacOptions[1]);
            break;

        default:
            // The X360 streams the same "Unhandled number of options" assert (cpp:223)
            // and still falls through to the resend.
            CGS_ASSERT(false, "Unhandled number of options in profile message displayer\n");
            break;
        }

        ResendMessageToApt();
    }

    // @ 0x82474200 (cpp:235) -- adopt the option-less no-space prompt: the loc id plus
    // the required/free KB counts the resend formats into it.
    void ProfileMessageComponent::ShowNoSpaceMessage(const char* lpcMessage, u32 luRequiredKb,
                                                     u32 luFreeKb)
    {
        CGS_ASSERT(lpcMessage != 0, "lpMessage != NULL");   // cpp:237

        muSaveDataSizeKb = luRequiredKb;
        muGameDataSizeKb = luFreeKb;
        mbHasMessage     = true;
        miNumOptions     = 0;

        CgsCore::StrCpy(macMessageId, KU_MAX_MESSAGE_ID_LEN, lpcMessage);
        CgsCore::StrCpy(macLeftOptionId, KU_MAX_MESSAGE_ID_LEN, macBlankString);
        CgsCore::StrCpy(macRightOptionId, KU_MAX_MESSAGE_ID_LEN, macBlankString);

        ResendMessageToApt();
    }

    // @ 0x82474370 (cpp:257) -- blank the three text fields, hide both glyphs, drop
    // the message state.
    void ProfileMessageComponent::HideMessage()
    {
        mMessageText.SetText(macBlankString);
        mLeftOptionText.SetText(macBlankString);
        mRightOptionText.SetText(macBlankString);
        mLeftOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                    ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
        mRightOptionButton.SetButton(ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
        miNumOptions = 0;
        mbHasMessage = false;
    }

    // =======================================================================
    //  BrnGui::BootProfile
    // =======================================================================

    // @ 0x8205AB98 (.rdata, no exported value): the 3 observed ids, pinned from the
    // Update/HandleControllerInput dispatch (64 at GETCACHE, 6 at RUNNING) and the
    // profile message choices' overlay-result feedback.
    const s32 BootProfile::maiEventToObserve[] =
        { KI_EVENT_GUI_CACHE, KI_EVENT_CONTROLLER, KI_EVENT_OVERLAY_RESULT };
    const s32 BootProfile::miNumEventsObserved = 3;

    // ARTIST .rdata @0x82F25F78: { id 127, type 4 }; the count at 0x82F25F80 is 1.
    // off_82F278E0[127] names the resource "SaveLoadComponent".
    const CgsGui::sResourceTuple BootProfile::maResourcesToLoad[1] =
        { { 127u, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 BootProfile::muNumResourcesToLoad = 1;

    // @ 0x824743F8 (cpp:282) -- base Construct then thread the profile manager
    // (BrnHudFlow::Prepare @0x8251A620 dispatches this wider overload for BF_PROFILE).
    void BootProfile::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm,
                                ProfileManager& lrProfileManager)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");   // cpp:282
        CgsGui::State::Construct(liId, lpFsm);
        mpProfileManager = &lrProfileManager;
        mpGuiCache       = 0;
        mbCheckDiskSpace = false;
    }

    // @ 0x82478440 -- seed GETCACHE, register the observed set, construct the message
    // component + attach it to the manager, and post the view-option record
    // { 8, 25, 12, 1, 1.0f }.
    void BootProfile::OnEnter()
    {
        meInternalState = E_INTERNALSTATE_GETCACHE;
        if (mpStateInterface != 0)
            mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mProfileMessage.Construct(mpStateInterface);
        if (mpProfileManager != 0)   // X360 unconditional; PC null-guard, see the flow's fallback
            mpProfileManager->AttachMessageDisplay(&mProfileMessage);
        GuiOptionEvent20 lOption(1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lOption), KI_CHANNEL_VIEW_STATE, 20);
    }

    // @ 0x824784F8 -- detach the message display, unregister, unload the save/load apt
    // (the empty-name level record on the view channel), release the profile resources.
    void BootProfile::OnLeave()
    {
        if (mpStateInterface == 0)
            return;
        if (mpProfileManager != 0)   // X360 unconditional; PC null-guard
            mpProfileManager->DetachMessageDisplay(mProfileMessage);
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        // ARTIST 0x82478558: the empty-name sentinel targets the same display level
        // used by SaveLoadComponent (level 3), so that prompt is gone before the
        // post-title intro state can begin.
        GuiAptNameFlagEvent20 lUnload("", 3);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lUnload), KI_CHANNEL_VIEW_STATE,
            static_cast<s32>(sizeof(GuiAptNameFlagEvent20)));
        if (!mbCheckDiskSpace)
        {
            if (mpGuiCache != 0)
                mpGuiCache->UnloadResources(maResourcesToLoad, muNumResourcesToLoad);
            if (mpGuiCache == 0 || !mpGuiCache->IsLoadingScreenVisible())
                PostCommand16<507>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        }
        else
        {
            mbCheckDiskSpace = false;
        }
    }

    // @ 0x82478610 -- accept/back presses drive the profile message choice through the
    // manager (accept with a live prompt also fires the "Accept" audio trigger):
    // accept -> option 0 on an ok prompt / option 1 on an ok-cancel prompt; back ->
    // option 0 on an ok-cancel prompt. PC fallback: with no manager wired, an accept at
    // RUNNING resolves through the manager's task callback.
    void BootProfile::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        const s32 liSubId =
            reinterpret_cast<const ControllerInputPressedPayload*>(lpEvent)->miButtonId;
        if (liSubId == KI_ACTION_ACCEPT || liSubId == KI_ACTION_ACCEPT_PC)
        {
            const s32 liNumOptions = mProfileMessage.GetNumOptions();
            if (liNumOptions >= 1)
            {
                // X360: GuiAudioTriggerEvent::Construct(7, "", "Accept") + OutputGuiEvent.
                BrnGui::GuiAudioTriggerEvent lAudio;
                lAudio.Construct(7, "", "Accept", "");
                mpStateInterface->OutputGuiEvent<BrnGui::GuiAudioTriggerEvent>(lAudio);
            }

            if (liNumOptions == 1)
            {
                if (mpProfileManager != 0)
                    mpProfileManager->HandleMessageChoice(0);
            }
            else if (liNumOptions == 2)
            {
                if (mpProfileManager != 0)
                    mpProfileManager->HandleMessageChoice(1);
            }
        }
        else if (liSubId == KI_ACTION_BACK && mProfileMessage.GetNumOptions() == 2)
        {
            if (mpProfileManager != 0)
                mpProfileManager->HandleMessageChoice(0);
        }
    }

    // Stage-transition log (the [BootLegal]-style diagnostic every boot state carries).
    static void LogProfileStage(s32 liFrom, s32 liTo)
    {
        char lac[64];
        std::snprintf(lac, sizeof(lac), "[BootProfile] stage %d -> %d\n", liFrom, liTo);
        CgsDev::Log::WriteToLog(lac);
    }

    // @ 0x82474468 (cpp:584) -- ProfileTaskResultHandler override: the manager's
    // ReportTaskCompleted lands here when the boot-up (or re-entry disk-space) task
    // resolves. Un-silence the manager and advance to LEAVING.
    void BootProfile::HandleProfileTaskResult()
    {
        if (mpProfileManager != 0)   // only the manager calls here, but keep the PC guard
        {
            mpProfileManager->SetSilentMode(false);
        }
        LogProfileStage(meInternalState, E_INTERNALSTATE_LEAVING);
        meInternalState = E_INTERNALSTATE_LEAVING;
    }

    // @ 0x8247E500 -- the profile stage machine (see the header's InternalState).
    void BootProfile::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;
        const InternalState lePrevState = meInternalState;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        switch (meInternalState)
        {
        case E_INTERNALSTATE_GETCACHE:
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId == KI_EVENT_GUI_CACHE)
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache pointer");   // cpp:351
                    mpGuiCache = lpCache;
                    if (mbCheckDiskSpace && mpProfileManager != 0)
                        mpProfileManager->CheckDiskSpace(this);
                    meInternalState = E_INTERNALSTATE_WAIT_RESOURCES;
                }
            }
            lpInQueue->Clear();
            break;

        case E_INTERNALSTATE_WAIT_RESOURCES:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");   // cpp:376
            if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            {
                lpInQueue->Clear();
                break;
            }
            // X360 passes flow 0 to both watcher calls.
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mProfileMessage.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            meInternalState = E_INTERNALSTATE_WAIT_FOR_MESSAGE;
            lpInQueue->Clear();
            break;

        case E_INTERNALSTATE_WAIT_FOR_MESSAGE:
            // Disk-space re-entry parks here until the manager's no-space prompt
            // arrives (the CheckDiskSpace task's ShowNoSpaceMessage), so the reloaded
            // movie has a message to replay.
            if (mbCheckDiskSpace && !mProfileMessage.HasMessage())
            {
                lpInQueue->Clear();
                break;
            }
            // Post the render-mode command 138 (the game bridges it to the dispatch
            // buffer's render state) and play the save/load prompt at display level 3.
            PostCommand16<138>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            mpStateInterface->PlayAptMovie("SaveLoadComponent", 3);
            meInternalState = E_INTERNALSTATE_WAIT_INITIALISE;
            lpInQueue->Clear();
            break;

        case E_INTERNALSTATE_WAIT_INITIALISE:
            CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:410
            if (!CacheAreAllAptComponentsInitialised(mpGuiCache))
            {
                lpInQueue->Clear();
                break;
            }
            if (mbCheckDiskSpace)
            {
                mProfileMessage.ResendMessageToApt();
            }
            else if (mpProfileManager != 0)   // X360 unconditional; PC null-guard
            {
                // The boot-up task (auto-load the profile as part of boot-up). The
                // completion comes back through HandleProfileTaskResult.
                mpProfileManager->Bootup(*this, true);
            }
            meInternalState = E_INTERNALSTATE_RUNNING;
            lpInQueue->Clear();
            break;

        case E_INTERNALSTATE_RUNNING:
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId == KI_EVENT_CONTROLLER)
                    HandleControllerInput(lpEvent);
            }
            lpInQueue->Clear();
            break;

        case E_INTERNALSTATE_LEAVING:
            if (!mbCheckDiskSpace)
                mpStateInterface->PlayLoadingScreen();
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            lpInQueue->Clear();
            break;

        default:
            CGS_ASSERT(false, "Invalid internal state");   // cpp:464
            lpInQueue->Clear();
            break;
        }

        if (meInternalState != lePrevState)
            LogProfileStage(lePrevState, meInternalState);
    }
}
