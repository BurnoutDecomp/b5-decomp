#pragma once

// ===================================================================================
// BrnGui::RoadPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h
//
// The Road Rules HUD panel: a road-sign icon plus two scoreboard boxes
// (BrnGui::RoadPanelData) the crash-nav map fills in, driven by the currently selected
// road rule (time vs crash) and the offline/online scoring mode.
//
// DECFIGS DWARF EXISTS FOR THIS TU:
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h
// It supplies BrnGui::RoadPanelData (namespace scope, h:52) and its nested PanelBox (h:65)
// in full, and `struct BrnGui::RoadPanel : public BrnGui::IconComponent` with the member
// run mRoadSign / mRoadPanelData / meIcon / mNames[4] / mScores[4] / mTargetCaption /
// mBestScoreBackingAnimation / meCurrentRule / meCurrentScoreMode / mbActive.
//
// ⭐ CORRECTED 2026-08-29 (main-menu wave F1). An older revision modelled the panel as an
// 18-row bank of 162-byte "FriendRow" records at +0x1A2. That was WRONG: the 0xA2 stride is
// `sizeof(RoadPanelData::PanelBox)` and +0x1A2 is `mRoadPanelData.mPanels[0].mNames[1]`.
// The bank never existed.
//
// ⭐⭐ TEXTFIELD CARVE RETIRED 2026-08-29 (main-menu wave G1). The +0x2A8..+0xD9B span was a
// single `maTextFieldsReserved` blob; RoadPanel::Construct @0x82425738 and
// UpdateVisibleScores @0x82425938 pin every member of it, and it is spelled out below.
//
// MEASURED X360 offsets (documentation only -- the host layout is name-based and every
// embedded pointer widens; all access below is BY NAME):
//   +0x0000  IconComponent base    (`IconComponent::Construct(this, name, si, 0, parent)`)
//   +0x00A0  mRoadSign             (BrnGui::RoadSignIcon -- STILL A CARVE, see below)
//   +0x0160  mRoadPanelData        (RoadPanelData, 0x144 -- CrashNavPanel memcpy's here)
//   +0x02A4  meIcon                (BrnGui::ERoadIcon; Construct stores 64)
//   +0x02A8  mNames[4]             (TextField, 0x128 stride -> +0x3D0/+0x4F8/+0x620)
//   +0x0748  mScores[4]            (TextField           -> +0x870/+0x998/+0xAC0)
//   +0x0BE8  mTargetCaption        (TextField "BestScoreText")
//   +0x0D10  mBestScoreBackingAnimation (AnimationComponent "BestTimeBackingAnimation", 0x8C)
//   +0x0D9C  meCurrentRule         (BrnStreetData::ScoreType; Construct stores 2)
//   +0x0DA0  meCurrentScoreMode    (GuiEventSetRoadRuleScoreMode::ERoadPanelModes)
//   +0x0DA4  mbActive
//   sizeof == 0xDB0
//
// ⛔ mRoadSign IS STILL A RESERVED CARVE, DELIBERATELY. BrnGui::RoadSignIcon has no home in
// the tree (its canonical home is GameSource/Gui/SatNav/BrnRoadSignIconManager.{h,cpp}, per
// the assert file path in RoadSignIcon::Construct @0x824F5170) and homing it would drag in
// RoadSignIconManager's eight siblings -- a separate TU this wave does not own. The four
// RoadSignIcon entry points this panel calls are routed through a documented file-local
// boundary in BrnRoadPanel.cpp; see the DELETE-WHEN note there.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/BrnGuiTextField.h"                            // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"             // BrnGui::IconComponent (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h" // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiFlow
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"    // CgsGui::StateInterface

namespace BrnGui
{
    class GuiCache;

    // ------------------------------------------------------------------------------
    // BrnGui::RoadPanelData -- the road-rule scoreboard payload a screen fills in and
    // hands to CrashNavPanel::SetRoadPanelData, which memcpy's it into the embedded road
    // panel. NAMESPACE SCOPE, not nested in RoadPanel (DWARF BrnRoadPanel.h:52).
    //
    // The layout is X360-CORROBORATED: PanelBox's members sum to 2 + 32 + 64 + 64 == 162 ==
    // 0xA2, exactly the stride RoadPanelData::Construct walks (`addi r29, r29, 0xA2`
    // @0x82425724, loop count `li r30, 2` @0x824256F4), and 2 * 0xA2 == 0x144 is exactly the
    // byte count CrashNavPanel::SetRoadPanelData memcpy's (@0x8243AA08). The whole record is a
    // pointer-free scalar run, so it is host-stable -- hence the static_asserts are absolute.
    // ------------------------------------------------------------------------------
    struct RoadPanelData
    {
        static const s32 KI_ROADRULE_COUNT     = 2;    // DWARF h:55
        static const s32 KI_PANEL_TEXT_LENGTH  = 32;   // DWARF h:57

