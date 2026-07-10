#include "GameSource/Gui/Flow/HUD/States/BrnBootProfile.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (stage log)
#include "GameSource/Gui/BrnGuiAptRuntime.h"                              // gpActiveAptRuntimeHost (component-init gate boundary)

#include <cstdio>   // std::snprintf (the stage log)

// BrnGui::BootProfile -- reconstructed from BURNOUT_X360_ARTIST.XEX
//   (OnEnter @0x82478440, OnLeave @0x824784F8, Update @0x8247E500,
//    HandleControllerInput @0x82478610; GetResourcesToLoad inline in the header).
//
// BF_PROFILE: after the title screen accepts, this state plays the save/load prompt
// apt movie "SaveLoadComponent" at display level 3, waits for its apt components,
// drives the profile manager's boot-up + message choices, and signals phase-complete
// (command 70 + the loading screen) once the profile task resolves.
//
// PROFILE-MANAGER BOUNDARY (FLAG): the BrnGui::ProfileManager + ProfileMessageComponent
// subsystem (device selection, sign-in tasks) is not reconstructed on PC, so their calls
// are gated on a null mpProfileManager. Where the console advances INTERACT ->
// SIGNAL_DONE from the profile task result (HandleProfileTaskResult @0x82474468), the
// PC boundary advances on the accept press -- the same user-visible observable
// (prompt shown -> accept -> loading screen + proceed) -- until ProfileManager lands.
namespace BrnGui
{
    namespace
    {
        const s32 KI_EVENT_CONTROLLER     = 6;    // controller action (sub-id @+4)
        const s32 KI_EVENT_GUI_CACHE      = 64;   // per-frame cache event (GuiCache* payload)
        const s32 KI_EVENT_OVERLAY_RESULT = 189;  // overlay result (the profile message choices)

        const s32 KI_ACTION_ACCEPT = 49;   // X360 HandleControllerInput accept sub-id
        const s32 KI_ACTION_BACK   = 50;   // X360 HandleControllerInput back sub-id
        // The PC controller chain's accept observable: BridgeControllerToGui synthesises
        // 6/45 for the accept press (the real 0x820352F0/0x82035330 binding tables).
        const s32 KI_ACTION_ACCEPT_PC = 45;

        const s32 KI_CHANNEL_GUI_OUT    = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;  // GuiOutViewState

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
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

        // ---- ProfileManager / ProfileMessageComponent boundary (FLAG: unreconstructed;
        //      each call is a no-op while mpProfileManager is null) ----------------------
        // FLAG PC-platform leaf: ProfileManager (Xbox profiles/save devices) is a console
        // subsystem boundary; no-op until its PC reconstruction lands.
        void ProfileManager_CheckDiskSpace(ProfileManager* /*lpManager*/) {}
        // FLAG PC-platform leaf: ProfileManager boundary (see above).
        void ProfileManager_Bootup(ProfileManager* /*lpManager*/)         {}
        // FLAG PC-platform leaf: ProfileManager boundary (see above).
        void ProfileManager_HandleMessageChoice(ProfileManager* /*lpManager*/, s32 /*liChoice*/) {}

        // ---- GuiCache boundary (same FLAG'd discipline as BrnBootLegalBoundary.cpp) ----
        bool CacheResourcesReady(GuiCache* lpCache)                 { return lpCache != 0; }
        void CacheClearExpectedAptComponentList(GuiCache* /*c*/)    {}
        void CacheAppendProfileMessageComponent(GuiCache* /*c*/)    {}
        bool CacheAreAllAptComponentsInitialised(GuiCache* lpCache)
        {
            // A component reports initialised only once its clip is PLACED; gate on the
            // Apt host being up so the SaveLoadComponent request has a live engine to
            // land in (the per-movie composed handshake is the follow-on).
            return lpCache != 0 && BrnGui::gpActiveAptRuntimeHost != 0 &&
                   BrnGui::gpActiveAptRuntimeHost->IsReady();
        }
    }

    // @ 0x8205AB98 (.rdata, no exported value): the 3 observed ids, pinned from the
    // Update/HandleControllerInput dispatch (64 at WAIT_CACHE, 6 at INTERACT) and the
    // profile message choices' overlay-result feedback.
    const s32 BootProfile::maiEventToObserve[] =
        { KI_EVENT_GUI_CACHE, KI_EVENT_CONTROLLER, KI_EVENT_OVERLAY_RESULT };
    const s32 BootProfile::miNumEventsObserved = 3;

    // FLAG (unrecovered .rdata @0x82F25F78/0x82F25F80): the profile screen's static
    // resource-tuple table carries no exported values; empty until recovered (the PC
    // cache boundary treats boot resources as resident).
    const CgsGui::sResourceTuple BootProfile::maResourcesToLoad[1] = { { 0u, CgsGui::E_GUI_RESOURCETYPE_START } };
    const u32 BootProfile::muNumResourcesToLoad = 0;

