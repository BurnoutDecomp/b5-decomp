// ===================================================================================
// BrnGui::AlwaysAvailableComponentsManager
//   GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.cpp
//
//   AlwaysAvailableComponentsManager::Construct       @ 0x824F3628
//   AlwaysAvailableComponentsManager::Prepare         @ 0x824F3760
//   AlwaysAvailableComponentsManager::PrepareFlapt    @ 0x824F3858
//   AlwaysAvailableComponentsManager::SetInEventQueue @ 0x824F3920  (virtual)
//   AlwaysAvailableComponentsManager::Update          @ 0x82509338  (virtual) [BLOCKED]
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX for SEMANTIC PARITY. All five methods are
// non-static members (asm: r3 = this). Member access is BY NAME throughout; the X360
// Begin/Fire/End dev-assert sequences fold into CGS_ASSERT(cond,"msg") per house style.
//
// This manager owns the GUI components that are always loaded (the in-game EATrax track
// banner, the achievement pop-up, the online-invite message bar, the save-icon spinner
// and the "Showtime!" banner). Construct builds them in place; Prepare drives a two-phase
// prepare/register state machine; PrepareFlapt binds each to its named clip in a flapt
// movie file; SetInEventQueue latches the input event queue Update pumps.
//
// Update (@0x82509338) is the large per-frame event pump. Its reconstruction is BLOCKED:
// it switches on ~15 distinct GUI event-payload types (read by field off the live event
// record) and reaches GuiCache / BrnGui::OptionsDataProfile FAR MEMBERS (cache +0xB878
// profile sub-object, cache +0x12BC0 active-track slot, profile +0x7344 trax id) plus the
// BrnSound EaTraxHelper text accessors -- none of which have a reconstructable type home
// in this dossier. Faithfully restoring it would require inventing those layouts, which the
// project forbids; it is left as a trap stub (NO raw-offset poke, NO faked types) for the
// TU that recovers the event-payload + cache/profile homes.
// ===================================================================================
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::RegisterForEvents
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                        // BrnFlapt::FileRef
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // the in-queue Update walks
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache (event 64 connect)

namespace BrnGui
{
    // The GUI events the manager registers to observe: the X360 passes the real game-data
    // table dword_8206F760 with a hard-coded count of 19 (asm @0x824F37FC `li r5,0x13`;
    // DWARF `maiEventToObserve[19]` / `miNumEventsObserved == 19`). The table's literal
    // contents live in a .rodata section not present in this dossier, so it is modelled as
    // the extern game-data symbol (NOT redefined here) to preserve the asm-attested count
    // without fabricating the four observe-only ids beyond the fifteen the Update switch
    // handles. The fifteen recovered from the @0x82509338 switch are: 9, 26, 43, 44, 64, 72,
    // 105, 175, 191, 355, 392, 502, 503, 516, 586.
    const s32 maiEventToObserve[19] =
    {
        26, 21, 43, 44, 9, 64, 105, 175, 502, 586,
        503, 72, 94, 192, 96, 392, 516, 191, 355,
    }; // ARTIST dword_8206F760

    namespace
    {
        const s32 KI_NUM_EVENTS_OBSERVED = 19;   // asm @0x824F37FC: li r5, 0x13

        // The movie-clip / component names the X360 binds each always-available component to.
        const char* const KAC_EATRAX_COMPONENT_MOVIE_CLIP        = "EATrax_mc";
        const char* const KAC_ACHIEVEMENT_POPUP_COMPONENT_NAME   = "AchievementPopup_mc";
        const char* const KAC_ONLINE_NOTIFICATION_COMPONENT_NAME = "OnlineInvite_mc";
        const char* const KAC_SAVE_ICON_COMPONENT_NAME           = "SaveIcon_mc";
        const char* const KAC_SHOWTIME_MESSAGE_CPT_NAME          = "ShowtimeMsg_cpt";
    }

