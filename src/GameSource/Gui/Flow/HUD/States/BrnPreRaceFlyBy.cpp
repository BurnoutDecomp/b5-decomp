// ===================================================================================
// BrnGui::PreRaceFlyByState  -- the "PRE_FLY_BY" HUD flow state (pre-event fly-by camera)
//   class:BrnGui::PreRaceFlyByState
//
//   PreRaceFlyByState (ctor)         @ 0x82514E58
//   IsMapApplicableToGameMode        @ 0x824B3120
//   IsMapPanApplicableToGameMode     @ 0x824B3190
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.h"

#include <cstring>   // std::memset (zero the opaque component storage)

namespace BrnGui
{
    namespace GSM = BrnGameState::GameStateModuleIO;

    // @ 0x82514E58 -- construct the pre-race fly-by state. The X360 writes the object's
    // most-derived vtable at +0x000, then the embedded GUI-component vtables (mEventName,
    // mModeType, mEventDescriptionText[5] TextFields; mLargeEventIcon; mStateAnimator;
    // mMainMapComponent) at their fixed offsets, and tail-calls MapManager::MapManager on the
    // embedded map manager @ +0xA2C. The observable post-state is a default-constructed state
    // with its sub-components zero-initialised and the map manager fully constructed; the raw
    // vtable stores (rodata pointers that widen on the 64-bit host) are reproduced as the
    // zero-init of the opaque component spans + the real embedded MapManager ctor.
    PreRaceFlyByState::PreRaceFlyByState()
    {
        std::memset(maTextFields, 0, sizeof(maTextFields));
        std::memset(maLargeEventIcon, 0, sizeof(maLargeEventIcon));
        std::memset(maStateAnimator, 0, sizeof(maStateAnimator));
        std::memset(maMainMapComponent, 0, sizeof(maMainMapComponent));
        std::memset(maTailState, 0, sizeof(maTailState));
        // mMapManager (guest +0xA2C) is constructed by its own member ctor (X360 tail-call to
        // BrnGui::MapManager::MapManager @0x82508550).
    }

    // @ 0x824B3120 -- is the mini-map applicable to this game mode? The X360 switch returns 0 for
    // ShowTime(2), RoadRage(3), Pursuit(4), StuntAttack(7), TrafficAttack(9), FreeBurnLobby(15),
    // OnlineShowTime(16); 1 (applicable) for every other mode.
    bool PreRaceFlyByState::IsMapApplicableToGameMode(GSM::EGameModeType leGameMode)
    {
        switch (leGameMode)
        {
            case GSM::E_MODE_OFFLINE_SHOWTIME:      // 2
            case GSM::E_MODE_ROAD_RAGE:             // 3
            case GSM::E_MODE_PURSUIT:               // 4
            case GSM::E_MODE_STUNT_ATTACK:          // 7
            case GSM::E_MODE_TRAFFIC_ATTACK:        // 9
            case GSM::E_MODE_ONLINE_FREE_BURN_LOBBY: // 15
            case GSM::E_MODE_ONLINE_SHOWTIME:       // 16
                return false;
            default:
                return true;
        }
    }

    // @ 0x824B3190 -- does the map pan for this game mode? The X360 switch returns 1 only for
    // OfflineRace(0), BurningRoute(5), Eliminator(6), MarkedMan(8); 0 otherwise.
    bool PreRaceFlyByState::IsMapPanApplicableToGameMode(GSM::EGameModeType leGameMode)
    {
        switch (leGameMode)
        {
            case GSM::E_MODE_OFFLINE_RACE:   // 0
            case GSM::E_MODE_BURNING_ROUTE:  // 5
            case GSM::E_MODE_ELIMINATOR:     // 6
            case GSM::E_MODE_MARKED_MAN:     // 8
                return true;
            default:
                return false;
        }
    }
}
