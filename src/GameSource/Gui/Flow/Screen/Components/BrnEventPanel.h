#pragma once

// ===================================================================================
// BrnGui::EventPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h
//
// The event-info HUD panel: a BrnGui::IconComponent that owns six text fields, a mode-logo
// icon and a car icon, and repaints them for whichever event the crash-nav map has
// highlighted. It also caches the player's per-mode ranks and rank-win counts, which the
// road-rage / stunt-attack arms interpolate the displayed TARGET score from.
//
// Member NAMES come from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h).
//
// ⭐⭐ HEAD CARVE RETIRED 2026-08-29 (main-menu wave G1). The previous revision reserved
// everything before meCurrentGameMode as one opaque `u8 maHeadReserved[0x8AC]`, because no
// bodied function in scope touched it. EventPanel::Construct @0x8243A2C0 pins the whole run:
//   +0x0000  IconComponent base   (`IconComponent::Construct(this, name, si, 0, parent)`
//                                   @0x8243A2DC -- no state-identifier table)
//   +0x0094  mTextfields[6]       (0x128 stride; the loop @0x8243A2F4..0x8243A324 walks
//                                   `addi r28,r28,0x128` from this+0x94 while the name pointer
//                                   walks off_82F251B0 -> off_82F251C8, i.e. six entries)
//   +0x0784  mModeLogo            (IconComponent "modeLogo_cpt", state table off_82F25198)
//   +0x0818  mCarIcon             (IconComponent "carIcon_cpt", no state table)
//   +0x08AC  meCurrentGameMode    (`stw 6, 0x8AC` == E_EVENT_TYPE_COUNT)
//   +0x08B0  muCurrentEventID     (`stw -1, 0x8B0`)
//   +0x08B4  miPlayerRank .. +0x08D4 miMarkedManRankWins  (nine `stw -1` stores)
//   +0x08D8  mbActive             (`stb 0, 0x8D8`, after the setup-done post)
// (The host layout is name-based and every embedded pointer widens; the guest offsets above
// are the proof that each member is where the DWARF puts it, not the access mechanism.)
// ===================================================================================

#include "types.hpp"
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // RaceEventData (+ EModeType)
#include "GameSource/Gui/BrnGuiTextField.h"                            // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"             // BrnGui::IconComponent (base + 2 by value)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiFlow
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"    // CgsGui::StateInterface

namespace BrnGui
{
    class GuiCache;
    struct WorldDataController;

    class EventPanel : public IconComponent
    {
        // CrashNavPanel embeds an EventPanel and reads meCurrentGameMode @+0x8AC through the
        // private ConvertLocalEventDefToProgressionEventDef (X360 CrashNavPanel accessor
        // @0x824BAE58). Friend rather than widening either to public (neither is X360-attested
        // as a public entry point).
        friend class CrashNavPanel;

    public:
        // Local event-filter classification (DWARF BrnEventPanel.h:53).
        enum EEventType
        {
            E_EVENT_TYPE_RACE          = 0,
            E_EVENT_TYPE_ROAD_RAGE     = 1,
            E_EVENT_TYPE_STUNT_ATTACK  = 2,
            E_EVENT_TYPE_SURVIVOR      = 3,
            E_EVENT_TYPE_BURNING_ROUTE = 4,
            E_EVENT_TYPE_ALL           = 5,
            E_EVENT_TYPE_COUNT         = 6,
        };

        // The six text fields the panel drives, in construction order (apt clip names come from
        // the file-static table in the .cpp; the console indexes them positionally).
        enum ETextField
        {
            E_TEXTFIELD_EVENT_NAME       = 0,   // "EventName"
            E_TEXTFIELD_EVENT_START      = 1,   // "EventStart"
            E_TEXTFIELD_GOAL_TITLE       = 2,   // "GoalTitle"
            E_TEXTFIELD_GOAL_TEXT        = 3,   // "GoalText"
            E_TEXTFIELD_ADDITIONAL_TITLE = 4,   // "AdditionalTitle"
            E_TEXTFIELD_ADDITIONAL_BODY  = 5,   // "AdditionalBody"
            E_TEXTFIELD_COUNT            = 6,
        };

        // DWARF BrnEventPanel.cpp:26 -- the six event second-level filter option labels.
        // PUBLIC static: the DWARF lists it ahead of the class's first `private:`, and
        // CrashNavPanel passes the table straight to MenuToggleGroupVarSize<3>::SetupToggle for
        // the E_PANEL_EVENT filter row (`addi r8, r11, off_82F25180` -> "$GAMEMODE_RACE"
        // @0x824405B0 in SetupComponent and @0x8243AD54 in RefreshSecondLevelFilter; six
        // options, `li r5, 6`). The DEFINITION belongs to the EventPanel TU.
        static const char* KAPC_EVENT_FILTER_OPTIONS[E_EVENT_TYPE_COUNT];

        // @0x8243A2C0 (DWARF cpp:79) -- IconComponent virtual at vtable slot 0.
        // CrashNavPanel::Construct reaches it through the slot (`(**(this + 11944))
        // (this + 11944, "eventPanel_mc", lpStateInterface, this + 4)` @0x82425F24).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82417B40 (DWARF cpp:131).
        void AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache);

