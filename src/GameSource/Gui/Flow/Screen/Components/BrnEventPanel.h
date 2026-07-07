#pragma once

// ===================================================================================
// BrnGui::EventPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h
//
// The event-info HUD panel (a BrnGui::IconComponent subclass, DWARF-attested base) that
// shows the mode logo, car icon and per-mode rank text for the highlighted event. This
// batch bodies the small rank-mutator/type-map slice:
//   SetPlayerRank  (@0x82417A10)  -> stores the player's overall rank at object +0x8B4
//   SetModeRanks   (@0x82417A70)  -> stores the four per-mode ranks at +0x8B8..+0x8C4
//   ConvertLocalEventDefToProgressionEventDef (@0x824B3600) -> maps the panel's local
//                                    EEventType to a BrnProgression::RaceEventData::EModeType
//
// Member NAMES + offsets come from DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h).
// X360 stores pin the rank block: miPlayerRank @+0x8B4, miCurrentRaceRank.. @+0x8B8/8BC/8C0/8C4.
// The panel head ahead of miPlayerRank (base IconComponent + TextField/IconComponent sub-objects
// + meCurrentGameMode + muCurrentEventID) is not attested by this batch, so it is a reserved
// byte-span so each named field lands at its binary offset without raw-offset casting.
//
// COMPILE DEPENDENCY: the return type of ConvertLocalEventDefToProgressionEventDef is
// BrnProgression::RaceEventData::EModeType. The committed BrnRaceEventData.h currently has NO
// EModeType enum, so it MUST be grown with the DWARF-attested enum below before this header
// compiles (values from references/DecFIGS/dwarfdump/.../BrnRaceEventData.h):
//
//     namespace BrnProgression { struct RaceEventData {
//         enum EModeType {
//             E_MODE_INVALID       = -1,
//             E_MODE_RACE          = 0,
//             E_MODE_ROAD_RAGE     = 1,
//             E_MODE_STUNT_ATTACK  = 2,
//             E_MODE_SURVIVOR      = 3,
//             E_MODE_BURNING_ROUTE = 4,
//             E_MODE_PURSUIT       = 5,
//             E_MODE_COUNT         = 6,
//         };
//         ...existing minimal slice...
//     }; }
//
// (Add it to the SINGLE owner BrnRaceEventData.h -- do NOT fork a second definition.)
// ===================================================================================

#include "types.hpp"
#include "SharedClasses/Progression/BrnRaceEventData.h"   // must define RaceEventData::EModeType (see note above)

namespace BrnGui
{
    class EventPanel /* : public IconComponent (DWARF: struct EventPanel : public BrnGui::IconComponent) */
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

        // @ 0x82417A10 - store the player's overall rank (must be >= 0). Writes +0x8B4.
        void SetPlayerRank(s32 iRank);

        // @ 0x82417A70 - store the four per-mode ranks (each must be >= 0). Writes +0x8B8..+0x8C4.
        void SetModeRanks(s32 iRaceRank, s32 iRoadRageRank,
                          s32 iStuntAttackRank, s32 iMarkedManRank);

    private:
        // @ 0x824B3600 - map a local EEventType to the progression race-event mode.
        // Identity for the five concrete modes; E_EVENT_TYPE_ALL / unknown -> E_MODE_COUNT.
        BrnProgression::RaceEventData::EModeType
        ConvertLocalEventDefToProgressionEventDef(EEventType eLocalType);

        // Reserved storage for the unrecovered panel head that precedes the game-mode field
        // (base IconComponent + mTextfields[6] + mModeLogo + mCarIcon). Layout-recovery
        // padding only; meCurrentGameMode must land at +0x8AC.
        u8         maHeadReserved[0x8AC];

        EEventType meCurrentGameMode;  // +0x8AC  (converted by CrashNavPanel::GetPanelActiveGameModeType)
        u32        muCurrentEventID;   // +0x8B0

        s32 miPlayerRank;              // +0x8B4  (DWARF BrnEventPanel.h:176)
        s32 miCurrentRaceRank;         // +0x8B8  (h:178)
        s32 miCurrentRoadRageRank;     // +0x8BC  (h:179)
        s32 miCurrentStuntAttackRank;  // +0x8C0  (h:180)
        s32 miCurrentMarkedManRank;    // +0x8C4  (h:181)
    };
}
