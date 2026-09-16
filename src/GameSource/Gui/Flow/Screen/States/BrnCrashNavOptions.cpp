// ===================================================================================
// BrnGui::CrashNavOptionsData  -- the CrashNav "options" screen data model
//   class:BrnGui::CrashNavOptionsData
//
//   SetFromProfile @ 0x82482A28
//   SetToProfile   @ 0x82482B10
//   OutputEvents   @ 0x82494278
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavOptions.h"
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"
#include "GameSource/Gui/BrnGuiShared.h"                                   // gGuiResourceIdentifier (the apt movie name)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                            // GuiOverlayRequest

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // getenv ([opt])
#include <cstdlib>                                           // getenv ([opt])

namespace BrnGui
{
    // @ 0x82482A28 -- pull the whole option model out of the persisted profile block.
    // Asserts the profile non-null (non-gating), then copies field by field via the
    // profile getters, in the X360 call order (each getter self-asserts !mbIsLocked).
    void CrashNavOptionsData::SetFromProfile(OptionsDataProfile* lpProfile)
    {
        CGS_ASSERT(lpProfile != nullptr, "lpOptionsData");

        meCameraUserOption = static_cast<s32>(lpProfile->GetCameraFeed());
        meVoipVolume       = static_cast<EOptionsVoipVolumes>(lpProfile->GetVoipVolume());
        meMusicVolume      = static_cast<EOptionsSoundVolumes>(lpProfile->GetMusicVolume());
        meSFXVolume        = static_cast<EOptionsSoundVolumes>(lpProfile->GetSFXVolume());
        mbSixAxisShowtime  = lpProfile->GetSixAxisShowtime();
        mbSixAxisSteering  = lpProfile->GetSixAxisSteering();
        mbForceFeedback    = lpProfile->GetForceFeedback();
        mbDefaultGameCamera = lpProfile->GetDefaultGameCamera();
        mbTips             = lpProfile->GetTips();
    }

    // @ 0x82482B10 -- write the whole option model back into the persisted profile block.
    // Asserts the profile non-null (non-gating), then pushes field by field via the
    // profile setters, in the X360 call order (DefaultGameCamera loaded before Tips).
    void CrashNavOptionsData::SetToProfile(OptionsDataProfile* lpProfile)
    {
        CGS_ASSERT(lpProfile != nullptr, "lpOptionsData");

        lpProfile->SetCameraFeed(
            static_cast<BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions>(meCameraUserOption));
        lpProfile->SetVoipVolume(static_cast<s32>(meVoipVolume));
        lpProfile->SetMusicVolume(static_cast<s32>(meMusicVolume));
        lpProfile->SetSFXVolume(static_cast<s32>(meSFXVolume));
        lpProfile->SetSixAxisShowtime(mbSixAxisShowtime);
        lpProfile->SetSixAxisSteering(mbSixAxisSteering);
        lpProfile->SetForceFeedback(mbForceFeedback);
        lpProfile->SetDefaultGameCamera(mbDefaultGameCamera);
        lpProfile->SetTips(mbTips);
    }