        // One scoreboard box: the local player's own score plus two labelled rows (the
        // friend row and the road-ruler row), each with a "the player has beaten it" flag.
        struct PanelBox
        {
            // @ 0x82418060 (DWARF h:77). Five strings then the two flags, in the order the
            // assert literal lists them ("lpYourScore && lpName1 && lpName2 && lpScore1 &&
            // lpScore2" -- the null-check order @0x824180B0.. is p1,p2,p4,p3,p5, which is
            // what types the parameters below).
            //
            // ⚠️ MEASURED ROW MAPPING (X360 @0x82418394..0x824183F0): the "1" pair lands in
            // array slot 1 and the "2" pair in slot 0 --
            //   mPlayerScores       <- lpacYourScore (SPrintf this+2)
            //   mNames[1]/mScores[1] <- lpacName1 / lpacScore1 (SPrintf this+66 / this+130)
            //   mNames[0]/mScores[0] <- lpacName2 / lpacScore2 (SPrintf this+34 / this+98)
            //   mabPlayerBestScore[1] <- lbBeaten1 (stb this+1)
            //   mabPlayerBestScore[0] <- lbBeaten2 (stb this+0)
            // The mapping is consistent across all three pairs, so it is reproduced as-is
            // rather than "corrected". FLAG: the parameter NAMES are inferred from the
            // assert literal; the DWARF carries no parameter names.
            void Construct(const char* lpacYourScore, const char* lpacName1,
                           const char* lpacScore1, const char* lpacName2,
                           const char* lpacScore2, bool lbBeaten1, bool lbBeaten2);

            bool mabPlayerBestScore[KI_ROADRULE_COUNT];             // box +0x00 (DWARF h:79)
            char mPlayerScores[KI_PANEL_TEXT_LENGTH];               // box +0x02 (DWARF h:80)
            char mNames[KI_ROADRULE_COUNT][KI_PANEL_TEXT_LENGTH];   // box +0x22 (DWARF h:81)
            char mScores[KI_ROADRULE_COUNT][KI_PANEL_TEXT_LENGTH];  // box +0x62 (DWARF h:82)
        };

        // @ 0x824256E0 (DWARF h:62) -- blank both boxes.
        void Construct();

        PanelBox mPanels[KI_ROADRULE_COUNT];   // DWARF h:85
    };

    static_assert(sizeof(RoadPanelData::PanelBox) == 0xA2,
                  "RoadPanelData::PanelBox is the 0xA2 stride RoadPanelData::Construct walks");
    static_assert(sizeof(RoadPanelData) == 0x144,
                  "RoadPanelData is the 0x144 blob CrashNavPanel::SetRoadPanelData memcpy's");

    class RoadPanel : public IconComponent
    {
    public:
        // GuiEventSetRoadRuleScoreMode::ERoadPanelModes, as this panel's asserts spell it
        // (`cmpwi 1` on +0xDA0; COUNT (2) is the "unset" value GuiCache is asserted against
        // in Construct, BrnGuiCache.h:4210).
        static const s32 KI_ROAD_PANEL_MODE_OFFLINE = 0;
        static const s32 KI_ROAD_PANEL_MODE_ONLINE  = 1;
        static const s32 KI_ROAD_PANEL_MODE_COUNT   = 2;

        // BrnStreetData::ScoreType, as this panel's asserts spell it (`cmplwi 2` bound in
        // SetCurrentRule; Construct parks meCurrentRule at COUNT == "no rule chosen yet",
        // which is why SetCurrentRule has a third from-state).
        static const s32 KI_SCORE_TYPE_TIME  = 0;   // BrnStreetData::E_SCORE_TYPE_START
        static const s32 KI_SCORE_TYPE_CRASH = 1;   // BrnStreetData::E_SCORE_TYPE_CRASH
        static const s32 KI_SCORE_TYPE_COUNT = 2;

        // The sign-colour codes GetSignColour answers with (the values the X360 returns:
        // 1 when neither box is beaten, 2 when the selected rule's box is, 3 when BOTH are).
        static const s32 KI_SIGN_COLOUR_NONE = 1;
        static const s32 KI_SIGN_COLOUR_ONE  = 2;
        static const s32 KI_SIGN_COLOUR_BOTH = 3;

        // Which of the two scoreboard rows is the FRIEND row. Measured: GetSelectedFriendName
        // returns `this + 0x1A2 + 0xA2*meCurrentRule`, and 0x1A2 == mRoadPanelData (+0x160)
        // + PanelBox::mNames (+0x22) + one 32-byte name == mPanels[0].mNames[1].
        static const s32 KI_FRIEND_ROW = 1;
        // The offline (road-ruler) row is the other one.
        static const s32 KI_RULER_ROW  = 0;

