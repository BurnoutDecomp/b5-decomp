#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                   // BrnGui::GuiFlow (the cache watcher selector)
#include "GameSource/Gui/BrnGuiProfile.h"                         // ProfileManager + ProfileMessageDisplay + ProfileTaskResultHandler
#include "GameSource/Gui/BrnGuiTextField.h"                       // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"  // BrnGui::ButtonIconComponent (by value)

// BrnGui::BootProfile - the boot profile/save-device GUI state (BF_PROFILE).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824743F8  OnEnter @0x82478440  OnLeave @0x824784F8
//   Update @0x8247E500     HandleControllerInput @0x82478610
//   HandleProfileTaskResult @0x82474468  GetResourcesToLoad @0x825080F0
// with the DecFIGS DWARF (GameSource/Gui/Flow/Hud/States/BrnBootProfile.h) as the
// shape/name authority. This header is also the DWARF home of the embedded
// BrnGui::ProfileMessageComponent (h:47).
//
// The state waits for the GUI cache, plays the save/load prompt apt movie
// "SaveLoadComponent" at display level 3 (the asm string xref @0x8247E6FC pins the
// name), waits for its apt components to initialise, fires the profile manager's
// boot-up task, routes controller accept/back into the manager's message choices,
// and -- when the manager reports the task complete (HandleProfileTaskResult) --
// posts phase-complete (command 70) plus the loading screen.
namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    // DWARF BrnBootProfile.h:47 -- the apt-side save/load prompt surface. The manager
    // forwards its MessageDisplay duties here (AttachMessageDisplay in OnEnter); the
    // component pushes the prompt onto the "SaveLoadComponent" movie's clips through
    // the standard component key/value chain (TextField/ButtonIcon AddOutputAptViewState
    // -> AptAux::UpdateFlashComponent -> AptCommunicator::UpdateComponent -> the
    // per-frame UpdateAll flush into the movie AS). The five sub-components bind the
    // movie's container clips: "message" / "option0" / "option1" (ControlledTextBox
    // text fields message_text / option0_text / option1_text) and the two glyphs
    // "option0_button" / "option1_button".
    class ProfileMessageComponent : public ProfileMessageDisplay
    {
    public:
        // @0x82473B30 (cpp:76) -- stash the interface, Construct the five
        // sub-components with the movie clip names, clear the message state.
        void Construct(CgsGui::StateInterface* lpStateInterface);

        // @0x82473C38 (cpp:121) -- register the five sub-component names as expected
        // apt components on the cache watcher (GuiCache::AppendExpectedAptComponent's
        // name-taking entry @0x824F87C0, once per sub-component).
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);

        // @0x82473C30 (cpp:108) -- the live prompt's option count (0 none / 1 ok /
        // 2 ok-cancel); BootProfile::HandleControllerInput keys the choice off it.
        s32 GetNumOptions();

        // DWARF h:188 (X360 inline: Update's WAIT_FOR_MESSAGE stage reads the flag
        // at this+1388 directly).
        bool HasMessage() const { return mbHasMessage; }

        // @0x82473CA8 (cpp:136) -- (re)push the stored message/options/glyphs to the
        // apt movie (the disk-space re-entry path replays it after the movie reloads).
        void ResendMessageToApt();

        // ProfileMessageDisplay overrides (the manager's prompt forwarding).
        // @0x82473DC0 (cpp:195) / @0x82474200 (cpp:235) / @0x82474370 (cpp:257).
        virtual void ShowMessage(const char* lpcMessage, u32 luNumberOfOptions,
                                 const char** lpacOptions);
        virtual void ShowNoSpaceMessage(const char* lpcMessage, u32 luRequiredKb,
                                        u32 luFreeKb);
        virtual void HideMessage();

    private:
        // DWARF h:95..h:97 size the three loc-id buffers (the X360 StrCpy tripwires
        // check < 0x40).
        static const u32 KU_MAX_MESSAGE_ID_LEN = 64;

        // DWARF cpp:47..cpp:53 -- the movie clip names + the blank string
        // ("message"/"option0"/"option1"/"option0_button"/"option1_button"/"",
        // Construct @0x82473B30 string xrefs).
        static const char macMessageTextName[8];
        static const char macLeftOptionTextName[8];
        static const char macRightOptionTextName[8];
        static const char macLeftOptionButtonName[15];
        static const char macRightOptionButtonName[15];
        static const char macBlankString[1];

        CgsGui::StateInterface* mpStateInterface;          // h:80  X360 +4
        TextField               mMessageText;              // h:82  X360 +8    ("message")
        TextField               mLeftOptionText;           // h:83  X360 +304  ("option0")
        TextField               mRightOptionText;          // h:84  X360 +600  ("option1")
        ButtonIconComponent     mLeftOptionButton;         // h:85  X360 +896  ("option0_button")
        ButtonIconComponent     mRightOptionButton;        // h:86  X360 +1040 ("option1_button")
        char macMessageId[KU_MAX_MESSAGE_ID_LEN];          // h:95  X360 +1184 (loc id)
        char macLeftOptionId[KU_MAX_MESSAGE_ID_LEN];       // h:96  X360 +1248 (loc id)
        char macRightOptionId[KU_MAX_MESSAGE_ID_LEN];      // h:97  X360 +1312 (loc id)
        s32  miNumOptions;                                 // h:98  X360 +1376
        u32  muSaveDataSizeKb;                             // h:99  X360 +1380 (no-space: required KB)
        u32  muGameDataSizeKb;                             // h:100 X360 +1384 (no-space: free KB)
        bool mbHasMessage;                                 // h:101 X360 +1388
    };

    // DWARF BrnBootProfile.h:115 -- the X360 layout carries the CgsGui::State base at
    // +0 and the ProfileTaskResultHandler base sub-object at +56 (Update passes
    // this+56 as the Bootup/CheckDiskSpace handler; HandleProfileTaskResult @0x82474468
    // reaches the members through the +56-adjusted this).
    struct BootProfile : public CgsGui::State, public ProfileTaskResultHandler
    {
        // The X360 Update's internal stage values (this+60; OnEnter seeds 0).
        // Names from the DWARF `enum InternalState` (BrnBootProfile.h:149).
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE         = 0,   // wait for the cache event (64); disk-space re-entry fires CheckDiskSpace
            E_INTERNALSTATE_WAIT_RESOURCES   = 1,   // wait resources + arm the component watch
            E_INTERNALSTATE_WAIT_FOR_MESSAGE = 2,   // (re-entry: wait the no-space prompt) then post 138 + play "SaveLoadComponent" @ level 3
            E_INTERNALSTATE_WAIT_INITIALISE  = 3,   // wait apt components + profile-manager boot-up
            E_INTERNALSTATE_RUNNING          = 4,   // drive controller input into the profile flow
            E_INTERNALSTATE_LEAVING          = 5,   // loading screen + phase-complete (70)
            E_INTERNALSTATE_COUNT            = 6,
        };

        // Keep the base 2-arg Construct reachable (the 3-arg overload below would
        // otherwise hide it; the PC flow falls back to it when no manager is wired).
        using CgsGui::State::Construct;

        // @ 0x824743F8 (cpp:280, DWARF: virtual, manager by reference) -- base
        // Construct then thread the profile manager (BrnHudFlow::Prepare @0x8251A620
        // dispatches this wider overload for the BF_PROFILE slot only).
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm,
                               ProfileManager& lrProfileManager);

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825080F0 - hands the boot-profile state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // @ 0x82474468 (cpp:584) -- ProfileTaskResultHandler override: the manager's
        // ReportTaskCompleted lands here when the boot-up (or disk-space) task
        // resolves; un-silence the manager and advance to LEAVING.
        virtual void HandleProfileTaskResult();

        // @ 0x82478610 - accept (sub-id 49) / back (50) presses drive the profile
        // manager's message choice; accept with a live prompt also fires the "Accept"
        // audio trigger.
        void HandleControllerInput(const CgsModule::Event* lpEvent);

    private:
        static const s32 maiEventToObserve[];                     // @ 0x8205AB98 (.rdata)
        static const s32 miNumEventsObserved;                     // == 3
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25F78 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25F80 (.rdata)

        InternalState           meInternalState;   // h:178  X360 this+60
        ProfileMessageComponent mProfileMessage;   // h:179  X360 this+64 (by value)
        ProfileManager*         mpProfileManager;  // h:180  X360 this+1456 (threaded by the 3-arg Construct)
        GuiCache*               mpGuiCache;        // h:181  X360 this+1460 (the cache event fills it)
        bool                    mbCheckDiskSpace;  // h:182  X360 this+1464 (re-entry disk-space mode)
    };
}