    // @ 0x82494278 -- publish the option model as a burst of GuiEvent records onto the
    // state's large output queue (channel id 40 = GuiEventOut). Store-for-store faithful
    // to the X360: seven records in asm order. meVoipVolume (this+0x04) is deliberately
    // NOT emitted; the audio-volumes record (type 463) is pushed twice with a byte-
    // identical {music,sfx} payload exactly as the X360 does. The default-game-camera
    // value is the RAW PROFILE word at +0x734C (asm: a bare `lwz r11, 0x734C(r28)` --
    // NO getter call, NO lock assert, NO ==1 normalization).
    void CrashNavOptionsData::OutputEvents(OptionsDataProfile* lpProfile,
                                           CgsGui::StateInterface* lpStateInterface)
    {
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpQueue =
            lpStateInterface->GetOutputEventQueue();

        // (1) camera-user-option (type 278) -- payload = meCameraUserOption (this+0x00).
        {
            GuiEventCrashNavCameraUserOption lEvent;
            lEvent.miCameraUserOption = meCameraUserOption;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (2) audio volumes (type 463) -- payload = { meMusicVolume(+0x08), meSFXVolume(+0x0C) }.
        {
            GuiEventCrashNavAudioVolumes lEvent;
            lEvent.miMusicVolume = static_cast<s32>(meMusicVolume);
            lEvent.miSFXVolume   = static_cast<s32>(meSFXVolume);
            lpQueue->AddEvent(&lEvent, 40, 20);

            // (3) the X360 pushes the same {music,sfx} record a second time, byte-identical.
            lpQueue->AddEvent(&lEvent, 40, 20);
        }

        // (4) controller toggles (type 472) -- payload = { showtime(+0x10), steering(+0x11), force(+0x12) }.
        {
            GuiEventCrashNavControllerToggles lEvent;
            lEvent.mbSixAxisShowtime = mbSixAxisShowtime;
            lEvent.mbSixAxisSteering = mbSixAxisSteering;
            lEvent.mbForceFeedback   = mbForceFeedback;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (5) default-game-camera (type 475) -- payload = the RAW PROFILE word at +0x734C.
        // The X360 emits a plain `lwz r11, 0x734C(r28)` here: the raw 32-bit flag word,
        // NOT OptionsDataProfile::GetDefaultGameCamera() (which would assert !mbIsLocked and
        // normalize to `word == 1`). Read the modelled member's word directly to match.
        {
            GuiEventCrashNavDefaultGameCamera lEvent;
            lEvent.miDefaultGameCamera = *reinterpret_cast<const s32*>(
                reinterpret_cast<const u8*>(lpProfile) + 0x734C);   // profile mbDefaultGameCamera word
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (6) tips (type 473) -- payload byte = mbTips (this+0x13).
        {
            GuiEventCrashNavTips lEvent;
            lEvent.mbTips = mbTips;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (7) commit marker (type 356) -- payload byte 0.
        {
            GuiEventCrashNavCommit lEvent;
            lEvent.mbFlag = 0;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }
    }

    // ===================================================================================
    // BrnGui::CrashNavOptions -- THE CRASH-NAV "OPTIONS" TAB.
    //
    // ⭐⭐ WHY THIS SLICE EXISTS. Until now the class was a header-only SHELL: it declared
    // GetResourcesToLoad and SetSettingsFromProfile and nothing else, so OnEnter / Update /
    // OnLeave fell through to the do-nothing CgsGui::State base. A state that registers for
    // no events receives none, so the tab could not be navigated, no option could be
    // changed, and -- the owner's report -- nothing it showed was ever applied or SAVED.
    // The console's save path is the ACCEPT arm below: ApplyAndSaveSettings ->
    // CrashNavOptionsData::SetToProfile (persist) + ::OutputEvents (publish). Both of those
    // already had bodies in this TU; nothing ever called them.
    // This is the same defect CrashNavSettings was fixed out of -- see that header's banner.
    //
    // Bodies reconstructed from BURNOUT_X360_ARTIST.XEX:
    //   CrashNavOptions             @0x82508AA0    OnEnter            @0x824BCEE8
    //   OnLeave                     @0x824CDAD0    Update             @0x824E0BD8
    //   UpdateInitSetup             @0x824C18B8    UpdateLoading      @0x824CDC00
    //   UpdateWFInit                @0x824C19B8    UpdatePermanent    @0x824DFF90
    //   HandleControllerInput       @0x824DE418    HandleTriggers     @0x824B7F90
    //   HandleOverlayCompleteEvent  @0x824D95F0    HandleOptionChanged@0x824D9408
    //   ApplyAndSaveSettings        @0x824CDD00    UpdateSoundSettings@0x824CDF10
    //   RestoreSoundSettings        @0x824CDDA0    RestoreVoipSettings@0x824CDE68
    //   StateCancelFlow             @0x824CE0C0    TriggerSound       @0x824CDFB0
    //   CrashNavOptionsData::SetUpComponent<4> @0x824C0210
    // ===================================================================================
    namespace
    {
        // The four ids this state observes (X360 .rdata @0x82066440, `li r5, 4`).
        // 64 == the GuiCache publish, 6 == controller input, 21 == apt trigger,
        // 189 == overlay complete -- the four UpdateInitSetup/UpdatePermanent switch on.
        const s32 KAI_EVENTS_TO_OBSERVE[] = { 64, 6, 21, 189 };
        const s32 KI_NUM_EVENTS_OBSERVED = 4;

        const s32 KI_EVENT_GUI_CACHE        = 64;
        const s32 KI_EVENT_CONTROLLER_INPUT = 6;
        const s32 KI_EVENT_APT_TRIGGER      = 21;
        const s32 KI_EVENT_OVERLAY_COMPLETE = 189;

        // EGameInputActions, exactly the six arms the X360 switch has.
        const s32 KI_ACTION_TOGGLE_PREV = 41;   // 0x29 -- previous option ROW
        const s32 KI_ACTION_TOGGLE_NEXT = 42;   // 0x2A -- next option ROW
        const s32 KI_ACTION_OPTION_PREV = 43;   // 0x2B -- previous OPTION within the row
        const s32 KI_ACTION_OPTION_NEXT = 44;   // 0x2C -- next OPTION within the row
        const s32 KI_ACTION_ACCEPT      = 49;   // 0x31 -- GUI_SELECT: apply + save + leave
        const s32 KI_ACTION_BACK        = 50;   // 0x32 -- GUI_CANCEL: confirm, then discard

        const s32 KI_CHANNEL_GUI_OUT  = 40;
        const s32 KI_CHANNEL_GUI_VIEW = 41;

        // The confirm-discard overlay the CANCEL arm raises when something was edited.
        const char* const KAC_CANCEL_CONFIRM_OVERLAY = "CNCnclCnfrm";
        const char* const KAC_OPTION_TOGGLE_NAME     = "optionToggle";
        const char* const KAC_HELP_ITEM_NAME         = "HelpItemAccept_mc";
        const char* const KAC_EMPTY                  = "";
        const s32         KI_APT_DISPLAY_LEVEL       = 3;

        // type 474 (0x1DA), record 16: the VOIP volume restored from the profile.
        // (The five records CrashNavOptionsData::OutputEvents posts live in the header;
        // this one is only ever posted by RestoreVoipSettings, so it homes here.)
        struct GuiEventCrashNavVoipVolume : public CgsGui::GuiEvent<474>
        {
            s32 miVoipVolume;   // +0x0C
            GuiEventCrashNavVoipVolume() : CgsGui::GuiEvent<474>(4, 12) {}
        };

        // X360 `li r8, -1 ; clrldi r8, r8, 32` -- Selectable::K_INVALID_ID.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // The screen's one resource tuple is { id 0x8D, type 4 } (see the embed-check TU),
        // and UpdateLoading plays that same identifier's movie.
        const s32 KI_RESOURCE_ID_OPTIONS = 0x8D;

        // The state's in-event queue (CgsGui::State +0x18) is the DWARF
        // InputBuffer::GuiEventQueue, an incomplete alias for the concrete instantiation.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The event-64 payload view (same local view the sibling CrashNav TUs carry; the
        // type has no committed home yet).
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpCachePointer;
        };

        // The event-6 payload view: the action sub-id rides in the payload's +4 word.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // The event-189 payload view. HandleOverlayCompleteEvent @0x824D95F0 reads the
        // overlay id at +0x04 and the answer at +0x08 (`lwz r11, 4(a2)` / `lwz r?, 8(a2)`).
        struct GuiOverlayCompletePayload : public CgsModule::Event
        {
            s32 miPad;        // +0x00
            u32 muOverlayId;  // +0x04 (a compressed CgsID)
            s32 miResult;     // +0x08 (1 == the positive answer)
        };
    }

    // ---- CrashNavOptions @0x82508AA0 -----------------------------------------------
    // The X360 body is vptr installs plus the embedded components' own constructors, all
    // of which C++ emits implicitly here. The scalar fields are seeded by OnEnter.
    CrashNavOptions::CrashNavOptions()
        : meState(E_STATE_INIT_SETUP)
        , mpGuiCache(0)
        , mMenuToggleGroup()
        , mOptionsData()
        , mHelpItem()
        , mbSettingsChanged(false)
        , mbInputHandled(false)
    {
    }

    // ---- OnEnter @0x824BCEE8 -------------------------------------------------------
    void CrashNavOptions::OnEnter()
    {
        // ⭐ REGISTER FIRST. Without this the state observes nothing, receives nothing, and
        // every arm below is dead code -- which is precisely the state this tab was in.
        mpStateInterface->RegisterForEvents(KAI_EVENTS_TO_OBSERVE, KI_NUM_EVENTS_OBSERVED);

        meState    = E_STATE_INIT_SETUP;   // `stw r11, 0x38`
        mpGuiCache = 0;                    // `stw r11, 0x3C`

        // The model's entry defaults. The X360 writes EXACTLY these seven fields inline
        // (this+0x3CE0 = 0, +0x3CE4/+0x3CE8/+0x3CEC = 8, +0x3CF0 = 0, +0x3CF1 = 1,
        // +0x3CF3 = 1); it does NOT touch mbForceFeedback (+0x3CF2) or mbDefaultGameCamera
        // (+0x3CF4) here. They are overwritten by SetSettingsFromProfile in UpdateWFInit;
        // the console seeds them anyway so a cache that never arrives still leaves a
        // defined model.
        mOptionsData.SetCameraUserOptions(0);
        mOptionsData.SetVoipVolume(CrashNavOptionsData::E_OPTION_VOIP_VOLUMES_8);
        mOptionsData.SetMusicVolume(CrashNavOptionsData::E_OPTION_SOUND_VOLUMES_8);
        mOptionsData.SetSFXVolume(CrashNavOptionsData::E_OPTION_SOUND_VOLUMES_8);
        mOptionsData.SetSixAxisShowtime(false);
        mOptionsData.SetSixAxisSteering(true);
        mOptionsData.SetTips(true);

        mMenuToggleGroup.Construct(KAC_OPTION_TOGGLE_NAME, mpStateInterface,
                                   E_OPTIONROW_COUNT, 0, KU_INVALID_APT_ID);
        mMenuToggleGroup.SetupGroup(E_OPTIONROW_COUNT, 0);

        mHelpItem.Construct(KAC_HELP_ITEM_NAME, mpStateInterface, 0);

        mbSettingsChanged = false;         // `stb r11, 0x3EA4`
    }

    // ---- OnLeave @0x824CDAD0 -------------------------------------------------------
    void CrashNavOptions::OnLeave()
    {
        mMenuToggleGroup.Clear();          // component vtable slot 6 (`lwz r11, 0x18(vtbl)`)

        meState = E_STATE_LEAVING;         // `stw r11, 0x38` == 4

        // Symmetric with OnEnter: the observer table is only KI_MAX_OBSERVERS wide, so a
        // state that registers on every entry without releasing runs it out.
        mpStateInterface->UnRegisterForEvents(KAI_EVENTS_TO_OBSERVE, KI_NUM_EVENTS_OBSERVED);

        // The apt UNLOAD sentinel: the EMPTY movie name at the display level.
        mpStateInterface->PlayAptMovie(KAC_EMPTY, KI_APT_DISPLAY_LEVEL);

        // [FLAG] The X360 tail also posts a type-460 GuiEventAudioTraxPreview (20-byte
        // record, 8-byte payload) to stop any preview this tab started. Its two payload
        // words are built in a REUSED stack qword whose low half still carries the apt
        // display level from the post above, so the second word cannot be read off this
        // call site with confidence -- and this tree's GuiEventAudioTraxPreview is an
        // opaque `u8 maData[8]` with no field names to settle it against. Posting a
        // half-guessed payload to a live consumer is worse than not posting, so the record
        // is REPORTED here. It only matters once the preview posts on the row-move arms
        // land (same FLAG in HandleControllerInput).

        // The "volume scrubbing finished" cue to the sound module.
        PostAudioTrigger(1, "AdjustGameVolume");
    }

    // ---- Update @0x824E0BD8 --------------------------------------------------------
    // The console's fall-through ladder: each stage runs, and when it reports done the NEXT
    // stage runs in the SAME visit (the `goto LABEL_n` chain), so a fast load settles in one
    // frame. E_STATE_LEAVING is the only stage that suppresses UpdatePermanent.
    void CrashNavOptions::Update()
    {
        bool lbRunPermanent = true;

        switch (meState)
        {
        case E_STATE_INIT_SETUP:
            meState = E_STATE_INIT_SETUP;
            if (!UpdateInitSetup())
            {
                break;
            }
            // fall through
        case E_STATE_LOADING:
            meState = E_STATE_LOADING;
            if (!UpdateLoading())
            {
                break;
            }
            // fall through
        case E_STATE_WF_INIT:
            meState = E_STATE_WF_INIT;
            if (!UpdateWFInit())
            {
                break;
            }
            // fall through
        case E_STATE_MAIN:
            meState = E_STATE_MAIN;
            break;

        case E_STATE_LEAVING:
            lbRunPermanent = false;
            meState = E_STATE_LEAVING;
            break;

        default:
            CGS_ASSERT(false, "Invalid internal state (");   // cpp:241
            break;
        }

        // [DIAG] NOT IN THE X360 BINARY -- [opt] the ladder stage, once per change.
        // Function-local because there is exactly one CrashNavOptions instance (the
        // pool state ScreenFlow constructs) and the header's layout is pinned.
        {
            static s32 siDiagLastStage = -1;
            if (static_cast<s32>(meState) != siDiagLastStage &&
                getenv("BRN_OPTIONS_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[opt] stage " << siDiagLastStage
                    << " -> " << static_cast<s32>(meState)
                    << " (0 initsetup 1 loading 2 wfinit 3 main 4 leaving)\n";
            }
            siDiagLastStage = static_cast<s32>(meState);
        }

        if (lbRunPermanent)
        {
            UpdatePermanent();
        }

        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ---- UpdateInitSetup @0x824C18B8 -----------------------------------------------
    // Walk this frame's inbound events for the GuiCache publish and latch it. Returns true
    // once the cache is in hand, which is what advances the ladder.
    bool CrashNavOptions::UpdateInitSetup()
    {
        bool lbGotCache = false;

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCache* lpCacheEvent =
                    reinterpret_cast<const GuiEventCache*>(lpEvent);
                CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                           "Invalid cache in CrashNavOptions::OnEnter");   // cpp:320
                lbGotCache = true;
                mpGuiCache = lpCacheEvent->mpCachePointer;
            }
        }

        return lbGotCache;
    }

    // ---- UpdateLoading @0x824CDC00 -------------------------------------------------
    // Hold until the screen's resources are resident, then DECLARE the apt components this
    // tab expects and ask the view to play the movie. Returns true once declared.
    bool CrashNavOptions::UpdateLoading()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:347

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mMenuToggleGroup.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_OPTIONS],
                                       KI_APT_DISPLAY_LEVEL);
        return true;
    }

    // ---- UpdateWFInit @0x824C19B8 --------------------------------------------------
    // Wait for those components to initialise, then fill the model from the profile and
    // build the four rows. This is the first frame the tab can be read or driven.
    bool CrashNavOptions::UpdateWFInit()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:385

        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        SetSettingsFromProfile();
        mOptionsData.SetUpComponent<E_OPTIONROW_COUNT>(&mMenuToggleGroup);
        return true;
    }

    // ---- UpdatePermanent @0x824DFF90 -----------------------------------------------
    // Runs in every stage but LEAVING: drain the events, tick the group, and keep the help
    // prompt honest -- ACCEPT is only offered once something has actually been edited.
    void CrashNavOptions::UpdatePermanent()
    {
        mbInputHandled = false;

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                // One consumer per frame: the console latches the first arm that claims it.
                if (!mbInputHandled)
                {
                    mbInputHandled = HandleControllerInput(lpEvent);
                }
                break;

            case KI_EVENT_APT_TRIGGER:
                HandleTriggers(lpEvent);
                break;

            case KI_EVENT_OVERLAY_COMPLETE:
                HandleOverlayCompleteEvent(lpEvent);
                break;

            default:
                break;
            }
        }

        mMenuToggleGroup.Update();   // group vtable slot 5 (`lwz r11, 0x14(vtbl)`)

        if (mbSettingsChanged)
        {
            mHelpItem.SetItem("$CAPS_BUTTON_ACCEPT",
                              ButtonIconComponent::E_PADBUTTON_SELECT,      // X360 `li r5, 4`
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);  // X360 `li r6, 15`
        }
        else
        {
            mHelpItem.SetItem(KAC_EMPTY,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE,   // X360 `li r5, 15`
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
    }

    // ---- HandleControllerInput @0x824DE418 -----------------------------------------
    // Returns true when this state consumed the press (UpdatePermanent's one-per-frame
    // latch). Every navigation arm is gated on the tab having settled (meState >= MAIN)
    // AND on the group actually moving -- a press that changes nothing is not consumed.
    bool CrashNavOptions::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavOptions::HandleControllerInput");   // cpp:509

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_TOGGLE_PREV:
            if (meState < E_STATE_MAIN || !mMenuToggleGroup.HighlightPrevious(false))
            {
                return true;
            }
            TriggerSound(lpInput->miButtonId);
            // [FLAG] The X360 row-move arms also drive the music preview as the cursor
            // enters/leaves the two volume rows: landing on MUSIC_VOLUME posts
            // GuiEventAudioTraxPreview { 57, 1 }, leaving it posts { -1, 0 }, and landing
            // on SFX_VOLUME posts a 112-byte BrnGui::GuiAudioTriggerEvent whose TYPE WORD
            // IS 457 while this tree's GuiAudioTriggerEvent derives from GuiEvent<201>.
            // The preview pair needs the meaning of the constant 57 (a track slot? a
            // preview mode?) and the trigger post needs the type-id discrepancy settled by
            // the record's consumer. Neither is recoverable from this call site, so both
            // are REPORTED rather than guessed -- they are audition feedback while the
            // cursor moves, not part of navigation, applying or saving.
            return true;

        case KI_ACTION_TOGGLE_NEXT:
            if (meState < E_STATE_MAIN || !mMenuToggleGroup.HighlightNext(false))
            {
                return true;
            }
            TriggerSound(lpInput->miButtonId);
            // [FLAG] see the preview/trigger note on the TOGGLE_PREV arm above.
            return true;

        case KI_ACTION_OPTION_PREV:
            if (meState < E_STATE_MAIN || !mMenuToggleGroup.HighlightPreviousItem())
            {
                return true;
            }
            HandleOptionChanged();
            TriggerSound(lpInput->miButtonId);
            return true;

        case KI_ACTION_OPTION_NEXT:
            if (meState < E_STATE_MAIN || !mMenuToggleGroup.HighlightNextItem())
            {
                return true;
            }
            HandleOptionChanged();
            TriggerSound(lpInput->miButtonId);
            return true;

        case KI_ACTION_ACCEPT:
            // ⭐ THE SAVE. Persist the model into the profile block and publish it, then
            // tear the group down and leave the tab.
            ApplyAndSaveSettings();
            mMenuToggleGroup.Unloaded();
            SendStateEvent("GO_BACK");
            return true;

        case KI_ACTION_BACK:
            // Discarding edited settings asks first; an untouched tab just leaves.
            if (mbSettingsChanged)
            {
                GuiOverlayRequest lRequest;
                lRequest.Construct(KAC_CANCEL_CONFIRM_OVERLAY);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest),
                    lRequest.GetEventType(), static_cast<s32>(sizeof(lRequest)));
            }
            else
            {
                StateCancelFlow();
            }
            return true;

        default:
            return false;
        }
    }

    // ---- HandleTriggers @0x824B7F90 ------------------------------------------------
    // The X360 body is the argument assert and nothing else: this screen's apt triggers
    // carry no arm. Kept so the dispatcher's case is faithful rather than absent.
    void CrashNavOptions::HandleTriggers(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavOptions::HandleTriggers");   // cpp:706
        (void)lpEvent;
    }

    // ---- HandleOverlayCompleteEvent @0x824D95F0 ------------------------------------
    // The confirm-discard overlay's answer. Only OUR overlay, and only a positive answer,
    // discards: restore the live audio to the profile's values and leave.
    void CrashNavOptions::HandleOverlayCompleteEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpOverlayCompleteEvent");   // cpp:1017

        const GuiOverlayCompletePayload* lpComplete =
            reinterpret_cast<const GuiOverlayCompletePayload*>(lpEvent);

        if (lpComplete->muOverlayId ==
                static_cast<u32>(CgsIDCompress(KAC_CANCEL_CONFIRM_OVERLAY)) &&
            lpComplete->miResult == 1)
        {
            RestoreSoundSettings();
            RestoreVoipSettings();
            mMenuToggleGroup.Unloaded();
            SendStateEvent("GO_BACK");
        }
    }

    // ---- HandleOptionChanged @0x824D9408 -------------------------------------------
    // Copy the highlighted row's selected id into the model. Every arm dirties the screen,
    // and the two volume rows also publish immediately so the change is AUDIBLE while it is
    // being scrubbed -- the value is not committed to the profile until ACCEPT.
    void CrashNavOptions::HandleOptionChanged()
    {
        const s32 liRow = mMenuToggleGroup.GetHighlightedIndex();
        MenuToggle* lpRow = mMenuToggleGroup.GetSelectable(liRow);
        CGS_ASSERT(lpRow != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218

        // The row's own inner group holds the option cursor; GetHighlightedId asserts one
        // is highlighted and returns its id (X360 `GetSelectable(group, row) + 168` then
        // SelectableGroup::GetHighlighted, id at +8).
        const u64 luSelectedId = lpRow->GetHighlightedId();

        switch (liRow)
        {
        case E_OPTIONROW_CAMERA:
            mOptionsData.SetCameraUserOptions(static_cast<s32>(luSelectedId));
            mbSettingsChanged = true;
            break;

        case E_OPTIONROW_MUSIC_VOLUME:
            mOptionsData.SetMusicVolume(
                static_cast<CrashNavOptionsData::EOptionsSoundVolumes>(luSelectedId));
            UpdateSoundSettings();
            mbSettingsChanged = true;
            break;

        case E_OPTIONROW_SFX_VOLUME:
            mOptionsData.SetSFXVolume(
                static_cast<CrashNavOptionsData::EOptionsSoundVolumes>(luSelectedId));
            UpdateSoundSettings();
            mbSettingsChanged = true;
            break;

        case E_OPTIONROW_TIPS:
        {
            // The row's options are { $GENERAL_OPTION_ON, $GENERAL_OPTION_OFF }, so id 0
            // means tips ON. The console asserts any other id and still stores false.
            bool lbTips = true;
            if (luSelectedId != 0)
            {
                CGS_ASSERT(luSelectedId == 1, "Invalid ID for selected option ");   // cpp:830
                lbTips = false;
            }
            mOptionsData.SetTips(lbTips);
            mbSettingsChanged = true;
            break;
        }

        default:
            CGS_ASSERT(false, "Unknown option ");   // cpp:837
            break;
        }

        // [DIAG] NOT IN THE X360 BINARY -- [opt] the edited model, per keypress.
        if (getenv("BRN_OPTIONS_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[opt] EDIT row " << liRow
                << " (0 camera 1 music 2 sfx 3 tips) id " << static_cast<s32>(luSelectedId)
                << " -> model music=" << static_cast<s32>(mOptionsData.GetMusicVolume())
                << " sfx=" << static_cast<s32>(mOptionsData.GetSFXVolume())
                << " changed=" << (mbSettingsChanged ? 1 : 0) << "\n";
        }
    }

    // ---- ApplyAndSaveSettings @0x824CDD00 ------------------------------------------
    // ⭐ THE WHOLE POINT OF THE TAB. SetToProfile persists the edited model into the
    // profile block the GUI cache owns; OutputEvents publishes the same model as the six
    // records the rest of the game listens for. Both bodies already existed in this TU --
    // this is the call that was missing.
    void CrashNavOptions::ApplyAndSaveSettings()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");             // cpp:867
        CGS_ASSERT(mpStateInterface != 0, "mpStateInterface"); // cpp:868

        mOptionsData.SetToProfile(mpGuiCache->GetOptionsDataProfile());
        mOptionsData.OutputEvents(mpGuiCache->GetOptionsDataProfile(), mpStateInterface);

        // [DIAG] NOT IN THE X360 BINARY -- [opt] what was PERSISTED, read back out
        // of the profile block so this line proves the WRITE, not the model.
        if (getenv("BRN_OPTIONS_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            OptionsDataProfile* lpDiagProfile = mpGuiCache->GetOptionsDataProfile();
            *CgsDev::Log::gpDebugPrint
                << "[opt] SAVE profile camera=" << static_cast<s32>(lpDiagProfile->GetCameraFeed())
                << " music=" << lpDiagProfile->GetMusicVolume()
                << " sfx=" << lpDiagProfile->GetSFXVolume()
                << " tips=" << (lpDiagProfile->GetTips() ? 1 : 0)
                << " ffb=" << (lpDiagProfile->GetForceFeedback() ? 1 : 0)
                << " -- published 278/463x2/472/475/473/356\n";
        }
    }

    // ---- UpdateSoundSettings @0x824CDF10 -------------------------------------------
    // Publish the model's CURRENT (uncommitted) volumes so scrubbing is audible.
    void CrashNavOptions::UpdateSoundSettings()
    {
        CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");   // cpp:927

        GuiEventCrashNavAudioVolumes lEvent;
        lEvent.miMusicVolume = static_cast<s32>(mOptionsData.GetMusicVolume());
        lEvent.miSFXVolume   = static_cast<s32>(mOptionsData.GetSFXVolume());
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lEvent)));
    }

    // ---- RestoreSoundSettings @0x824CDDA0 ------------------------------------------
    // The inverse: republish the PROFILE's volumes, undoing an abandoned scrub.
    void CrashNavOptions::RestoreSoundSettings()
    {
        CGS_ASSERT((mpStateInterface != 0) && (mpGuiCache != 0),
                   "( mpStateInterface ) && ( mpGuiCache )");   // cpp:886

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();

        GuiEventCrashNavAudioVolumes lEvent;
        lEvent.miMusicVolume = static_cast<s32>(lpProfile->GetMusicVolume());
        lEvent.miSFXVolume   = static_cast<s32>(lpProfile->GetSFXVolume());
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lEvent)));
    }

    // ---- RestoreVoipSettings @0x824CDE68 -------------------------------------------
    void CrashNavOptions::RestoreVoipSettings()
    {
        CGS_ASSERT((mpStateInterface != 0) && (mpGuiCache != 0),
                   "( mpStateInterface ) && ( mpGuiCache )");   // cpp:906

        GuiEventCrashNavVoipVolume lEvent;
        lEvent.miVoipVolume =
            static_cast<s32>(mpGuiCache->GetOptionsDataProfile()->GetVoipVolume());
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lEvent)));
    }

    // ---- PostAudioTrigger ----------------------------------------------------------
    // The 112-byte audio-trigger record the X360 builds inline at three sites in this TU
    // (TriggerSound @0x824CDFB0, OnLeave @0x824CDAD0 and the two volume arms of
    // HandleControllerInput): { size 100, type 457, offset 12 } followed by the native
    // { component[32], action, label[32], movie[32] } payload. Spelled with the tree's
    // existing wire type and the same hand-built record BrnChallengeListComponent.cpp
    // already uses for this event, so the queued bytes match the console's.
    void CrashNavOptions::PostAudioTrigger(s32 liAction, const char* lpacMovie)
    {
        GuiAudioTriggerEvent lEvent;
        lEvent.Construct(liAction, KAC_EMPTY, lpacMovie);

        struct Record
        {
            s32 miSize;
            s32 miType;
            s32 miOffset;
            GuiAudioTriggerWirePayload457 mPayload;
        } lRecord = { 100, 457, 12, {} };
        std::memcpy(&lRecord.mPayload, lEvent.macComponent, sizeof(lRecord.mPayload));

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRecord)));
    }

    // ---- TriggerSound @0x824CDFB0 --------------------------------------------------
    // The menu click. Moving between ROWS and moving between OPTIONS are different cues,
    // and any other action reaching here is the console's own assert.
    void CrashNavOptions::TriggerSound(s32 liAction)
    {
        const char* lpacLabel = 0;

        switch (liAction)
        {
        case 0x25:   // 37
        case 0x26:   // 38
        case KI_ACTION_TOGGLE_PREV:
        case KI_ACTION_TOGGLE_NEXT:
            lpacLabel = "MenuToggleDefault";
            break;

        case 0x27:   // 39
        case 0x28:   // 40
        case KI_ACTION_OPTION_PREV:
        case KI_ACTION_OPTION_NEXT:
            lpacLabel = "MenuItemToggleDefault";
            break;

        default:
            CGS_ASSERT(false, "lpcLabel");   // cpp:999
            break;
        }

        if (lpacLabel == 0)
        {
            return;
        }

        PostAudioTrigger(7, lpacLabel);
    }

    // ---- StateCancelFlow @0x824CE0C0 -----------------------------------------------
    void CrashNavOptions::StateCancelFlow()
    {
        RestoreSoundSettings();
        RestoreVoipSettings();
        mMenuToggleGroup.Unloaded();
        SendStateEvent("GO_BACK");
    }
}