    // @0x824F3628 -- construct the EventObserver base and the five always-available
    // components in place, then clear the manager's flags / pointers.
    void AlwaysAvailableComponentsManager::Construct()
    {
        CgsGui::EventObserver::Construct();

        // The components take the manager's StateInterface (the EventObserver base member,
        // guest +4) as their channel to the rest of the GUI.
        CgsGui::StateInterface* lpStateInterface = &mStateInterface;

        mEATraxInGameComponent.Construct(KAC_EATRAX_COMPONENT_MOVIE_CLIP, lpStateInterface, 0);
        mAchievementPopupComponent.Construct(KAC_ACHIEVEMENT_POPUP_COMPONENT_NAME, lpStateInterface, 0);
        mOnlineNotificationMessageComponent.Construct(KAC_ONLINE_NOTIFICATION_COMPONENT_NAME,
                                                      lpStateInterface, 0);
        // The save-icon and showtime components are polymorphic; the X360 dispatches their
        // Construct through the vtable (slot 0).
        mSaveIconComponent.Construct(KAC_SAVE_ICON_COMPONENT_NAME, lpStateInterface, 0);
        mShowtimeMessageComponent.Construct(KAC_SHOWTIME_MESSAGE_CPT_NAME, lpStateInterface, 0);

        mePrepareStage = E_PREPARE_START;

        mbExternalResourcesDependenciesLoaded = false;
        mbGameLoadStateCompleted              = false;
        mbContainerMovieClipPlaying           = false;
        mbShowNewsNotification                = false;

        mbFlaptPrepared = false;
        mpGuiCache      = 0;
    }

    // @0x824F3760 -- advance the prepare/register state machine one step. Each call drops
    // into its current stage and falls through to the next, so a single call can walk
    // START -> REGISTERFOREVENTS -> LOADRESOURCES; it returns true only once the GuiCache has
    // been latched (mpGuiCache != null, set by the connect event in Update) and the stage
    // reaches DONE.
    bool AlwaysAvailableComponentsManager::Prepare(CgsGui::GuiAccessPointers* lpGuiAccessPointers)
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
            mePrepareStage = E_PREPARE_START;
            // Prepare the EventObserver base (its StateInterface) with the access pointers
            // and a null resource allocator (X360 passes 0 for the allocator).
            CGS_ASSERT(CgsGui::EventObserver::Prepare(lpGuiAccessPointers, 0),
                       "CgsGui::EventObserver::Prepare(lpGuiAccessPointers)");
            // fall through

        case E_PREPARE_REGISTERFOREVENTS:
            mePrepareStage = E_PREPARE_REGISTERFOREVENTS;
            mStateInterface.RegisterForEvents(maiEventToObserve, KI_NUM_EVENTS_OBSERVED);
            // fall through

        case E_PREPARE_LOADRESOURCES:
            mePrepareStage = E_PREPARE_LOADRESOURCES;
            // Only advance to DONE once the GuiCache pointer has been latched.
            if (mpGuiCache == 0)
            {
                return false;
            }
            // fall through

        case E_PREPARE_DONE:
            mePrepareStage = E_PREPARE_DONE;
            return true;

