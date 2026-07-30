#ifndef BRN_LICENSE_COMPONENT_H
#define BRN_LICENSE_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                    // Vector2 (SetPosition)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                     // BrnGui::IconComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                                    // BrnGui::TextField (six by value)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                // BrnGui::GuiFlow
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple

// ============================================================================
// GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h
//
// BrnGui::LicenseComponent -- the apt-driven "driver licence" screen component
// (the licence card shown by the Intro / CompletedGame / InstantResults / crash-nav
// driver-details flows). It shows the player's current licence rank artwork, the
// localised licence-issued date, the "wins to next rank" upgrade text, and it streams
// the licence's own GUI resources (one apt bundle per rank) in and out through the GUI
// cache as its presentation state machine advances.
//
// ⚠ CORRECTION (2026-07-30). The previous revision of this header stated that "no
// DecFIGS DWARF exists for this TU" and derived the class shape from the asm alone.
// THAT CLAIM WAS WRONG. The DWARF is complete --
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h
// -- and carries the full 17-value ELicenseStates enum, all 26 data members in order
// with their names, and the whole method set. Every member offset the X360 asm touches
// (148/164/168/172/176/472/768/1064/1360/1656/1952/1953/1954/1956/1960/1964/1968/1969/
// 1972/1976/1977/1978/1980/1984/1988/1992) matches the DWARF's declaration order and
// the guest widths exactly. This header is therefore rebuilt FROM THE DWARF, with the
// values gated on BURNOUT_X360_ARTIST.XEX. Two names the old header guessed were wrong
// and are corrected here: its `mDateTextField` is `mDateIssuedTextField`, and its
// `mbRankTransitionActive` is `mbHiding`.
//
// Base chain (the member offsets prove it):
//   LicenseComponent : IconComponent : CgsGui::GuiComponent
//     GuiComponent  : vptr@+0x00, macName[128]@+0x04, muHashedName@+0x84,
//                     mpStateInterface@+0x88   (guest sizeof 0x8C)
//     IconComponent : + mpStateIdentifiers@+0x8C, muStateIndex@+0x90 (guest sizeof 0x94)
//   so this class's own members begin at guest +0x94 and it ends at guest 0x7CC, which
//   is exactly the stride BrnGui::Intro's members prove (+0xD0 -> +0x89C).
//
// Guest byte offsets in the trailing comments are the 32-bit-pointer ABI's; the gate
// compiles 64-bit, so every member is reached BY NAME and the host sizeof legitimately
// drifts.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (class:BrnGui::LicenseComponent):
//   Construct                   @0x8241A610   AppendExpectedAptComponent @0x8241A790
//   OnLoad                      @0x82440AC0   ReleaseResources           @0x82440BC0
//   HandleAptLoadTriggers       @0x8241A848   HandleAptTransitionTriggers@0x8243BE20
//   Update                      @0x8243C0B8   SetPlayerInfo              @0x8243C380
//   SetPosition                 @0x824277D0   ShowLicense                @0x82440C98
//   ShowUpgradedLicense         @0x8243C7E0   HideLicense                @0x82434998
//   AddWin                      @0x82434A30   ShowScore                  @0x8241AAD8
//   RankUp                      @0x8243C918   SetVisible                 @0x82440E38
//   SendPlayerPictureEvent      @0x8243CB90   SetPercentageComplete      @0x8241AB48
//   UpdateDirt                  @0x8242D898   SetRank                    @0x8241A4B8
//   StartOutputtingGamerpic     @0x8243CC30   StopOutputtingGamerpic     @0x8243CD00
//   SetCachePointer             @0x824B31E8   SetProfilePointer          @0x824B3248
//   EnsureResourcesAreLoaded    @0x824B3300   EnsureResourcesAreUnloaded @0x824B33F8
// IsVisible / IsHiding / IsReady have no out-of-line bodies in the image -- they are the
// header inlines the DWARF declares at h:325 / h:309 / h:464.
// ============================================================================

namespace BrnGui { class GuiCache; }            // stored by pointer (mpGuiCache); body links from BrnGuiCache TU
namespace BrnProgression { class Profile; }     // stored by pointer (mpProfile)
namespace CgsGui { struct GuiEventAptTriggerPayload; }

namespace BrnGui
{
    // DWARF BrnLicenseComponent.h:46.
    class LicenseComponent : public IconComponent
    {
    public:
        // DWARF BrnLicenseComponent.h:49 -- the top ordinary licence rank.
        static const s32 KI_MAX_RANK = 5;

