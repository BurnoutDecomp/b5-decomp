#pragma once

// ===================================================================================
// BrnGui::RoadPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h
//
// The Road Rules HUD panel. GetSelectedFriendName (@0x82417FF8) returns the name string
// of the currently-selected friend row (online road-rule scoreboard).
//
// DECFIGS DWARF DOES EXIST FOR THIS TU (an earlier revision of this banner claimed it did
// not): references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h.
// It supplies BrnGui::RoadPanelData (namespace scope, h:52) and its nested PanelBox (h:65)
// in full -- both are transcribed below. It also spells `struct BrnGui::RoadPanel : public
// BrnGui::IconComponent` with members mRoadSign / mRoadPanelData / ...; that base and that
// member order are NOT applied to the RoadPanel slice below, which is still the
// X360-measured GetSelectedFriendName slice described next and is left untouched here.
//
// No prior reconstruction carries this panel's full member layout. The recovered RoadPanel
// shape comes from the X360 access in GetSelectedFriendName:
//   - a bank of friend rows beginning at object +0x1A2 with a 162-byte (0xA2) stride; the
//     row's leading bytes are the friend-name string (the method returns &row[index]).
//   - the selected-row index at object +0xD9C (lwz, signed).
//   - the scoring mode at object +0xDA0 (lwz; asserted == 1, the ONLINE road-panel mode).
// The exact row count is not recoverable from this single accessor; the bank is modeled
// with the maximal count that keeps the index/mode fields at their binary offsets
// (18 rows: +0x1A2 .. +0xD05, leaving +0xD06..+0xD9B reserved). The unrecovered head and
// the gap are reserved byte-spans so each named field lands at its binary offset without
// raw-offset casting; all access is by name. (assert site: BrnRoadPanel.h:231.)
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    // ------------------------------------------------------------------------------
    // BrnGui::RoadPanelData -- the road-rule scoreboard payload a screen fills in and
    // hands to CrashNavPanel::SetRoadPanelData, which memcpy's it into the embedded road
    // panel. NAMESPACE SCOPE, not nested in RoadPanel (DWARF BrnRoadPanel.h:52); this is
    // the type BrnCrashNavPanel.h forward-declares. Member names, types and order are
    // DWARF-verbatim (h:55/:57/:65/:77-:85).
    //
    // The layout is X360-CORROBORATED, not merely transcribed: PanelBox's members sum to
    // 2 + 32 + 64 + 64 == 162 == 0xA2, which is exactly the stride RoadPanelData::Construct
    // walks (`addi r29, r29, 0xA2` @0x82425724, loop count `li r30, 2` @0x824256F4), and
    // 2 * 0xA2 == 0x144 is exactly the byte count CrashNavPanel::SetRoadPanelData memcpy's
    // (@0x8243AA08). The whole record is a pointer-free scalar run, so it is host-stable --
    // hence the static_asserts below are absolute, not relative.
    //
    // Both Construct bodies are real out-of-line X360 symbols homed in this class's own TU
    // (BrnRoadPanel.cpp): RoadPanelData::Construct @0x824256E0 (it just runs PanelBox::
    // Construct over both boxes with five NULL strings and two false flags) and
    // PanelBox::Construct @0x82418060 (it asserts
    // "lpYourScore && lpName1 && lpName2 && lp..." at BrnRoadPanel.cpp:99 and then copies
    // the five strings in). Declarations only here.
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
            // assert literal lists them.
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

    class RoadPanel
    {
    public:
        // Online road-panel scoring mode (asserted value, X360 cmpwi 1).
        // Mirrors GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE.
        static const s32 KI_ROAD_PANEL_MODE_ONLINE = 1;

        // Per-row stride of a friend entry (X360 mulli 0xA2). The friend-name string is at
        // the start of the row.
        static const s32 KI_FRIEND_ROW_STRIDE = 0xA2;   // 162 bytes

        // Maximal row count that keeps the index/mode fields at their X360 offsets.
        static const s32 KI_MAX_FRIEND_ROWS = 18;

        // @ 0x82417FF8 - return the name string of the currently-selected friend row. Asserts
        // the panel is in the online scoring mode (the selection bank is only valid online).
        const char* GetSelectedFriendName() const;

    private:
        // One friend scoreboard row. 162-byte stride; only the leading name string is
        // recovered by this TU, the rest of the row is reserved storage.
        struct FriendRow
        {
            char macName[KI_FRIEND_ROW_STRIDE];   // row +0x000 (162-byte stride; name at start)
        };

        // Reserved storage for the unrecovered panel head that precedes the row bank
        // (object +0x1A2). Layout-recovery padding only.
        u8        maHeadReserved[0x1A2];

        FriendRow maFriendRows[KI_MAX_FRIEND_ROWS];   // object +0x1A2 .. +0xD05

        // Reserved span between the row bank and the selected-index field (object +0xD9C).
        u8        maGapReserved[0xD9C - (0x1A2 + KI_MAX_FRIEND_ROWS * KI_FRIEND_ROW_STRIDE)];

        s32       miSelectedFriendIndex;   // +0xD9C  (lwz)
        s32       miScoringMode;           // +0xDA0  (lwz; asserted == ONLINE)
    };
}