        // The four scoreboard rows the panel renders: rows 1 and 3 are the LOCAL PLAYER's own
        // rows for boxes 0 and 1; rows 0 and 2 are the opposing (friend / road-ruler) rows.
        // Proven by UpdateVisibleScores' targets -- this+0x2A8/0x3D0/0x4F8/0x620 for the names
        // and this+0x748/0x870/0x998/0xAC0 for the scores.
        enum ERow
        {
            E_ROW_BOX0_OPPONENT = 0,
            E_ROW_BOX0_PLAYER   = 1,
            E_ROW_BOX1_OPPONENT = 2,
            E_ROW_BOX1_PLAYER   = 3,
            E_ROW_COUNT         = 4,
        };

        // DWARF BrnRoadPanel.cpp:38 -- the two road-rule second-level filter option labels.
        // PUBLIC static (the DWARF lists it ahead of the class's first `private:`), because
        // CrashNavPanel::RefreshSecondLevelFilter passes the table straight to
        // MenuToggleGroupVarSize<3>::SetupToggle (`addi r8, r11, off_82F251D8` ->
        // "$CN_LEGEND_RR_TIMES", @0x8243AE04). Definition belongs to the RoadPanel TU.
        static const char* KAPC_RR_FILTER_OPTIONS[KI_SCORE_TYPE_COUNT];

        // @0x82425738 (DWARF cpp:127) -- IconComponent virtual, vtable slot 0
        // (CrashNavPanel::Construct reaches it through the slot @0x82425F44).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82418410 (DWARF cpp:172).
        void AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache);

        // DWARF cpp:203 -- no standalone X360 symbol: the console INLINES it into
        // CrashNavPanel::SetRoadPanelData @0x8243A9E4..0x8243AA14 (FindRoadFromName +
        // DisplayRoad on mRoadSign, then `memcpy 0x144` into mRoadPanelData, then
        // UpdateVisibleScores). Bodied here, which is what the DWARF method set says.
        void SetRoadPanelData(const char* lpacRoadName, RoadPanelData& lrData);

        // @0x8242D1B8 (cpp:226) / @0x8243A3E0 (cpp:298) / @0x8243A490 (cpp:329) /
        // @0x824184D0 (cpp:360).
        void SetCurrentRule(s32 leRule);
        void SwitchScoreMode();
        void TransitionIn();
        void TransitionOut();

        // @0x82417FF8 (DWARF h:229) -- the name string of the currently-selected friend row.
        const char* GetSelectedFriendName() const;

        // DWARF h:199 / h:214 -- header-inline on the console (CrashNavPanel's two road-rule
        // accessors inline them as a bare `lwz` at +0xD9C / +0xDA0).
        //
        // DWARF DELTA: declared `BrnStreetData::ScoreType GetCurrentRule() const` and
        // `GuiEventSetRoadRuleScoreMode::ERoadPanelModes GetScoringMode() const`. Kept as
        // s32 here because the committed consumers compare the results against plain
        // integers; retyping them is a separate, consumer-visible change.
        s32 GetCurrentRule() const  { return meCurrentRule; }
        s32 GetScoringMode() const  { return meCurrentScoreMode; }

    private:
        // @0x82425938 (this TU) -- repaint every row + the "best score" caption + the sign
        // colour from the current rule / scoring mode. Called by SetRoadPanelData,
        // SetCurrentRule and SwitchScoreMode.
        void UpdateVisibleScores();

        // @0x82418550 (this TU) -- which colour the road sign should be, from the two boxes'
        // "player has beaten it" flags for the CURRENT scoring mode.
        s32 GetSignColour() const;

        // ---- DWARF member run (X360 offsets are documentation only) -------------------
        // Reserved carve stands in ONLY for mRoadSign, whose type has no committed home.
        u8            maRoadSignReserved[0xC0];    // +0x00A0  mRoadSign (BrnGui::RoadSignIcon,
                                                   //          DWARF h:154; see the header banner)
        RoadPanelData mRoadPanelData;              // +0x0160  (DWARF h:156)
        s32           meIcon;                      // +0x02A4  (DWARF h:167, BrnGui::ERoadIcon)
        TextField     mNames[E_ROW_COUNT];         // +0x02A8  (DWARF h:169)
        TextField     mScores[E_ROW_COUNT];        // +0x0748  (DWARF h:170)
        TextField     mTargetCaption;              // +0x0BE8  (DWARF h:171)
        AnimationComponent mBestScoreBackingAnimation;  // +0x0D10 (DWARF h:172)
        s32           meCurrentRule;               // +0x0D9C  (DWARF h:174)
        s32           meCurrentScoreMode;          // +0x0DA0  (DWARF h:175)
        bool          mbActive;                    // +0x0DA4  (DWARF h:177)
    };
}
