// ===================================================================================
// BrnGui::CrashNavTrax -- THE PAUSE MENU'S "EA TRAX" TAB (CN_TRAX, script id 94).
//
// Owner, 2026-09-16: "make more UI work, like the EA Trax UI ... Every elements of the
// pause menu need to work 1:1".
//
// ⭐⭐ THIS TAB DID NOT EXIST IN CODE. The class was a header-only shell (only
// GetResourcesToLoad), so OnEnter / Update / OnLeave fell through to the do-nothing
// CgsGui::State base, the state registered for no events and therefore received none, and
// the component that draws the track list had no TU on the compile line at all. Unlike
// CN_STATS, this one IS reachable in the shipped flow: BRNSCREENFSM's NextState_8CN_SETTINGS
// sends "TO_TRAX" to Transition_8CN_SETTINGS_94CN_TRAX, and CN_SETTINGS is one RB press
// from the START pause screen.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (16 functions):
//   CrashNavTrax::OnEnter          @0x824B8C90   OnLeave              @0x824CF630
//   Update                         @0x824E0D50   UpdateInitSetup      @0x824C1DE0
//   UpdateLoading                  @0x824CF6C0   UpdateWFInit         @0x824D9D58
//   UpdatePermanent                @0x824E03B8   HandleControllerInput@0x824DECF8
//   HandleTriggers                 @0x824B8D00   HandleOverlayCompleteEvent @0x824DEF60
//   HandleTraxEnabledStateChange   @0x824CF898   OnUpdatePlayOrderMode@0x824CF7C0
//   PreviewTrack                   @0x824CF830   ApplyAndSaveSettings @0x824CF938
//   StateCancelFlow                @0x824D9E30   GetResourcesToLoad   @0x825000E0 (header)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavTrax.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"            // CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // VariableEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"   // OptionsDataProfile (the save path)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"           // GuiOverlayRequest

namespace BrnGui
{
    // ---- static out-of-line definitions -------------------------------------------
    // .rdata @0x82066620, read out of the image: { 6, 0x15, 0x40, 0xBD } -- controller
    // input (6), apt trigger (21), GuiCache handover (64), overlay complete (189). These
    // are exactly the four ids UpdateInitSetup / UpdatePermanent switch on.
    const s32 CrashNavTrax::maiEventToObserve[4] = { 6, 21, 64, 189 };
    const s32 CrashNavTrax::miNumEventsObserved  = 4;

    // .rdata @0x82F27278 = { {0x91, 4}, {0x56, 4} }, count word @0x82F27288 = 2. The same
    // pair BrnScreenStatesDataLinkStubs.cpp had independently read for this address.
    const CgsGui::sResourceTuple CrashNavTrax::maResourcesToLoad[] =
    {
        { 145u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  86u, CgsGui::E_GUI_RESOURCETYPE_APT },
    };
    const u32 CrashNavTrax::muNumResourcesToLoad = 2;

    // DWARF BrnCrashNavTrax.cpp:50 -- the apt instance name OnEnter constructs the list
    // component under (X360 .rdata "EATraxMenuInstance", 18 chars + NUL == the DWARF's 19).
    const char CrashNavTrax::mpacEATraxMenuComponentName[19] = "EATraxMenuInstance";

    namespace
    {
        // The state in-queue is an 18KB variable event queue (X360
        // VariableEventQueue<18432,16>), reached through the base State's
        // mpInGuiEventQueue -- the committed BrnCrashNavStats / BrnCredits idiom.
        typedef CgsModule::VariableEventQueue<18432, 16> CrashNavTraxInQueue;

        const s32 KI_EVENT_CONTROLLER_INPUT = 6;
        const s32 KI_EVENT_APT_TRIGGER      = 21;
        const s32 KI_EVENT_GUI_CACHE        = 64;
        const s32 KI_EVENT_OVERLAY_COMPLETE = 189;

        // The controller actions this screen answers. 41/42 move the highlight, 43/44
        // change the highlighted track's state, 51 changes EVERY track's state, 52 flips
        // the play order, 55 auditions the highlighted track, 49/50 accept and cancel.
        const s32 KI_ACTION_TOGGLE_PREV    = 41;   // 0x29
        const s32 KI_ACTION_TOGGLE_NEXT    = 42;   // 0x2A
        const s32 KI_ACTION_OPTION_PREV    = 43;   // 0x2B
        const s32 KI_ACTION_OPTION_NEXT    = 44;   // 0x2C
        const s32 KI_ACTION_ACCEPT         = 49;   // 0x31
        const s32 KI_ACTION_BACK           = 50;   // 0x32
        const s32 KI_ACTION_CHANGE_ALL     = 51;   // 0x33
        const s32 KI_ACTION_TOGGLE_ORDER   = 52;   // 0x34
        const s32 KI_ACTION_PREVIEW_TRACK  = 55;   // 0x37

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const char* const KAC_CANCEL_CONFIRM_OVERLAY = "CNCnclCnfrm";
        const char* const KAC_EMPTY                  = "";
        const s32         KI_APT_DISPLAY_LEVEL       = 3;

