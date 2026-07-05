#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                       // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"                          // GameStateModuleIO::EPlayerTeam / EGameModeType
#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionSingle.h"         // PlayerPositionSingleComponent + PlayerPositionSingleData + enums
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"   // BrnFlaptComponent (base)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                          // MovieClipRef (embedded x2)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                          // TextFieldRef (embedded x2)

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable.h
//
// BrnGui::PlayerPositionTableComponent - the in-race HUD player-position table:
// a fixed KI_MAX_BARS_NEEDED (9) array of PlayerPositionSingleComponent rows,
// fed from a parallel array of PlayerPositionSingleData records (both defined in
// BrnPlayerPositionSingle.h) that are (re)built each frame, sorted per game
// mode, then pushed into the rows.
//
// Class shape / member names / offsets from the DecFIGS DWARF
// (BrnPlayerPositionTable.h) gated on the X360 ledger. The PlayerPositionSingleData
// record and the PlayerTypes/RevengeStatus/AwardStatus/HeadsetStatus enums are
// NOT redeclared here -- they have a committed home in BrnPlayerPositionSingle.h,
// which this header includes. Full method surface declared for coherence; only
// ClearStoredData / AddInvisibleTeamLine / FunctionSortTeamHighToLow / SetCache
// are bodied in this TU so far.
//
// X360 byte offsets (access BY NAME): vtable+base @0x00..0x0B,
// maPlayerComponents[9] @0x0C (stride 0xCC), mpCache @0x738,
// mpChallengeManager @0x73C, meGameMode @0x740, miCurrentBarsUsed @0x744,
// maPlayerSingleData[9] @0x748 (stride 0x38 == sizeof(PlayerPositionSingleData)),
// then meLastFrameSkillState / mbFreeburnChallengeRunning /
// miFreeburnChallengeCurrentData / meCurrentChallengeDataType + the title/skill
// clip & text refs.
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class GuiCache;
    struct FreeburnChallengeManager;
    struct GuiEventRaceDistanceRemaining;

    static const s32 KI_MAX_BARS_NEEDED = 9;   // DWARF h:128

    // DWARF h:58 -- shared base (adds nothing over the flapt component base).
    struct BasePlayerPositionTableComponent : public BrnFlaptComponent
    {
    };

    struct PlayerPositionTableComponent : public BasePlayerPositionTableComponent
    {
        // Public ledger surface (DWARF h:84..123). Only SetCache is bodied here.
        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lacParentName);
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);
        void SetupGameMode();
        void UpdatePositionDetails(const GuiEventRaceDistanceRemaining* lpDistanceEvent);
        bool HandleFrameTrigger(const char* lacName, s32 liFrame);
        void SetCache(GuiCache* lpCache);   // @0x82473458 (this TU)

    private:
        // Private ledger surface (DWARF h:180..256). Bodied here:
        // ClearStoredData / AddInvisibleTeamLine / FunctionSortTeamHighToLow.
        void ClearStoredData();                                    // @0x8241EEF0 (this TU)
        void FillOutInActiveRaceCarOrder(const GuiEventRaceDistanceRemaining* lpDistanceEvent);
        void FillOutOnlineData(EActiveRaceCarIndex leCurrentActiveRaceCar);
        void FillOutOnlineValueData(EActiveRaceCarIndex leCurrentActiveRaceCar,
                                    const GuiEventRaceDistanceRemaining* lpDistanceEvent);
        void DisplayData();
        void SortData();
        void CountLinesAndAddTotal();
        void AddInvisibleTeamLine();                               // @0x82413BC8 (this TU)
        static s32 FunctionSortHighToLow(const void* lp1, const void* lp2);
        static s32 FunctionSortLowToHigh(const void* lp1, const void* lp2);
        static s32 FunctionSortLowToHighZeroInvalid(const void* lp1, const void* lp2);
        static s32 FunctionSortTeamLowToHigh(const void* lp1, const void* lp2);
        static s32 FunctionSortTeamHighToLow(const void* lp1, const void* lp2); // @0x82413A20 (this TU; asm-attested, not in DWARF method list)
        static s32 FunctionSortAlphabetical(const void* lp1, const void* lp2);
        void ProcessOnlineFreeburnTable();
        void SetTitleText(const char* lpcText);
        void SetSkillsText(const char* lpcText);
        void SetValuesDirty();

        PlayerPositionSingleComponent maPlayerComponents[KI_MAX_BARS_NEEDED]; // +0x0C  (DWARF h:131, stride 0xCC)
        GuiCache*                     mpCache;                    // +0x738 (DWARF h:134)
        const FreeburnChallengeManager* mpChallengeManager;       // +0x73C (DWARF h:135)
        BrnGameState::GameStateModuleIO::EGameModeType meGameMode; // +0x740 (DWARF h:137)
        s32                           miCurrentBarsUsed;          // +0x744 (DWARF h:138)
        PlayerPositionSingleData      maPlayerSingleData[KI_MAX_BARS_NEEDED]; // +0x748 (DWARF h:142, stride 0x38)
        // FLAG: the two enum fields below are raw s32 here (both enums are s32-width): their
        // homes (BurnoutSkillzData::EBurnoutSkillType / ChallengeListEntryAction::EChallengeDataType)
        // pull in heavy game-state/challenge headers, and no function bodied in this TU touches
        // them -- kept for layout coherence only. Adopt the real enum types when this TU grows
        // the freeburn-challenge table logic that uses them.
        s32                           meLastFrameSkillState;      // (DWARF h:144, EBurnoutSkillType)
        bool                          mbFreeburnChallengeRunning; // (DWARF h:145)
        s32                           miFreeburnChallengeCurrentData; // (DWARF h:146)
        s32                           meCurrentChallengeDataType; // (DWARF h:148, EChallengeDataType)
        BrnFlapt::MovieClipRef        mTitleBarMCR;               // (DWARF h:169)
        BrnFlapt::TextFieldRef        mTitleText;                 // (DWARF h:171)
        BrnFlapt::TextFieldRef        mSkillzText;                // (DWARF h:172)
        BrnFlapt::MovieClipRef        mPageIconMCR;               // (DWARF h:174)
    };
}