        // DWARF BrnLicenseComponent.h:178. The licence-presentation state machine. The two
        // values the X360 asserts by NAME (E_LICENSE_FIRST_RESOURCE_UNLOADED == 8 in
        // EnsureResourcesAreLoaded, E_LICENSE_RESOURCES_UNLOADED == 16 in
        // EnsureResourcesAreUnloaded) pin the numbering the rest of the switch tables use.
        enum ELicenseStates
        {
            E_LICENSE_CONSTRUCTED                      = 0,
            E_LICENSE_DATA_SUPPLIED                    = 1,
            E_LICENSE_FIRST_RESOURCE_LOADED            = 2,
            E_LICENSE_SHOWING_NORMAL                   = 3,
            E_LICENSE_SHOWING_UPGRADE_PENDING          = 4,
            E_LICENSE_SHOWING_TRANSOUT                 = 5,
            E_LICENSE_UPGRADING_OLD_LICENSE_LEAVING    = 6,
            E_LICENSE_FIRST_RESOURCE_UNLOADING         = 7,
            E_LICENSE_FIRST_RESOURCE_UNLOADED          = 8,
            E_LICENSE_SECOND_RESOURCE_LOADING          = 9,
            E_LICENSE_SECOND_RESOURCE_LOADED           = 10,
            E_LICENSE_UPGRADING_NEW_LICENSE_INITIALISING = 11,
            E_LICENSE_UPGRADING_NEW_LICENSE_WAITING    = 12,
            E_LICENSE_UPGRADING_NEW_LICENSE_ARRIVING   = 13,
            E_LICENSE_UPGRADING_ADDING_REQUIRED_WINS   = 14,
            E_LICENSE_UPGRADING_DONE                   = 15,
            E_LICENSE_RESOURCES_UNLOADED               = 16,
            E_LICENSE_COUNT                            = 17,
        };

        // @0x8241A610 (cpp:91). VIRTUAL -- the signature matches CgsGui::GuiComponent's
        // vtable slot 0, and the X360 Intro/CompletedGame/InstantResults call sites reach it
        // as this class's own Construct. Runs IconComponent::Construct (no state-identifier
        // table), seeds every scalar, and Constructs the six embedded TextFields parented on
        // this component's name.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x8241A790 (cpp:143) -- register this component and its three visible text fields
        // as apt components the given flow layer must see initialised.
        void AppendExpectedAptComponent(GuiFlow leFlow);

        // @0x82440AC0 (cpp:167) -- the selected rank's apt bundle has loaded: mount its movie
        // at level 4 by name, latch whether the profile carries a licence picture, and start
        // the gamerpic feed when it does not.
        void OnLoad();

        // @0x82440BC0 (cpp:200) -- unmount the licence movie, stop the gamerpic feed and hand
        // the rank tuples back to the GUI cache.
        void ReleaseResources();

        // @0x8241A848 (cpp:232) -- an apt clip has (re)loaded: re-push the matching text
        // field's cached text. Returns whether the trigger was this component's.
        bool HandleAptLoadTriggers(const CgsGui::GuiEventAptTriggerPayload* lpAptTrigger);

        // @0x8243BE20 (cpp:294) -- an apt transition finished on this component: advance the
        // presentation state machine. Returns whether the trigger was this component's.
        bool HandleAptTransitionTriggers(const CgsGui::GuiEventAptTriggerPayload* lpAptTrigger);

        // @0x8243C0B8 (cpp:399) -- the per-frame pump for the four timed / streaming states.
        void Update();

        // @0x8243C380 (cpp:551) -- supply everything the card displays and pick the rank
        // resources to stream. Argument order is the DWARF's
        // (const char*, bool, bool, int32_t, int32_t, bool, bool) and each slot is pinned by
        // the X360 prologue (r5 -> mbElite, r6 -> mbFinishedGame, r7 -> rank, r8 -> points,
        // r9 -> mbShowUpgradePending, r10 -> mbShowPoints).
        void SetPlayerInfo(const char* lpcPlayerName, bool lbElite, bool lbFinishedGame,
                           s32 liRank, s32 liPointsToNextRank,
                           bool lbShowUpgradePending, bool lbShowPoints);

        // @0x824277D0 (cpp:716) -- push "_x" / "_y" apt view states (immediate).
        void SetPosition(Vector2 lv2Position);

        // @0x82440C98 (cpp:743) -- bring the card on screen from E_LICENSE_FIRST_RESOURCE_LOADED.
        void ShowLicense(bool lbForceCentred);

        // @0x8243C7E0 (cpp:814) -- begin the rank-upgrade presentation.
        void ShowUpgradedLicense(f32 lfTimeToShowNextRank, bool lbShowPoints);

        // @0x82434998 (cpp:868) -- play the transition-out frame.
        void HideLicense();

        // @0x82434A30 (cpp:904) -- one more win in the current rank.
        void AddWin();

        // @0x8241AAD8 (cpp:988) -- reveal the points/wins line.
        void ShowScore();

