#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                       // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"                          // GameStateModuleIO::EPlayerTeam
#include "GameSource/GameState/BrnCgsPlayerName.h"                              // CgsNetwork::PlayerName (mPlayerName, 16 bytes)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"   // BrnFlaptComponent (base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h" // FlaptAnimatorComponent (embedded x2)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                          // MovieClipRef (embedded x2)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                          // TextFieldRef (embedded x2)

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionSingle.h
//
// BrnGui::PlayerPositionSingleComponent - one row of the HUD player-position
// list: the player's name text, a value text (time/score/distance formatted per
// game mode / skill type), the revenge + headset icons, and two colour
// animators. Class shape / member names / enums from the DecFIGS DWARF
// (BrnPlayerPositionSingle.h), gated on the X360 ledger. This TU bodies
// Construct / Prepare / RenderValue; the rest of the DWARF surface is
// declaration-only (their own ledger functions).
//
// X360 word offsets (documented; access BY NAME): base {iface[0], mAptRef[1..2]},
// mPlayerColourAnimator[3..16], mPlayerTextColourAnimator[17..30], mRevenge[31..32],
// mAudio[33..34], mName[35..37], mValue[38..40], mePlayerType[41]=7,
// meActiveRaceCarIndex[42]=-1, meHeadsetStatus[43], mePlayerColour[44],
// meRevengeStatus[45]=3, meAwardState[46], mfValue[47], mbValueChanged@+192,
// mpCache[49], mpLanguageManager[50].
// ============================================================================

namespace CgsLanguage { class LanguageManager; }

namespace BrnGui
{
    class GuiCache;

    // DWARF BrnPlayerPositionSingle.h:61 -- the row's display persona.
    enum PlayerTypes
    {
        E_PLAYERTYPES_BASIC                          = 0,
        E_PLAYERTYPES_FREEBURN_CHALLENGE_OFF         = 1,
        E_PLAYERTYPES_FREEBURN_CHALLENGE_NOT_ACTIVE  = 2,
        E_PLAYERTYPES_FREEBURN_CHALLENGE_IN_PROGRESS = 3,
        E_PLAYERTYPES_FREEBURN_CHALLENGE_COMPLETE    = 4,
        E_PLAYERTYPES_FREEBURN_CHALLENGE_TOTAL       = 5,
        E_PLAYERTYPES_NEW_PLAYER                     = 6,
        E_PLAYERTYPES_INVISIBLE                      = 7,
        E_PLAYERTYPES_COUNT                          = 8,
        E_PLAYERTYPES_FIRST                          = 0,
    };

    // DWARF h:78.
    enum RevengeStatus
    {
        E_REVENGESTATUS_MARKEDMAN = 0,
        E_REVENGESTATUS_UP        = 1,
        E_REVENGESTATUS_DOWN      = 2,
        E_REVENGESTATUS_INVISIBLE = 3,
        E_REVENGESTATUS_COUNT     = 4,
    };

    // DWARF h:90.
    enum AwardStatus
    {
        E_AWARDSTATUS_NONE    = 0,
        E_AWARDSTATUS_CROWNED = 1,
        E_AWARDSTATUS_REEFED  = 2,
        E_AWARDSTATUS_BOTH    = 3,
        E_AWARDSTATUS_COUNT   = 4,
    };

    // DWARF h:101.
    enum HeadsetStatus
    {
        E_HEADSETSTATUS_INVISIBLE = 0,
        E_HEADSETSTATUS_OFF       = 1,
        E_HEADSETSTATUS_ON        = 2,
        E_HEADSETSTATUS_ACTIVE    = 3,
        E_HEADSETSTATUS_COUNT     = 4,
    };

    // DWARF BrnPlayerPositionSingle.h -- one built-then-sorted player-position record.
    // The PlayerPositionTableComponent keeps a parallel array of these (stride 0x38 ==
    // sizeof) alongside its row components; ClearStoredData/AddInvisibleTeamLine build
    // them, SortData/FunctionSort* order them, DisplayData pushes them into the rows.
    // X360 byte offsets (access BY NAME): mPlayerName @+0x00 (CgsNetwork::PlayerName, 16),
    // meNameType @+0x10, mfTableValue @+0x14, mePlayerType @+0x18, meActiveRaceCarIndex
    // @+0x1C, meHeadsetStatus @+0x20, meRevengeStatus @+0x24, meAwardState @+0x28,
    // meTeam @+0x2C, mePlayerColour @+0x30 (EGuiPlayerColours; raw s32 -- enum home
    // pending, same as the sibling component), miHoldingSlot @+0x34 (s8).
    struct PlayerPositionSingleData
    {
        CgsNetwork::PlayerName mPlayerName;            // +0x00
        s32                    meNameType;             // +0x10
        f32                    mfTableValue;           // +0x14
        PlayerTypes            mePlayerType;           // +0x18
        EActiveRaceCarIndex    meActiveRaceCarIndex;   // +0x1C
        HeadsetStatus          meHeadsetStatus;        // +0x20
        RevengeStatus          meRevengeStatus;        // +0x24
        AwardStatus            meAwardState;           // +0x28
        BrnGameState::GameStateModuleIO::EPlayerTeam meTeam;   // +0x2C
        s32                    mePlayerColour;         // +0x30 (EGuiPlayerColours; raw s32)
        s8                     miHoldingSlot;          // +0x34
    };

