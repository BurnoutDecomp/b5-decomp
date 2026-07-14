#ifndef BRN_GUI_ALWAYS_AVAILABLE_COMPONENTS_MANAGER_H
#define BRN_GUI_ALWAYS_AVAILABLE_COMPONENTS_MANAGER_H

#include "types.hpp"

#include "GameShared/GameClasses/Gui/Model/CgsEventObserver.h"   // CgsGui::EventObserver (base)
#include "GameSource/Gui/Flow/Permanent/Components/BrnEATraxInGameComponent.h"
#include "GameSource/Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.h"
#include "GameSource/Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.h"
#include "GameSource/Gui/Flow/Permanent/Components/BrnSaveIconComponent.h"
#include "GameSource/Gui/Flow/Permanent/Components/BrnShowtimeMessageComponent.h"

// ============================================================================
// GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h
//
// BrnGui::AlwaysAvailableComponentsManager -- the front-end manager for the GUI
// components that are ALWAYS loaded (the in-game EATrax "now playing" banner, the
// achievement pop-up, the online-invite message bar, the save-icon spinner and
// the "Showtime!" banner). It derives from CgsGui::EventObserver and registers
// for the GUI events that drive those overlays.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Member NAMES / TYPES / order are
// from the DecFIGS DWARF (BrnGuiAlwaysAvailableComponentsManager.h); member
// PLACEMENT and behaviour are confirmed against the X360 pseudocode + asm:
//   Construct        @ 0x824F3628
//   Prepare          @ 0x824F3760
//   PrepareFlapt     @ 0x824F3858
//   SetInEventQueue  @ 0x824F3920   (virtual)
//   Update           @ 0x82509338   (virtual)   [reconstruction BLOCKED -- see .cpp]
//
// The five always-available components are embedded BY VALUE (the X360 Construct
// builds each in place at a fixed offset inside this object). Their PC declarations
// expose the members needed by the faithfully homed construction/preparation path;
// every access is through a named member.
// ============================================================================

// The DWARF spells the in-event-queue type InputBuffer::GuiEventQueue; that nested typedef
// resolves to CgsModule::VariableEventQueue<18432,16> (the GUI module-IO queue the X360
// GetFirstEvent walks). Modelled by the real homed template; opaque here (stored/forwarded
// by SetInEventQueue, only walked by the BLOCKED Update body).
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue; }

namespace BrnGui
{
    class GuiCache;
    class GuiModule;

    class AlwaysAvailableComponentsManager : public CgsGui::EventObserver
    {
    public:
        // The two-phase Prepare state machine (DWARF: PrepareStage @ .h:46). Prepare
        // advances one stage per call (START -> REGISTERFOREVENTS -> LOADRESOURCES ->
        // DONE) and returns true only once it reaches DONE.
        enum PrepareStage
        {
            E_PREPARE_START             = 0,
            E_PREPARE_REGISTERFOREVENTS = 1,
            E_PREPARE_LOADRESOURCES     = 2,
            E_PREPARE_DONE              = 3,
        };

        // @0x824F3628 -- build the embedded EventObserver + the five always-available
        // components and clear the manager's flags / pointers.
        void Construct();

        // @0x824F3760 -- advance the prepare state machine one step; true at DONE.
        bool Prepare(CgsGui::GuiAccessPointers* lpGuiAccessPointers);

        // @0x824F3858 -- bind every always-available component to its named movie clip
        // in the supplied flapt file, then initialise the EATrax + achievement
        // components and mark the container clip playing / flapt prepared.
        void PrepareFlapt(const BrnFlapt::FileRef& lFile);

        // @0x824F3920 -- latch the input GUI event queue the manager pumps in Update.
        virtual void SetInEventQueue(CgsModule::VariableEventQueue<18432, 16>* lpInGuiEventQueue);

        // @0x82509338 -- pump the input event queue and drive the overlays. The body is
        // BLOCKED (depends on uncommitted GUI event-payload types + GuiCache /
        // OptionsDataProfile far members); see the .cpp for the trap stub + reason.
        virtual void Update();

    private:
        // The five always-available GUI components, embedded by value. Guest offsets
        // (relative to `this`) are noted for provenance only.
        EATraxInGameComponent         mEATraxInGameComponent;          // +0x10020
        AchievementPopupComponent     mAchievementPopupComponent;      // +0x10060
        OnlineInviteMessageComponent  mOnlineNotificationMessageComponent; // +0x100B8
        BrnSaveIconComponent          mSaveIconComponent;              // +0x10124
        BrnShowtimeMessageComponent   mShowtimeMessageComponent;       // +0x1013C

        // Scratch text recovered from the rich-presence "now playing" event (event 503
        // in Update): the composer and the work/track title. Width 48 from the X360
        // CgsCore::StrCpy bound (the manager copies up to 0x30 bytes into each).
        char macComposerTextId[48];    // +0x10154
        char macWorkTextId[48];        // +0x10184

        PrepareStage mePrepareStage;                       // +0x101B4

        bool mbExternalResourcesDependenciesLoaded;        // +0x101B8
        bool mbGameLoadStateCompleted;                     // +0x101B9
        bool mbContainerMovieClipPlaying;                  // +0x101BA
        bool mbShowNewsNotification;                        // +0x101BB

        CgsModule::VariableEventQueue<18432, 16>* mpInGuiEventQueue;      // +0x101BC
        GuiCache*                   mpGuiCache;             // +0x101C0
        bool                        mbFlaptPrepared;        // +0x101C4
    };

    // Reach the named manager sub-object owned by GuiModule. Declared next to the type it
    // returns so callers such as BrnGui::ViewModule do not need GuiModule.h's heavy includes.
    AlwaysAvailableComponentsManager* GetAlwaysAvailableComponentsManager(GuiModule* lpGuiModule);
}

#endif // BRN_GUI_ALWAYS_AVAILABLE_COMPONENTS_MANAGER_H