        // "no track" for the preview channel -- the value ACCEPT and the cancel flow both
        // post to stop whatever is auditioning (`li r11, -1` @0x824D9E5C).
        const s32 KI_PREVIEW_STOP = -1;

        // ---- the three records this screen posts on channel 40 ---------------------
        // Each is the X360's stack record verbatim: { payload bytes, wire type, payload
        // offset } followed by the payload, handed to AddEvent with the record's own size.
        // They are spelled here rather than taken from BrnGuiDemangledEventTypes.h because
        // that header's GuiEventAudioTraxUpdate is a 20-byte placeholder slice and
        // including it alongside BrnGuiEventTypeDefs.h is a hard C2011 (the same clash the
        // CrashNavOptions wave hit).

        // type 458 (0x1CA), record 48: the two per-track bit fields, republished whenever
        // a track's state changes. `li r11, 0x20 / 0x1CA / 0x10` @0x824CF910..0x824CF928,
        // then AddEvent(queue, rec, 40, 0x30). The 8-byte alignment of FastBitArray<128>
        // is what puts the payload at +16 rather than +12.
        struct GuiEventAudioTraxUpdateRecord
        {
            s32 miSize;      // 32 -- payload bytes
            s32 miType;      // 458
            s32 miOffset;    // 16 -- payload offset
            s32 miPad;
            GuiEventAudioTraxUpdate::EATraxArrayType mEnabledInFreeBurn;   // +0x10
            GuiEventAudioTraxUpdate::EATraxArrayType mEnabledInEvents;     // +0x20
        };

        // type 460 (0x1CC), record 20: audition this track (-1 == stop).
        struct GuiEventAudioTraxPreviewRecord
        {
            s32 miSize;      // 8
            s32 miType;      // 460
            s32 miOffset;    // 12
            s32 miTrackIndex;   // +0x0C
            u8  mbFlag;         // +0x10 -- the console writes 0 and never anything else
        };

        // type 462 (0x1CE), record 16: the play-order selector.
        struct GuiEventAudioTraxPlayOrderRecord
        {
            s32 miSize;      // 4
            s32 miType;      // 462
            s32 miOffset;    // 12
            s32 miPlayOrderMode;   // +0x0C
        };

        // type 356 (0x164), record 16: the same "commit the profile" marker
        // CrashNavOptionsData::OutputEvents ends with.
        struct GuiEventTraxCommitRecord
        {
            s32 miSize;      // 1
            s32 miType;      // 356
            s32 miOffset;    // 12
            u8  mbFlag;      // +0x0C -- 0
        };

        // The event-189 payload view (the committed CrashNavOptions shape): the overlay id
        // at +0x04 and the answer at +0x08.
        struct GuiOverlayCompletePayload : public CgsModule::Event
        {
            s32 miPad;        // +0x00
            u32 muOverlayId;  // +0x04
            s32 miResult;     // +0x08 (1 == the positive answer)
        };

