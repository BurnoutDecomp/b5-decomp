#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"       // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple
#include "GameSource/GameState/BrnGameStateSharedIO.h"                // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameSource/Gui/SatNav/BrnMapManager.h"                      // BrnGui::MapManager (embedded @ guest +0xA2C)

// BrnGui::PreRaceFlyByState - the "PRE_FLY_BY" HUD flow state (one of the 14 states the HUD
// flow pool owns; built by BrnHudFlow::Prepare @0x8251A620, via the named ctor
// BrnGui::PreRaceFlyByState::PreRaceFlyByState @0x82514E58). Derives from CgsGui::State.
//
// Class shape + member names/order from the DecFIGS DWARF (BrnPreRaceFlyBy.h). The X360 ctor
// attests these guest byte offsets (base ptr is u8*, so asm store displacements are true byte
// offsets). The ctor writes the object's most-derived vtable at +0x000 then the embedded
// GUI-component vtables:
//   +0x038 mEventName            (BrnGui::TextField,            vtable off_82072F8C)
//   +0x160 mModeType             (BrnGui::TextField,            +0x128 stride)
//   +0x288 mEventDescriptionText[0..4] (5 x BrnGui::TextField,  +0x128 stride each:
//   +0x3B0                        0x288/0x3B0/0x4D8/0x600/0x728)
//   +0x4D8
//   +0x600
//   +0x728
//   +0x850 mLargeEventIcon       (BrnGui::IconComponent,        vtable off_82072F90)
//   +0x8E4 mStateAnimator        (BrnGui::AnimationComponent,   vtable off_82072F68)
//   +0x9A0 mMainMapComponent     (BrnGui::MainMapComponent,     vtable off_82076608)
//   +0xA2C mMapManager           (BrnGui::MapManager, embedded ctor @0x82508550; spans 0x578)
// Object size 0x1040 (4160; == BrnHudFlow::Prepare's PRE_FLY_BY pool slot).
//
// PHASE NOTE: the seven TextFields / IconComponent / AnimationComponent / MainMapComponent are
// carried as guest-sized opaque storage (their per-member layout is not sized in this phase, and
// host pointer widening 4->8 would desync the guest offsets). mMapManager is a real reconstructed
// sub-object, constructed by the ctor. The out-of-line virtual/state machinery (OnEnter/OnLeave/
// Update/GetResourcesToLoad + the map/icon/description helpers) lands with the fuller class TU.
// FLAG (consolidator): a sibling shell of this same class lives at
// GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h (carries the GetResourcesToLoad accessor +
// maResourcesToLoad statics); the two are the same BrnGui::PreRaceFlyByState and want unifying.
namespace BrnGui
{
    struct PreRaceFlyByState : public CgsGui::State
    {
        // DWARF BrnPreRaceFlyBy.h:84 -- the fly-by presentation phase.
        enum EPreRaceState
        {
            E_PRERACE_INVALID              = -1,
            E_PRERACE_UNLOADED             = 0,
            E_PRERACE_LOADING_COMPONENTS   = 1,
            E_PRERACE_ACTIVE_MAP_ICON_DELAY = 2,
            E_PRERACE_ACTIVE_EVENT_TITLES  = 3,
            E_PRERACE_ACTIVE_MAP_INTRO     = 4,
            E_PRERACE_ACTIVE_SHOW_MAP      = 5,
            E_PRERACE_ACTIVE_MEDALS        = 6,
            E_PRERACE_ACTIVE_TRANS_OUT     = 7,
            E_PRERACE_ACTIVE_DONE          = 8,
            E_PRERACE_COUNT                = 9,
        };

        PreRaceFlyByState();                                          // @ 0x82514E58

    private:
        // @ 0x824B3120 -- is the mini-map shown for this game mode? False for ShowTime(2),
        // RoadRage(3), Pursuit(4), StuntAttack(7), TrafficAttack(9), FreeBurnLobby(15),
        // OnlineShowTime(16); true otherwise.
        bool IsMapApplicableToGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);

        // @ 0x824B3190 -- does the map pan for this game mode? True only for OfflineRace(0),
        // BurningRoute(5), Eliminator(6), MarkedMan(8); false otherwise.
        bool IsMapPanApplicableToGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);

        // --- members (DWARF order; base CgsGui::State occupies guest +0x00..+0x38) ---

        // guest +0x038 .. +0x850 : mEventName + mModeType + mEventDescriptionText[5]
        // (7 x BrnGui::TextField, guest stride 0x128). Opaque this phase.
        u8  maTextFields[0x850 - 0x38];

        // guest +0x850 .. +0x8E4 : mLargeEventIcon (BrnGui::IconComponent). Opaque.
        u8  maLargeEventIcon[0x8E4 - 0x850];

        // guest +0x8E4 .. +0x9A0 : mStateAnimator (BrnGui::AnimationComponent). Opaque.
        u8  maStateAnimator[0x9A0 - 0x8E4];

        // guest +0x9A0 .. +0xA2C : mMainMapComponent (BrnGui::MainMapComponent). Opaque.
        u8  maMainMapComponent[0xA2C - 0x9A0];

        // guest +0xA2C : mMapManager (real reconstructed sub-object; spans 0x578 -> ends +0xFA4).
        MapManager mMapManager;

        // guest +0xFA4 .. +0x1040 : remaining state bookkeeping (meCurrentState / mfTimeRemaining /
        // mbEndRequestSent / mbDoMapPan / mfIconAnimationStartTime / mpGuiCache / mpIconManager /
        // mIconManagerOwnerId / mv2WorldCenterPoint / mbHiddenDueToPause / miPreviousIconCount).
        // Opaque this phase (object size 0x1040).
        u8  maTailState[0x1040 - 0xFA4];
    };
}
