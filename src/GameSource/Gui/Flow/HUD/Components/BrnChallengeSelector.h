#pragma once

// BrnChallengeSelector.h
// Home of BrnGui::ChallengeSelector -- the in-race HUD component that lets the player
// scroll through the freeburn challenges currently available to the party, showing each
// challenge's localised description/title in an embedded text field and posting a
// "select challenge" GUI event when one is chosen. Derives from CgsGui::GuiComponent
// (the apt-view-state / output-event plumbing). Layout + method set from the DecFIGS
// DWARF (GameSource/Gui/Flow/Hud/Components/BrnChallengeSelector.h), X360-attested per
// the ledger.
//
// LAYOUT (X360 authoritative; offsets are the store/load displacements in Construct and
// the accessors, guest 32-bit byte offsets -- the PC gate compiles 64-bit so members are
// reached BY NAME):
//   +0x88   mpStateInterface  -- base GuiComponent member (a1[34])
//   +0x8C   mTextfield        -- BrnGui::TextField, sizeof 0x128 (embedded by value)
//   +0x1B4  mau16AvailableChallengeIndexToChallengeIndexMapping[2000]  (u16; @436)
//   +0x1154 miAvailableChallenges                                       (@4436)
//   +0x1158 miCurrentAvailableChallengeIndex                            (@4440)
//   +0x115C mpChallengeList                                             (@4444)
//   +0x1160 mpGuiCache                                                  (@4448)
//   +0x1164 mbIsHost                                                    (@4452)
//   +0x1165 mbVisible                                                   (@4453)

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID (typedef u64)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"  // CgsGui::GuiComponent base
#include "GameSource/Gui/BrnGuiTextField.h"                          // BrnGui::TextField (by value)

namespace CgsGui { struct StateInterface; struct GuiEventControllerInputPressed; }
namespace BrnResource { class ChallengeList; struct ChallengeListEntry; }

namespace BrnGui
{
    class GuiCache;

    struct ChallengeSelector : public CgsGui::GuiComponent
    {
        // @ 0x824158F0 -- base Construct, then construct the embedded "Text_mc" field and
        // clear the challenge/index/pointer/flag members.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName) override;

        // ---- declared-only (each reconstructed in its own TU) ----
        void Show();
        void Hide();
        bool IsVisible();
        void SetChallengeList(const BrnResource::ChallengeList* lpChallengeList);
        void SetGuiCachePointer(GuiCache* lpGuiCache);
        void HandleControllerInput(const CgsGui::GuiEventControllerInputPressed* lpEvent);
        void SelectAvailableChallengeByID(CgsID lID, bool lbSelect);
        void SelectAvailableChallenge(s32 liAvailableChallengeIndex, bool lbSelect);

        // ---- reconstructed in this TU ----
        // @ 0x8242C270 -- rebuild the available-challenge mapping table for the given party
        // player count; returns the new available-challenge count.
        s32 SetAvailableChallenges(s32 liNumPlayers);

        // @ 0x824159E0 -- current number of available challenges.
        s32 GetAvailableChallengeCount();

        // @ 0x824159E8 -- respond to an apt component-load notification: re-push the text
        // field's stored text, or publish the host/client transition-in view-state.
        void HandleLoadNotification(const char* lpacName);

    private:
        // @ 0x82415960 -- map an available-challenge slot index to its full challenge-list
        // index (via the mapping table); range-guarded.
        s32 GetChallengeIndex(s32 liAvailableChallengeIndex);

        // declared-only (own TU)
        const BrnResource::ChallengeListEntry* GetAvailableChallengeData(s32 liAvailableChallengeIndex);

        // ---- layout (DWARF offsets; see header banner) ----
        static const s32 KI_MAX_PROFILE_CHALLENGES = 2000;   // mapping-table capacity

        BrnGui::TextField                mTextfield;                                                     // +0x8C
        u16                              mau16AvailableChallengeIndexToChallengeIndexMapping[KI_MAX_PROFILE_CHALLENGES]; // +0x1B4
        s32                              miAvailableChallenges;             // +0x1154
        s32                              miCurrentAvailableChallengeIndex;  // +0x1158
        const BrnResource::ChallengeList* mpChallengeList;                  // +0x115C
        GuiCache*                        mpGuiCache;                        // +0x1160
        bool                             mbIsHost;                          // +0x1164
        bool                             mbVisible;                         // +0x1165

        static const char KAC_TEXT_FIELD_NAME[8];   // "Text_mc"
    };
}