        // @0x82430D70. DWARF cpp:159 declares (uint32_t, const GuiCache*, bool). The X360 build
        // inserts a FOURTH argument in the middle -- the challenged-event score record
        // CrashNavMap::Update builds on its stack and CrashNavPanel::SetEventPanelData forwards
        // (`mr r5, r29` @0x8243A8D4, mpGuiCache in r6, the flag in r7).
        // FLAG: typed `const void*` here rather than CrashNavPanel::ChallengedEventScore -- that
        // record is nested in CrashNavPanel, which includes THIS header, so naming it would be a
        // circular include. The record's home stays BrnCrashNavPanel.h. Only its LEADING s32 is
        // read here (`lwz r11, 0(r26)` on the stunt-attack and burning-route arms).
        void SetEventData(u32 luEventId, const void* lpChallengedScores,
                          const GuiCache* lpGuiCache, bool lbShowPanel);

        // @0x82417BE0 (DWARF cpp:545) -- the second-level event filter selection.
        void SetCurrentGameMode(EEventType leGameMode);

        // @0x82417D38 / @0x82417E98 (DWARF cpp:602 / :648) -- panel show/hide animations
        // (CrashNavPanel::ChangeVisiblePanelState @0x8243A5A0 / @0x8243A6D8).
        void TransitionIn();
        void TransitionOut();

        // DWARF h:289 -- the four per-mode WIN counts (+0x8C8..+0x8D4). CrashNavPanel::RecEvent
        // inlines it for the rank-progress response (`v12[562..565] = a2[4..7]`
        // @0x82442070..0x8244208C).
        void SetModeRankWins(s32 liRaceWins, s32 liRoadRageWins,
                             s32 liStuntAttackWins, s32 liMarkedManWins);

        // @0x82417A10 - store the player's overall rank (must be >= 0). Writes +0x8B4.
        void SetPlayerRank(s32 iRank);

        // @0x82417A70 - store the four per-mode ranks (each must be >= 0). Writes +0x8B8..+0x8C4.
        void SetModeRanks(s32 iRaceRank, s32 iRoadRageRank,
                          s32 iStuntAttackRank, s32 iMarkedManRank);

    private:
        // @0x824B3600 - map a local EEventType to the progression race-event mode.
        // Identity for the five concrete modes; E_EVENT_TYPE_ALL / unknown -> E_MODE_COUNT.
        BrnProgression::RaceEventData::EModeType
        ConvertLocalEventDefToProgressionEventDef(EEventType eLocalType);

        // ADDITIVE GROW (main-menu wave G1). SetEventData's two "what target does the panel
        // show?" helpers, both DWARF members of this class and both bodied in this TU. They are
        // NOT in the measured link closure on their own -- SetEventData is their only caller --
        // but SetEventData cannot be reconstructed without them.
        //
        // @0x8242CCB8 (cpp:~505) -- the ROAD RAGE takedown target: this rank's takedown target
        // interpolated towards the next rank's by how far the player is through this rank's win
        // requirement, rounded half-up. At (or past) the last rank it is that rank's own target.
        s32 GetRoadRageTakedownScore(const WorldDataController* lpWorldDataController) const;

        // @0x8242C888 (cpp:~455) -- the STUNT ATTACK score target: the same interpolation, but
        // between the EVENT's own two neighbouring rank scores, and rounded to two significant
        // figures. Identical in shape to the committed sibling
        // BrnProgression::ProgressionManager::GetStuntRunScoreTarget @0x8237B7A8.
        s32 GetStuntRunScore(const WorldDataController* lpWorldDataController,
                             const BrnProgression::RaceEventData* lpEventData) const;

        // ---- DWARF member run (X360 offsets are documentation only; access is BY NAME) ----
        TextField     maTextfields[E_TEXTFIELD_COUNT];  // +0x0094
        IconComponent mModeLogo;                        // +0x0784
        IconComponent mCarIcon;                         // +0x0818

        EEventType meCurrentGameMode;  // +0x08AC  (converted by CrashNavPanel::GetPanelActiveGameModeType)
        u32        muCurrentEventID;   // +0x08B0  (0 == "no event"; Construct parks it at ~0u)

        s32 miPlayerRank;              // +0x08B4  (DWARF BrnEventPanel.h:176)
        s32 miCurrentRaceRank;         // +0x08B8  (h:178)
        s32 miCurrentRoadRageRank;     // +0x08BC  (h:179)
        s32 miCurrentStuntAttackRank;  // +0x08C0  (h:180)
        s32 miCurrentMarkedManRank;    // +0x08C4  (h:181)

        s32 miOfflineRaceRankWins;     // +0x08C8  (h:183)
        s32 miRoadRageRankWins;        // +0x08CC  (h:184)
        s32 miStuntAttackRankWins;     // +0x08D0  (h:185)
        s32 miMarkedManRankWins;       // +0x08D4  (h:186)
        bool mbActive;                 // +0x08D8  (h:189)
    };
}