    // DWARF h:53 -- the shared base of the player-position row components (adds
    // nothing over the flapt component base).
    struct BasePlayerPositionSingleComponent : public BrnFlaptComponent
    {
    };

    struct PlayerPositionSingleComponent : public BasePlayerPositionSingleComponent
    {
        // @0x82413C58 (this TU, DWARF h:154) -- reset the row to invisible defaults,
        // latch the language manager, construct the two colour animators.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x82421D78 (this TU, DWARF h:162) -- bind the row's clip + children out of
        // the loaded flapt file.
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // @0x82421F78 (this TU, DWARF h:191) -- format mfValue into the value text
        // field per game mode / challenge target / today's-best skill.
        void RenderValue();

        // DWARF h:169/h:182/h:187/h:195 -- their own ledger functions
        // (declaration-only here).
        void Update(struct PlayerPositionSingleData* lpData);
        void SetInvisible();
        void SetCache(GuiCache* lpCache);
        f32 GetValue() const { return mfValue; }   // trivial accessor (header-inline)

        // ADDITIVE GROW ([stuntrace F2] wave, 2026-08-27). DWARF-declared PUBLIC
        // (BrnPlayerPositionSingle.h:199 `void SetValueChanged()`, and the DWARF's own
        // BrnPlayerPositionTable.cpp:1334 shows PlayerPositionTableComponent::SetValuesDirty
        // as a loop over it), but the X360 compiler INLINED it at every call site -- there
        // is no symbol for it in scratch/func_index.tsv -- so the body lands here.
        // Attested by the raise-loop the sibling table's SetupGameMode @0x8243E6F8 emits
        // twice (@0x8243E7A8 and @0x8243E818):
        //     addi r10, r31, 0xCC      ; == maPlayerComponents[0] (+0x0C) + 0xC0
        //     li   r11, 9              ; KI_MAX_BARS_NEEDED rows
        //     li   r9, 1
        //     stb  r9, 0(r10) ; addi r10, r10, 0xCC   ; stride == sizeof(row)
        // -- +0xC0 is mbValueChanged. A bare named-member store; no assert on console.
        void SetValueChanged() { mbValueChanged = true; }

    private:
        // DWARF cpp:868/cpp:889 -- the "wow" gates (value beats the per-type limit).
        // No standalone X360 symbols (inlined into RenderValue @0x8242212C/@0x8242241C);
        // recovered here from those instances.
        bool IsChallengeWow(s32 leDataType, f32 lfValue) const;
        bool IsTodaysBestWow(s32 leSkillType, f32 lfValue) const;

        // DWARF h:252/h:253 -- the per-type "wow" limits (XEX .rodata
        // @0x82F25680 / @0x82F2564C).
        static const f32 KAF_CHALLENGE_WOW_LIMITS[24];
        static const f32 KAF_TODAYS_BEST_WOW_LIMITS[12];

        FlaptAnimatorComponent mPlayerColourAnimator;      // [3]  (DWARF h:228)
        FlaptAnimatorComponent mPlayerTextColourAnimator;  // [17] (DWARF h:229)
        BrnFlapt::MovieClipRef mRevenge;                   // [31] (DWARF h:231)
        BrnFlapt::MovieClipRef mAudio;                     // [33] (DWARF h:232)
        BrnFlapt::TextFieldRef mName;                      // [35] (DWARF h:234)
        BrnFlapt::TextFieldRef mValue;                     // [38] (DWARF h:235)
        PlayerTypes            mePlayerType;               // [41] (DWARF h:240)
        EActiveRaceCarIndex    meActiveRaceCarIndex;       // [42] (DWARF h:241)
        HeadsetStatus          meHeadsetStatus;            // [43] (DWARF h:242)
        s32                    mePlayerColour;             // [44] (DWARF h:243 EGuiPlayerColours; raw s32 -- enum home pending)
        RevengeStatus          meRevengeStatus;            // [45] (DWARF h:244)
        AwardStatus            meAwardState;               // [46] (DWARF h:245)
        f32                    mfValue;                    // [47] (DWARF h:246)
        bool                   mbValueChanged;             // +192 (DWARF h:247)
        GuiCache*              mpCache;                    // [49] (DWARF h:249)
        CgsLanguage::LanguageManager* mpLanguageManager;   // [50] (DWARF h:250)
    };
}