        template <typename T>
        void PostRecord(CgsGui::StateInterface* lpStateInterface, const T& lrRecord)
        {
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lrRecord), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(T)));
        }
    }

    // ---- OnEnter @0x824B8C90 --------------------------------------------------------
    void CrashNavTrax::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meCurrentState = E_INTERNALSCREENSTATE_SETUP;   // this+0x38 = 0
        mpGuiCache     = 0;                             // this+0x3C = 0

        // The X360 passes a null parent prefix and -1 for the DWARF's unused 4th argument.
        mEATraxMenuComponent.Construct(mpacEATraxMenuComponentName, mpStateInterface, 0, -1);

        meTraxPlayOrderMode =
            GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_SEQUENTIAL;   // this+0x230 = 0
    }

    // ---- OnLeave @0x824CF630 --------------------------------------------------------
    // Stamp LEAVING first (the asm stores 4 before the unregister), drop the event
    // registration and tear the movie down with the empty-name PlayAptMovie every sibling
    // state leaves on.
    void CrashNavTrax::OnLeave()
    {
        meCurrentState = E_INTERNALSCREENSTATE_LEAVING;
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->PlayAptMovie(KAC_EMPTY, KI_APT_DISPLAY_LEVEL);
    }

    // ---- Update @0x824E0D50 ---------------------------------------------------------
    // The family's fall-through sub-state ladder. ⚠️ Its "run permanent" flag is the
    // INVERSE of the one CrashNavStats uses: here `v2 = 0` before the switch and only the
    // LEAVING arm sets it, and the tail runs UpdatePermanent when it is still clear -- so
    // UpdatePermanent runs in every state except LEAVING, which is the same net behaviour
    // spelled the other way round. Reproduced as the console spells it.
    void CrashNavTrax::Update()
    {
        bool lbLeaving = false;   // X360 `v2 = 0`

        switch (meCurrentState)
        {
        case E_INTERNALSCREENSTATE_SETUP:
            meCurrentState = E_INTERNALSCREENSTATE_SETUP;
            if (!UpdateInitSetup())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_LOADING:
            meCurrentState = E_INTERNALSCREENSTATE_LOADING;
            if (!UpdateLoading())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_INITIALISING:
            meCurrentState = E_INTERNALSCREENSTATE_INITIALISING;
            if (!UpdateWFInit())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_RUNNING:
            // The DWARF's UpdateRunning (cpp:333) is folded to exactly this one call.
            meCurrentState = E_INTERNALSCREENSTATE_RUNNING;
            mEATraxMenuComponent.Update();
            break;

        case E_INTERNALSCREENSTATE_LEAVING:
            lbLeaving      = true;
            meCurrentState = E_INTERNALSCREENSTATE_LEAVING;
            break;

        default:
            // X360 cpp:155 -- the streamed form, "Invalid internal state (" << state << ").
            CGS_ASSERT(false, "Invalid internal state");
            break;
        }

        if (!lbLeaving)
        {
            UpdatePermanent();
        }

        // The in-queue is cleared unconditionally, so an event no rung consumed is dropped.
        reinterpret_cast<CrashNavTraxInQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ---- UpdateInitSetup @0x824C1DE0 ------------------------------------------------
    // Wait for the GuiCache handover. ⚠️ Unlike its siblings this rung does NOT post a
    // "setup done" command -- it only latches the cache and reports true.
    bool CrashNavTrax::UpdateInitSetup()
    {
        CrashNavTraxInQueue* lpInQueue =
            reinterpret_cast<CrashNavTraxInQueue*>(mpInGuiEventQueue);

        bool lbSetupDone = false;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                // cpp:219 -- the console's copy-pasted message, kept verbatim.
                CGS_ASSERT(lpCache, "Invalid cache in RaceMainHudState::OnEnter");

                lbSetupDone = true;
                mpGuiCache  = lpCache;
            }

            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        return lbSetupDone;
    }

    // ---- UpdateLoading @0x824CF6C0 --------------------------------------------------
    // Once both apt resources are in, show the movie and register the ONE apt component
    // this screen waits on -- the track list itself. The console inlines the DWARF's
    // SetExpectedAptComponentList (cpp:277) into this tail.
    bool CrashNavTrax::UpdateLoading()
    {
        CGS_ASSERT(mpGuiCache, "NULL != mpGuiCache");   // cpp:245

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpStateInterface->PlayAptMovie("BrnCrashNavTrax", KI_APT_DISPLAY_LEVEL);

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mEATraxMenuComponent.GetName());
        return true;
    }

    // ---- UpdateWFInit @0x824D9D58 ---------------------------------------------------
    // The components are up: seed the list from the PROFILE's two bit fields and its play
    // order, then clear the "settings changed" latch that seeding just set -- entering the
    // tab must not count as an edit, or BACK would ask to discard on an untouched screen.
    bool CrashNavTrax::UpdateWFInit()
    {
        CGS_ASSERT(mpGuiCache, "NULL != mpGuiCache");   // cpp:298

        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        mEATraxMenuComponent.Initialize(lpProfile->GetTraxAvailableInFreeBurn(),
                                        lpProfile->GetTraxAvailableInEvents());

        meTraxPlayOrderMode = lpProfile->GetTraxPlayOrderMode();
        OnUpdatePlayOrderMode();

        mEATraxMenuComponent.ResetSettingsChanged();
        return true;
    }

    // ---- UpdatePermanent @0x824E03B8 ------------------------------------------------
    void CrashNavTrax::UpdatePermanent()
    {
        CrashNavTraxInQueue* lpInQueue =
            reinterpret_cast<CrashNavTraxInQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
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

            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // ---- HandleControllerInput @0x824DECF8 ------------------------------------------
    // ⚠️ THE FIVE LIST ARMS ARE GATED ON `meCurrentState >= RUNNING` (`*(v3+56) >= 3`) but
    // ACCEPT, BACK and the play-order flip are NOT. That is the console's own asymmetry --
    // you can back out of, or accept, a screen that is still loading.
    void CrashNavTrax::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavTrax::HandleControllerInput");   // cpp:408

        const s32 liAction =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
        const bool lbRunning = (meCurrentState >= E_INTERNALSCREENSTATE_RUNNING);

        switch (liAction)
        {
        case KI_ACTION_TOGGLE_PREV:
            if (lbRunning)
            {
                mEATraxMenuComponent.HandleInput(
                    EATraxMenuComponent::E_INPUTCOMMAND_HIGHLIGHT_PREVIOUS_TRACK);
            }
            break;

        case KI_ACTION_TOGGLE_NEXT:
            if (lbRunning)
            {
                mEATraxMenuComponent.HandleInput(
                    EATraxMenuComponent::E_INPUTCOMMAND_HIGHLIGHT_NEXT_TRACK);
            }
            break;

        case KI_ACTION_OPTION_PREV:
            if (lbRunning)
            {
                mEATraxMenuComponent.HandleInput(
                    EATraxMenuComponent::E_INPUTCOMMAND_CHANGE_CURRENT_TRACK_STATE_TO_PREVIOUS);
                HandleTraxEnabledStateChange();
            }
            break;

        case KI_ACTION_OPTION_NEXT:
            if (lbRunning)
            {
                mEATraxMenuComponent.HandleInput(
                    EATraxMenuComponent::E_INPUTCOMMAND_CHANGE_CURRENT_TRACK_STATE_TO_NEXT);
                HandleTraxEnabledStateChange();
            }
            break;

        case KI_ACTION_CHANGE_ALL:
            if (lbRunning)
            {
                mEATraxMenuComponent.HandleInput(
                    EATraxMenuComponent::E_INPUTCOMMAND_CHANGE_ALL_TRACKS_TO_NEXT);
                HandleTraxEnabledStateChange();
            }
            break;

        case KI_ACTION_PREVIEW_TRACK:
            if (lbRunning)
            {
                PreviewTrack(mEATraxMenuComponent.GetHighlightedTrackIndex());
            }
            break;

        case KI_ACTION_TOGGLE_ORDER:
            // (mode + 1) % 2 -- the selector only has SEQUENTIAL and RANDOM.
            meTraxPlayOrderMode = static_cast<GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode>(
                (static_cast<s32>(meTraxPlayOrderMode) + 1) %
                GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_COUNT);
            OnUpdatePlayOrderMode();
            break;

        case KI_ACTION_ACCEPT:
            // Stop any audition, commit, leave.
            PreviewTrack(KI_PREVIEW_STOP);
            ApplyAndSaveSettings();
            SendStateEvent("GO_BACK");
            break;

        case KI_ACTION_BACK:
            // Discarding edited settings asks first; an untouched tab just leaves.
            if (mEATraxMenuComponent.SettingsChanged())
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
            break;

        default:
            break;
        }
    }

    // ---- HandleTriggers @0x824B8D00 -------------------------------------------------
    // The console's whole body is the null assert -- this screen's apt clips raise no
    // trigger it acts on.
    void CrashNavTrax::HandleTriggers(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in CrashNavTrax::HandleTriggers");   // cpp:636
        (void)lpEvent;
    }

    // ---- HandleOverlayCompleteEvent @0x824DEF60 -------------------------------------
    void CrashNavTrax::HandleOverlayCompleteEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpOverlayCompleteEvent");   // cpp:544

        const GuiOverlayCompletePayload* lpComplete =
            reinterpret_cast<const GuiOverlayCompletePayload*>(lpEvent);

        if (lpComplete->muOverlayId ==
                static_cast<u32>(CgsIDCompress(KAC_CANCEL_CONFIRM_OVERLAY)) &&
            lpComplete->miResult == 1)
        {
            StateCancelFlow();
        }
    }

    // ---- OnUpdatePlayOrderMode @0x824CF7C0 ------------------------------------------
    // Push the new order to the caption and publish it so the music module reorders now --
    // it is audible before ACCEPT commits it, exactly like the volume rows on the options
    // tab.
    void CrashNavTrax::OnUpdatePlayOrderMode()
    {
        mEATraxMenuComponent.UpdatePlayOrderMode(meTraxPlayOrderMode);

        GuiEventAudioTraxPlayOrderRecord lRecord;
        lRecord.miSize          = 4;
        lRecord.miType          = 462;
        lRecord.miOffset        = 12;
        lRecord.miPlayOrderMode = static_cast<s32>(meTraxPlayOrderMode);
        PostRecord(mpStateInterface, lRecord);
    }

    // ---- PreviewTrack @0x824CF830 ---------------------------------------------------
    // Audition liTrackIndex, or stop when it is -1.
    void CrashNavTrax::PreviewTrack(s32 liTrackIndex)
    {
        GuiEventAudioTraxPreviewRecord lRecord;
        lRecord.miSize       = 8;
        lRecord.miType       = 460;
        lRecord.miOffset     = 12;
        lRecord.miTrackIndex = liTrackIndex;
        lRecord.mbFlag       = 0;
        PostRecord(mpStateInterface, lRecord);
    }

    // ---- HandleTraxEnabledStateChange @0x824CF898 -----------------------------------
    // Republish BOTH bit fields straight from the component, so a track that was just
    // enabled or disabled takes effect immediately -- the change is live before it is
    // saved, and StateCancelFlow is what puts the profile's copy back.
    void CrashNavTrax::HandleTraxEnabledStateChange()
    {
        GuiEventAudioTraxUpdateRecord lRecord;
        lRecord.miSize   = 32;
        lRecord.miType   = 458;
        lRecord.miOffset = 16;
        lRecord.miPad    = 0;
        lRecord.mEnabledInFreeBurn = *mEATraxMenuComponent.GetTraxEnabledInFreeBurnBitfieldPtr();
        lRecord.mEnabledInEvents   = *mEATraxMenuComponent.GetTraxEnabledInEventsBitfieldPtr();
        PostRecord(mpStateInterface, lRecord);
    }

    // ---- ApplyAndSaveSettings @0x824CF938 -------------------------------------------
    // ⭐ THE SAVE. The two bit fields and the play order go into the profile, then the
    // commit marker (type 356) asks the save system to write it out -- the same marker
    // CrashNavOptionsData::OutputEvents ends with.
    void CrashNavTrax::ApplyAndSaveSettings()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:670

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        lpProfile->SetTraxAvailableInFreeBurn(
            mEATraxMenuComponent.GetTraxEnabledInFreeBurnBitfieldPtr());
        lpProfile->SetTraxAvailableInEvents(
            mEATraxMenuComponent.GetTraxEnabledInEventsBitfieldPtr());
        lpProfile->SetTraxPlayOrderMode(meTraxPlayOrderMode);

        GuiEventTraxCommitRecord lRecord;
        lRecord.miSize   = 1;
        lRecord.miType   = 356;
        lRecord.miOffset = 12;
        lRecord.mbFlag   = 0;
        PostRecord(mpStateInterface, lRecord);
    }

    // ---- StateCancelFlow @0x824D9E30 ------------------------------------------------
    // Leave WITHOUT saving, and undo the live changes: stop the audition, go back, then
    // republish the PROFILE's bit fields and play order over the edited ones. Note the
    // order -- the console sends GO_BACK before it restores, and the restore reads the
    // profile fresh rather than trusting a cached copy.
    void CrashNavTrax::StateCancelFlow()
    {
        PreviewTrack(KI_PREVIEW_STOP);
        SendStateEvent("GO_BACK");

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();

        GuiEventAudioTraxUpdateRecord lRecord;
        lRecord.miSize   = 32;
        lRecord.miType   = 458;
        lRecord.miOffset = 16;
        lRecord.miPad    = 0;
        lRecord.mEnabledInFreeBurn = *lpProfile->GetTraxAvailableInFreeBurn();
        lRecord.mEnabledInEvents   = *lpProfile->GetTraxAvailableInEvents();

        const GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode lePlayOrderMode =
            lpProfile->GetTraxPlayOrderMode();

        PostRecord(mpStateInterface, lRecord);

        GuiEventAudioTraxPlayOrderRecord lOrderRecord;
        lOrderRecord.miSize          = 4;
        lOrderRecord.miType          = 462;
        lOrderRecord.miOffset        = 12;
        lOrderRecord.miPlayOrderMode = static_cast<s32>(lePlayOrderMode);
        PostRecord(mpStateInterface, lOrderRecord);
    }
}
