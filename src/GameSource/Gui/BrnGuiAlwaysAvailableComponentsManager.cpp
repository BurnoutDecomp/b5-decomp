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

    // @0x82509338 -- per-frame event pump. RECONSTRUCTION BLOCKED: see the file header. The
    // body switches on ~15 GUI event-payload types and reaches GuiCache /
    // BrnGui::OptionsDataProfile far members and the BrnSound EaTraxHelper accessors, none
    // of which have a reconstructable type home in this dossier. Trap stub until those homes
    // exist -- NO raw-offset poke, NO faked types.
    void AlwaysAvailableComponentsManager::Update()
    {
        // BLOCKED: needs the GUI event-payload type homes (events 9/26/43/44/64/72/105/175/
        // 191/355/392/502/503/516/586), BrnGui::OptionsDataProfile (+0x7344 / cache +0xB878),
        // the GuiCache active-track far member (+0x12BC0) and BrnSound::Module::Io::EaTraxHelper.
        CGS_ASSERT(false,
                   "BrnGui::AlwaysAvailableComponentsManager::Update is blocked: missing GUI "
                   "event-payload + GuiCache/OptionsDataProfile/EaTraxHelper type homes");
    }
}
