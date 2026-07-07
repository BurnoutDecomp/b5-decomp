#pragma once

// ===================================================================================
// BrnGui::CrashNavPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h
//
// The "crash nav" map panel. It multiplexes between two display sub-modes -- showing the
// list of available EVENTS and showing the ROAD RULES scoreboard -- and exposes typed
// accessors for the currently-active event/road-rule selection. Each accessor asserts the
// panel is in the matching sub-mode before returning.
//
// No DecFIGS DWARF and no Feb-2007 source exist for this TU. The recovered member shape
// comes from the X360 accesses in the three accessors (@0x824BAE58 / @0x824185C8 /
// @0x82418668):
//   - the display sub-mode at object +0x90 (lwz; asserted == 0 "showing events" or
//     == 2 "showing road rules").
//   - an embedded BrnGui::EventPanel sub-object at object +0x2EA8 (the event-info panel;
//     GetPanelActiveGameModeType converts its meCurrentGameMode @sub+0x8AC through
//     EventPanel::ConvertLocalEventDefToProgressionEventDef).
//   - the active road-rule type at object +0x481C (lwz).
//   - the active road-rule scoring mode at object +0x4820 (lwz).
// The unrecovered head/gaps are reserved byte-spans so each named field lands at its binary
// offset without raw-offset casting; all access is by name. (assert site file string:
// BrnCrashNavPanel.h.)
//
// PARTIAL LAYOUT: this class is much larger than the slice modeled here (its constructor
// installs ~33 embedded sub-panel/toggle vtables out to object +0x5194); only the members
// the three accessors touch are recovered. Do not embed CrashNavPanel by value against this
// header -- the tail is not modeled.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h"   // embedded EventPanel + EModeType

namespace BrnGui
{
    class CrashNavPanel
    {
    public:
        // Which selection list the panel is currently showing (object +0x90). The two values
        // the accessors assert against; value 1 is not exercised by this TU and is left
        // unnamed.
        enum EShowingMode
        {
            E_SHOWING_MODE_EVENTS     = 0,   // GetPanelActiveGameModeType requires this
            E_SHOWING_MODE_ROAD_RULES = 2,   // the road-rule accessors require this
        };

        // @ 0x824BAE58 - active game/progression mode for the highlighted event. Asserts the
        // panel is showing events, then maps the embedded event panel's current game mode
        // through EventPanel::ConvertLocalEventDefToProgressionEventDef. Non-const because that
        // collaborator is a non-const method.
        BrnProgression::RaceEventData::EModeType GetPanelActiveGameModeType();

        // @ 0x824185C8 - active road-rule type. Asserts the panel is showing road rules.
        s32 GetPanelActiveRoadRuleType() const;

        // @ 0x82418668 - active road-rule scoring mode. Asserts the panel is showing road rules.
        s32 GetRoadPanelScoreMode() const;

    private:
        // Reserved storage for the unrecovered panel head that precedes the sub-mode field
        // (object +0x90). Layout-recovery padding only.
        u8           maHeadReserved[0x90];

        EShowingMode meShowingMode;   // +0x90  (lwz; asserted EVENTS/ROAD_RULES)

        // Reserved span between the sub-mode field and the embedded event panel (object +0x2EA8).
        u8           maReservedToEventPanel[0x2EA8 - (0x90 + sizeof(EShowingMode))];

        EventPanel   mEventPanel;     // +0x2EA8  (event-info sub-panel)

        // Reserved span between the event panel and the road-rule fields (object +0x481C).
        u8           maReservedToRoadRule[0x481C - (0x2EA8 + sizeof(EventPanel))];

        s32          miActiveRoadRuleType;        // +0x481C  (lwz)
        s32          miActiveRoadRuleScoreMode;   // +0x4820  (lwz)
    };
}
