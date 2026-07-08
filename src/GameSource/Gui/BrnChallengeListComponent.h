#ifndef BRN_CHALLENGE_LIST_COMPONENT_H
#define BRN_CHALLENGE_LIST_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID (typedef u64)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent (base) + CgsGui::StateInterface
#include "GameSource/GameState/BrnGameStateSharedIO.h"      // BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData

// BrnGui::ChallengeListComponent -- the GUI "view challenges" list component.
//
// PATH NOTE: the DecFIGS DWARF homes this class at
// GameSource/Gui/Flow/Screen/Components/BrnChallengeListComponent.{h,cpp}. It was first
// committed here (GameSource/Gui/) as a one-member minimal slice for the standalone
// GetChallengeStyle accessor. To keep a SINGLE owning definition (no ODR fork), the full
// recovered class is grown in place here rather than re-defined under Components/.
//
// Layout recovered from BURNOUT_X360_ARTIST.XEX (Construct @0x8242D9A0 pins every member
// offset) + the DecFIGS DWARF (member names/types, BrnChallengeListComponent.h). The
// component derives from CgsGui::GuiComponent (guest base occupies [0, +0x8C); the first
// own member maacAptState lands at +0x8C). Host is 64-bit, so members are held BY NAME in
// DWARF order -- guest byte offsets are documented, not reproduced.

namespace BrnGui
{
    class GuiCache;   // committed: GameSource/Gui/BrnGuiCache.h (GetFreeburnChallengeList)
}

namespace BrnResource
{
    class  ChallengeList;        // committed: SharedClasses/DataLists/ChallengeList.h
    struct ChallengeListEntry;   // committed: SharedClasses/DataLists/ChallengeListEntry.h
}

namespace BrnGui
{

class ChallengeListComponent : public CgsGui::GuiComponent
{
public:
    // BrnChallengeListComponent.h:113/114 -- the on-screen window size and the apt-state
    // string buffer length (both proven by Construct's SnPrintf loop: 5 iterations, 16-byte
    // destinations).
    static const s32 KI_MAX_DISPLAYABLE_CHALLENGES = 5;
    static const s32 KI_MAX_STATE_STRING_LENGTH    = 16;

    // @0x8242D9A0 -- chain the base GuiComponent::Construct, seed the per-slot apt-state
    // names ("apt_state_%d" / "apt_text_%d"), clear the view state, and Construct the
    // embedded every-player completion-status block.
    virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                           const char* lpacParentName);

    // @0x82434CC8 -- rebuild the apt output for the visible window (declared-only in this
    // slice; blocked -- needs the CgsLanguage formatting entry sub_82866450 + the
    // off_82F253xx state-name table, both un-homed).
    void Update();

    // @0x82440F90 / @0x82441078 -- move the highlight up / down (declared-only in this
    // slice; blocked -- both post a BrnGui::GuiAudioTriggerEvent, whose type/home and the
    // StateInterface OutputGuiEvent<> posting helper are not yet reconstructed).
    bool HighlightPrevious();
    bool HighlightNext();

    // @0x82441170 -- bind the component to a GuiCache's freeburn challenge list, count the
    // challenges matching liNumPlayers, and refresh the ticker.
    void Setup(GuiCache* lpGuiCache, s32 liNumPlayers, bool lbShowButton);

    // @0x82435040 -- the CgsID of the currently highlighted challenge (0 if none).
    CgsID GetHighlightedChallengeID() const;

    // BrnChallengeListComponent.h:155/169/179/185 -- declared-only (out of this slice).
    bool IsChallengeSelectable();
    void ShowButton(bool lbShow);
    bool IsAtTopOfList() const;
    bool IsAtBottomOfList() const;

    // @0x8248FB58 -- resolve lID to its list entry and forward the entry's freeburn style.
    // Kept at its committed s32 return (the DecFIGS DWARF types it
    // BrnResource::ChallengeListEntry::EFreeburnChallengeStyle; the X360 pseudo types it int
    // and the original commit chose s32 -- preserved so its committed body still matches).
    s32 GetChallengeStyle(CgsID lID) const;

    // @0x8241B618 -- store the incoming every-player completion status (a whole-block copy)
    // and mark the view dirty.
    void HandleEveryPlayerCompletionStatus(
        const BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData* lpStatus);

private:
    // @0x8242DA38 -- return the liWantedIndex-th challenge (of those matching miNumPlayers)
    // whose content is bought, optionally reporting its raw list index. 0 if none.
    const BrnResource::ChallengeListEntry* GetFilteredChallenge(s32 liWantedIndex,
                                                                s32* lpiOutIndex) const;

    // @0x8242D??? -- push the highlighted challenge description to the ticker (declared-only;
    // its own ledger TU).
    void ShowDescriptionInTicker();

    // ---- layout (DWARF order; guest offsets from Construct @0x8242D9A0) ----
    char maacAptState[KI_MAX_DISPLAYABLE_CHALLENGES][KI_MAX_STATE_STRING_LENGTH];     // +0x8C
    char maacAptStateText[KI_MAX_DISPLAYABLE_CHALLENGES][KI_MAX_STATE_STRING_LENGTH]; // +0xDC

    GuiCache*                          mpGuiCache;      // +0x12C
    const BrnResource::ChallengeList*  mpChallengeList; // +0x130

    // +0x138 -- the every-player freeburn completion-status block (2104 bytes; Construct
    // calls FburnChallengeEveryPlayerStatusData::Construct on it and HandleEveryPlayer...
    // block-copies 2104 bytes into it). The DWARF names the member type
    // GuiEventFburnChallengeEveryPlayerStatus; the X360 constructs/copies it as the
    // committed 2104-byte FburnChallengeEveryPlayerStatusData, which we model directly.
    BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData mEveryPlayerCompletionStatus;

    s32  miNumPlayers;           // +0x970 -- the player-count filter (challenges to show)
    s32  miNumChallenges;        // +0x974 -- count of challenges matching miNumPlayers
    s32  miStartChallengeIndex;  // +0x978 -- scroll-window base index
    s32  miHighlightedIndex;     // +0x97C -- highlight offset within the window
    bool mbDirty;                // +0x980 -- view needs a rebuild
    bool mbShowButton;           // +0x981 -- show the select button
};

} // namespace BrnGui

#endif // BRN_CHALLENGE_LIST_COMPONENT_H