    // @ 0x82478440 -- seed WAIT_CACHE, register the observed set, attach the profile
    // message display, and post the view-option record { 8, 25, 12, 1, 1.0f }.
    void BootProfile::OnEnter()
    {
        meInternalState = E_PROFILE_WAIT_CACHE;
        mpGuiCache = 0;
        miMessageChoiceMode = 0;
        mbCheckDiskSpaceMode = false;
        mpProfileManager = 0;   // FLAG: threaded by the DWARF 3-arg Construct once ProfileManager lands
        if (mpStateInterface != 0)
            mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        // ProfileMessageComponent::Construct + ProfileManager::AttachMessageDisplay --
        // FLAG boundary (see the header note); no-ops while the manager is null.
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
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        // X360 size 20 with 4-byte pointers; sizeof on the x64 gate (the pointer widens).
        GuiAptNameFlagEvent20 lUnload("", 1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lUnload), KI_CHANNEL_VIEW_STATE,
            static_cast<s32>(sizeof(GuiAptNameFlagEvent20)));
        if (!mbCheckDiskSpaceMode)
        {
            // GuiCache::UnloadResources(maResourcesToLoad) -- boundary no-op (resident
            // synchronous PC loads), then the player-name refresh command (507).
            PostCommand16<507>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        }
        else
        {
            mbCheckDiskSpaceMode = false;
        }
    }

    // @ 0x82478610 -- accept/back presses drive the profile message choice; an accept
    // with a live message also fires the "Accept" audio trigger. PC boundary: with no
    // profile manager, an accept at INTERACT resolves the (absent) profile task, so the
    // flow advances to SIGNAL_DONE -- the console's HandleProfileTaskResult observable.
    void BootProfile::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        const s32 liSubId = *reinterpret_cast<const s32*>(
            reinterpret_cast<const char*>(lpEvent) + 4);
        if (liSubId == KI_ACTION_ACCEPT || liSubId == KI_ACTION_ACCEPT_PC)
        {
            if (miMessageChoiceMode >= 1)
            {
                // GuiAudioTriggerEvent(7, "", "Accept") -> the accept blip. The trigger
                // event's full 112-byte record is posted by the component path on PC;
                // the profile message flow is manager-gated. [FLAG boundary]
                ProfileManager_HandleMessageChoice(mpProfileManager,
                                                   miMessageChoiceMode == 2 ? 1 : 0);
            }
            else if (mpProfileManager == 0 && meInternalState == E_PROFILE_INTERACT)
            {
                // FLAG PC profile boundary: no ProfileManager -> the accept press IS the
                // profile-task resolution (HandleProfileTaskResult's advance).
                meInternalState = E_PROFILE_SIGNAL_DONE;
            }
        }
        else if (liSubId == KI_ACTION_BACK && miMessageChoiceMode == 2)
        {
            ProfileManager_HandleMessageChoice(mpProfileManager, 0);
        }
    }

    // Stage-transition log (the [BootLegal]-style diagnostic every boot state carries).
    static void LogProfileStage(s32 liFrom, s32 liTo)
    {
        char lac[64];
        std::snprintf(lac, sizeof(lac), "[BootProfile] stage %d -> %d\n", liFrom, liTo);
        CgsDev::Log::WriteToLog(lac);
    }

    // @ 0x8247E500 -- the profile stage machine (see the header's EInternalState).
    void BootProfile::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;
        const EInternalState lePrevState = meInternalState;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        switch (meInternalState)
        {
        case E_PROFILE_WAIT_CACHE:
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId == KI_EVENT_GUI_CACHE)
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache pointer");
                    mpGuiCache = lpCache;
                    if (mbCheckDiskSpaceMode)
                        ProfileManager_CheckDiskSpace(mpProfileManager);
                    meInternalState = E_PROFILE_WAIT_RESOURCES;
                }
            }
            lpInQueue->Clear();
            break;

        case E_PROFILE_WAIT_RESOURCES:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");
            if (!CacheResourcesReady(mpGuiCache))
            {
                lpInQueue->Clear();
                break;
            }
            CacheClearExpectedAptComponentList(mpGuiCache);
            CacheAppendProfileMessageComponent(mpGuiCache);
            meInternalState = E_PROFILE_SHOW_SAVELOAD;
            lpInQueue->Clear();
            break;

        case E_PROFILE_SHOW_SAVELOAD:
            // Post the render-mode command 138 (the game bridges it to the dispatch
            // buffer's render state) and play the save/load prompt at display level 3.
            PostCommand16<138>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            mpStateInterface->PlayAptMovie("SaveLoadComponent", 3);
            meInternalState = E_PROFILE_WAIT_COMPONENTS;
            lpInQueue->Clear();
            break;

        case E_PROFILE_WAIT_COMPONENTS:
            CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");
            if (!CacheAreAllAptComponentsInitialised(mpGuiCache))
            {
                lpInQueue->Clear();
                break;
            }
            ProfileManager_Bootup(mpProfileManager);
            meInternalState = E_PROFILE_INTERACT;
            lpInQueue->Clear();
            break;

        case E_PROFILE_INTERACT:
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId == KI_EVENT_CONTROLLER)
                    HandleControllerInput(lpEvent);
            }
            lpInQueue->Clear();
            break;

        case E_PROFILE_SIGNAL_DONE:
            if (!mbCheckDiskSpaceMode)
                mpStateInterface->PlayLoadingScreen();
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            lpInQueue->Clear();
            break;

        default:
            CGS_ASSERT(false, "Invalid internal state");
            lpInQueue->Clear();
            break;
        }

        if (meInternalState != lePrevState)
            LogProfileStage(lePrevState, meInternalState);
    }
}