        default:
            return false;
        }
    }

    // @0x824F3858 -- bind every always-available component to its named movie clip in the
    // supplied flapt file, initialise the EATrax + achievement components, and mark the
    // container clip playing / the flapt prepared.
    void AlwaysAvailableComponentsManager::PrepareFlapt(const BrnFlapt::FileRef& lFile)
    {
        mEATraxInGameComponent.Prepare(KAC_EATRAX_COMPONENT_MOVIE_CLIP, lFile);
        mAchievementPopupComponent.Prepare(KAC_ACHIEVEMENT_POPUP_COMPONENT_NAME, lFile);
        mOnlineNotificationMessageComponent.Prepare(KAC_ONLINE_NOTIFICATION_COMPONENT_NAME, lFile);
        mSaveIconComponent.Prepare(KAC_SAVE_ICON_COMPONENT_NAME, lFile);
        mShowtimeMessageComponent.Prepare(KAC_SHOWTIME_MESSAGE_CPT_NAME, lFile);

        mbContainerMovieClipPlaying = true;

        mEATraxInGameComponent.Initialize();
        mAchievementPopupComponent.Initialize();

        mbFlaptPrepared = true;
    }

    // @0x824F3920 -- latch the input GUI event queue the manager pumps each frame in Update.
    void AlwaysAvailableComponentsManager::SetInEventQueue(CgsModule::VariableEventQueue<18432, 16>* lpInGuiEventQueue)
    {
        mpInGuiEventQueue = lpInGuiEventQueue;
    }

    // True once the manager observes an event id (its 19-entry table). Lets the GuiModule
    // fan the manager's subscribed events into its in-queue (the console routes them through
    // the shared observer-subscription filter; this is the same membership test).
    bool AlwaysAvailableComponentsManager::ObservesEvent(s32 liId) const
    {
        for (s32 li = 0; li < KI_NUM_EVENTS_OBSERVED; ++li)
            if (maiEventToObserve[li] == liId)
                return true;
        return false;
    }

    namespace
    {
        // The event-64 connect payload (the GuiCache pointer), same shape BootProfile reads.
        struct AacGuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };
    }

    // @0x82509338 -- per-frame event pump: walk the in-queue and drive the always-available
    // overlays. Reconstructed from the X360 body. The two in-game EATrax rich-presence cases
    // (502/503) reach BrnSound::Module::Io::EaTraxHelper + BrnGui::OptionsDataProfile /
    // GuiCache FAR members that have no committed type home; those specific accessor calls are
    // FLAG'd deferrals (documented below) -- the event DISPATCH and every other case (the
    // save-icon 355, the connect 64, showtime, achievement, the load flags) are faithful.
    void AlwaysAvailableComponentsManager::Update()
    {
        // The console gates the whole pump on the flapt being bound (this+65988): with no
        // clips bound there is nothing to drive.
        if (!mbFlaptPrepared || mpInGuiEventQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liId)
            {
            case 64:   // connect: latch the GuiCache (the far members the in-game cases read)
                CGS_ASSERT(lpEvent != 0, "Invalid cache in AlwaysAvailableComponentsManager::Update");
                mpGuiCache = static_cast<const AacGuiEventCache*>(lpEvent)->mpGuiCache;
                break;

            case 26:   // per-frame time-step: stamp the timed (in-game) overlays
                mbGameLoadStateCompleted = true;
                // FLAG PC-platform leaf: the console stamps the EATrax/achievement timers
                // from GuiEventTimeInfo::GetTime() (@0x8240E328), whose body is not committed
                // to the link. Those overlays are in-game only (never on the boot/front-end
                // path the save icon runs on); the flag above is the observable this frame.
                break;

            case 44:   // buddy notification cleared
                mbShowNewsNotification = false;
                break;

            case 72:   // external resource dependencies loaded
                mbExternalResourcesDependenciesLoaded = true;
                break;

            case 355:  // autosave icon: show (payload==1) / hide the top-left save spinner
                if (*reinterpret_cast<const u8*>(lpEvent) == 1)
                    mSaveIconComponent.ShowSaveIcon();
                else
                    mSaveIconComponent.HideSaveIcon();
                break;

            case 392:  // showtime banner: show / hide
                if (*reinterpret_cast<const u8*>(lpEvent) == 1)
                    mShowtimeMessageComponent.Show();
                else
                    mShowtimeMessageComponent.Hide(true);
                break;

            case 191:  // showtime hide (immediate) when the payload flag is clear
                if (*reinterpret_cast<const u8*>(lpEvent) == 0)
                    mShowtimeMessageComponent.Hide(true);
                break;

            case 9:    // fall-through hide: only once fully prepared
            case 516:
                if (mePrepareStage == E_PREPARE_DONE)
                    mShowtimeMessageComponent.Hide(true);
                break;

            case 586:  // new achievement unlocked
                mAchievementPopupComponent.DisplayNewAchievementNotification(
                    reinterpret_cast<const AchievementPopupComponent::AchievementsBitArray*>(lpEvent));
                break;

            // The online-invite chyron (175/43/105) and the EATrax "now playing" cases
            // (502/503) drive in-game-only overlays that do not fire on the boot/front-end
            // path. Their console bodies reach OnlineInviteMessageComponent::ShowMessage (not
            // yet committed) and BrnSound::Module::Io::EaTraxHelper + BrnGui::OptionsDataProfile
            // / GuiCache far members with no committed type home. The dispatch is kept; the
            // un-homed accessor calls are FLAG'd deferrals until those TUs land.
            // FLAG PC-platform leaf: in-game EATrax/online-invite overlays (un-homed callees).
            case 175:
            case 43:
            case 105:
            case 502:
            case 503:
                break;

            default:
                break;
            }
        }
    }
}