        // @0x8243C918 (cpp:1019) -- rank up (the animated licence swap).
        void RankUp(f32 lfTimeToNextWinIncrement, f32 lfTimeToShowNextRank, bool lbShowPoints);

        // @0x82440E38 (cpp:1095) -- show/hide the card without a transition state change.
        void SetVisible(bool lbVisible);

        // DWARF h:325 / h:309 -- header inlines (no out-of-line body in the image).
        bool IsVisible() const { return mbVisible; }
        bool IsHiding()  const { return mbHiding; }

        // @0x824B31E8 (DWARF h:364) -- latch the GUI cache the component streams through.
        void SetCachePointer(GuiCache* lpGuiCache);

        // @0x824B3248 (DWARF h:382) -- latch the player profile and, when it changes, refresh
        // the licence-issued date field from the profile's date, formatted for the language.
        void SetProfilePointer(BrnProgression::Profile* lpProfile);

        // @0x8243CB90 (cpp:1198) -- called every frame by the owning state: keep pushing the
        // profile's licence picture at the view while the card is showing it.
        void SendPlayerPictureEvent();

        // @0x8241AB48 (cpp:1274) -- store the completion percentage and, on an elite licence,
        // print it into the upgrade line.
        void SetPercentageComplete(s32 liPercentageComplete);

        // @0x824B3300 (DWARF h:418) / @0x824B33F8 (DWARF h:446) -- stream the rank resources.
        bool EnsureResourcesAreLoaded();
        bool EnsureResourcesAreUnloaded();

        // DWARF h:464 -- header inline. "Ready" is the settled showing state: the card has
        // finished its transition in and is not mid-upgrade. (No out-of-line body exists and
        // no recovered caller inlines it, so only the DWARF's declaration is attested; the
        // predicate below is the one the state machine makes true exactly between
        // ShowLicense's transition-complete and HideLicense.)
        bool IsReady() { return meCurrentLicenseState == E_LICENSE_SHOWING_NORMAL; }

    private:
        // @0x8242D898 (cpp:1160) -- push the "apt_dirtLevel" control variable.
        void UpdateDirt();

        // @0x8241A4B8 (DWARF h:341) -- select the rank artwork (asserts 0..KI_MAX_RANK).
        void SetRank(s32 liRank);

        // @0x8243CC30 (cpp:1229) / @0x8243CD00 (cpp:1255) -- start/stop the Xbox gamer-picture
        // texture feed (GUI event 264).
        void StartOutputtingGamerpic();
        void StopOutputtingGamerpic();

        // ---- DWARF member list, in declaration order (guest offsets in comments) ----
        CgsGui::sResourceTuple   maLicenseResourcesToLoad[2];  // h:233  +0x94
        s32                      miNumResourcesToLoad;         // h:234  +0xA4
        GuiCache*                mpGuiCache;                   // h:236  +0xA8
        BrnProgression::Profile* mpProfile;                    // h:237  +0xAC
        TextField                mPlayerNameTextField;         // h:239  +0xB0    "playerName"
        TextField                mNextRankTextField;           // h:240  +0x1D8   "playerUpgrade"
        TextField                mDateIssuedTextField;         // h:241  +0x300   "IssuedOnText_cpt"
        TextField                mCompletionMonthField;        // h:243  +0x428   "MonthText_cpt"
        TextField                mCompletionDateField;         // h:244  +0x550   "DateText_cpt"
        TextField                mCompletionYearField;         // h:245  +0x678   "YearText_cpt"
        bool                     mbAtTopRank;                  // h:251  +0x7A0
        bool                     mbElite;                      // h:252  +0x7A1
        bool                     mbFinishedGame;               // h:253  +0x7A2
        s32                      miCurrentRank;                // h:255  +0x7A4
        s32                      miWinsInCurrentRank;          // h:256  +0x7A8
        s32                      miPercentComplete;            // h:258  +0x7AC
        bool                     mbVisible;                    // h:260  +0x7B0
        bool                     mbHiding;                     // h:261  +0x7B1
        ELicenseStates           meCurrentLicenseState;        // h:263  +0x7B4
        bool                     mbShowUpgradePending;         // h:265  +0x7B8
        bool                     mbShowPoints;                 // h:266  +0x7B9
        bool                     mbForceCentred;               // h:267  +0x7BA
        f32                      mfRequiredWinsTickUpRate;     // h:269  +0x7BC
        f32                      mfTimeToShowNextRank;         // h:271  +0x7C0
        f32                      mfTimeToNextWinIncrement;     // h:272  +0x7C4
        bool                     mbShowingProfilePicture;      // h:274  +0x7C8
    };
}

#endif // BRN_LICENSE_COMPONENT_H
