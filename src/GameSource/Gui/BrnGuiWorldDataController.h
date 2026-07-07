#ifndef BRN_GUI_WORLD_DATA_CONTROLLER_H
#define BRN_GUI_WORLD_DATA_CONTROLLER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID (landmark/trigger lookups)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<T>
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"           // BrnWorld::GlobalColourPalette, PlayerCarColourPalette

// BrnGui::WorldDataController -- the GUI-side world/progression data front-end. The cache owns
// one; it fronts the loaded trigger / progression / vehicle / player-car-colour / street
// resources for the sat-nav renderer, license components and event panels.
//
// Layout + method SHAPE are taken from the DecFIGS DWARF for this exact path
// (references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiWorldDataController.{h,cpp}) and gated on
// the X360 ARTIST ledger. The member OFFSETS in the comments are the X360 32-bit-ABI byte offsets
// proven by the accessor asm (mpTriggerData@0x424, mpProgressionData@0x444, mpVehicleList@0x464,
// mpPlayerCarColours@0x468); they are NOT host-asserted because ResourcePtr<T> and the raw
// pointers widen to 64-bit on the host and mReceiverQueue is modelled as an opaque size-holding
// blob. Callers reach the members BY NAME, so exact host offsets are immaterial to compilation.
//
// This wave homes the readiness accessors GetColourPaletteFromType / GetProgressionData /
// GetTotalNumberOfLandmarks / GetTotalNumberOfOnlineLandmarks, and keeps the two the sat-nav
// renderer already links (GetEventInfoFromEventId / GetTotalNumberOfOnlineLandmarks). The
// Prepare acquire state machine + GetRequiredWinsInRank land in later waves.

namespace BrnProgression { struct RaceEventData; struct ProgressionData; }
namespace BrnTrigger     { struct TriggerData; struct Landmark; struct BoxRegion; }
namespace BrnResource    { struct VehicleList; }
namespace BrnStreetData  { struct StreetData; }
namespace BrnGameState   { class  LandmarkIndex; }
namespace BrnGui { struct ChallengeList; }

namespace BrnWorld
{
    // Palette selector for GetColourPaletteFromType (indexes GlobalColourPalette::maPalettes[4]).
    // Enumerators are DWARF-authoritative (PlayerCarColours.h:32); eNumPalettes == 4 and matches
    // BrnWorld::E_NUM_PALETTES in BrnGlobalColourPalette.h.
    enum EPalettesTypes
    {
        eGloss       = 0,
        eMetallic    = 1,
        ePearlescent = 2,
        eSpecial     = 3,
        eNumPalettes = 4,
    };
}

namespace BrnGui
{
    struct WorldDataController
    {
        // DWARF BrnGuiWorldDataController.h:157. State machine of the resource-acquisition flow.
        enum EWorldDataControllerState
        {
            E_WORLDDATACONTROLLERSTATE_DESTRUCTED                              = 0,
            E_WORLDDATACONTROLLERSTATE_CONSTRUCTED                             = 1,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_TRIGGERS                  = 2,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_TRIGGERS            = 3,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_VEHICLES                  = 4,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_VEHICLES            = 5,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_PROGRESSION               = 6,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_PROGRESSION         = 7,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_STREET_DATA               = 8,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_STREET_DATA         = 9,
            E_WORLDDATACONTROLLERSTATE_PLAYERCARCOLOURS                        = 10,
            E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS                      = 11,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_FREEBURN_CHALLENGES       = 12,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_FREEBURN_CHALLENGES = 13,
            E_WORLDDATACONTROLLERSTATE_PREPARED                                = 14,
            E_WORLDDATACONTROLLERSTATE_READY                                   = 15,
            E_WORLDDATACONTROLLERSTATE_COUNT                                   = 16,
        };

        // ---- Accessors (bodies in BrnGuiWorldDataController.cpp) --------------------------------

        // DWARF h:93 / X360 0x82501270 -- the landmark whose region index equals lLandmarkIndex
        // (linear scan of the trigger data's landmark table; asserts + fires a not-found message).
        const BrnTrigger::Landmark* GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex) const;

        // DWARF h:98 / X360 0x82501480 -- the landmark whose id equals lLandmarkID (linear scan).
        const BrnTrigger::Landmark* GetLandmarkInfoFromID(CgsID lLandmarkID) const;

        // DWARF h:103 / X360 0x82501698 -- the OFFLINE race-event record for an event id (sat-nav
        // renderer). Walks the progression event-junction table for a matching junction id.
        const BrnProgression::RaceEventData* GetEventInfoFromEventId(u32 luEventId) const;

        // DWARF h:108 / X360 0x82501740 -- the ONLINE race-event record for an event id.
        const BrnProgression::RaceEventData* GetOnlineEventInfoFromEventId(u32 luEventId) const;

        // DWARF h:112 / X360 0x8248E6D8 -- total landmark count (mpTriggerData->miLandmarkCount).
        s32 GetTotalNumberOfLandmarks() const;

        // DWARF h:116 / X360 0x824286E0 -- online landmark count (mpTriggerData->miOnlineLandmarkCount).
        s32 GetTotalNumberOfOnlineLandmarks() const;

        // DWARF h:122 / X360 0x82501AC0 -- copy the generic-region box for trigger lTriggerID into
        // *lpRegion (linear scan of the trigger data's generic-region table; fires on empty/miss).
        void GetTriggerVolumeRegion(CgsID lTriggerID, BrnTrigger::BoxRegion* lpRegion) const;

        // DWARF h:133 / X360 0x824F3AF0 -- the loaded vehicle-list resource (raw pointer member).
        const BrnResource::VehicleList* GetVehicleList() const;

        // DWARF h:137 / X360 0x824F3AF8 -- the loaded freeburn-challenge-list resource.
        const ChallengeList* GetFreeburnChallengeList() const;

        // DWARF h:147 / X360 0x824BDA40 -- the lType'th player-car colour palette entry.
        const BrnWorld::PlayerCarColourPalette* GetColourPaletteFromType(BrnWorld::EPalettesTypes lType) const;

        // DWARF h:151 / X360 0x82428818 -- the loaded progression resource (ResourcePtr operator->).
        const BrnProgression::ProgressionData* GetProgressionData() const;

    private:
        // Full DWARF-faithful member set (BrnGuiWorldDataController.h:182-193), in source order.
        // mReceiverQueue is an EventReceiverQueue<1024,16> whose full layout is not modelled; it is
        // held here as an opaque byte blob sized so the following resource pointers land at their
        // X360 offsets (mpTriggerData@0x424). X360 offsets are in the comments and NOT host-asserted.
        EWorldDataControllerState meState;               // X360 +0x000  (DWARF :182)
        u8                        mReceiverQueue[0x41C]; // X360 +0x004  EventReceiverQueue<1024,16> (opaque)
        s32                       miResourceCount;       // X360 +0x420  (DWARF :184)

        CgsResource::ResourcePtr<BrnTrigger::TriggerData>          mpTriggerData;      // X360 +0x424  (DWARF :186)
        CgsResource::ResourcePtr<BrnProgression::ProgressionData>  mpProgressionData;  // X360 +0x444  (DWARF :188)
        const BrnResource::VehicleList*                           mpVehicleList;      // X360 +0x464  (DWARF :189)
        CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>    mpPlayerCarColours; // X360 +0x468  (DWARF :190)
        CgsResource::ResourcePtr<BrnStreetData::StreetData>       mpStreetData;       // X360 +0x488  (DWARF :191)
        const ChallengeList*                                     mpChallengeList;    // DWARF :193
    };
}

#endif // BRN_GUI_WORLD_DATA_CONTROLLER_H
