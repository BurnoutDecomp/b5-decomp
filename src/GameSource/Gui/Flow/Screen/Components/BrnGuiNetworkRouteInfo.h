#pragma once

// ===================================================================================
// BrnGui::GuiNetworkRouteInfo  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h
//   class:BrnGui::GuiNetworkRouteInfo
//
// The online "network route info" GUI component: the map + options panel shown while a
// networked game/route is being set up. It embeds a row of nine option groups (an apt
// animator + a heading + a value TextField each), a map icon manager and the main map
// component (which itself owns a sat-nav MapManager), the crash-nav borough helper, and a
// game-mode event record. It is composed BY VALUE inside the online screen states
// (OnlineGameOptions / OnlineGameOptionsSummary / OnlineLoading /
// OnlineGameRoomPlayerInfo), which is why those states' ctors call this ctor.
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h
//   (X360-attested). GuiNetworkRouteInfo : public CgsGui::GuiComponent, so it carries the
//   base vptr @+0x00, macName[128] @+0x04, muHashedName @+0x84 and mpStateInterface @+0x88
//   (base size 0x8C) before its own members.
//
// LAYOUT proven store-for-store from BURNOUT_X360_ARTIST.XEX ctor @0x82511568
// (guest 32-bit-pointer ABI byte offsets):
//   +0x0000  CgsGui::GuiComponent base            vptr off_82076698 installed @+0x00
//   +0x008C  maOptionsAnimator[9]  (AnimationComponent, stride 0x8C)  vptr off_82072F68
//   +0x0578  maHeading[9]          (BrnGui::TextField, stride 0x128)  vptr off_82072F8C
//   +0x0FE0  maValue[9]            (BrnGui::TextField, stride 0x128)  vptr off_82072F8C
//   +0x1A48  mpIconManager         (MapIconManager*)
//   +0x1A4C  mIconManagerOwnerId   (MapIconManager::OwnerId)
//   +0x1A50  mMainMapComponent     (MainMapComponent)  vptr off_82076608 @+0x1A50;
//                                    embeds a BrnGui::MapManager @+0x8C (abs +0x1ADC)
//   ...      mv2WorldCenterPoint / mCrashNavBorough (not written by the ctor)
//   +0x20E0  mEvent  (BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event)
//                                    vptr off_82071824 @+0x20E0
//   ...      mpGuiCache / miNumComponentsLoaded / meSelectedCounty / meState /
//            mbReceivedInfo / mbShowingMap  (not written by the ctor)
//
// The embedded sub-component classes (AnimationComponent / MainMapComponent /
// CrashNavBorough / the game-mode Event) are not yet modelled as named C++ members, so --
// exactly as CgsGuiModule.cpp does -- the aggregate is backed by an explicit byte-storage
// member and every location the ctor writes is addressed by its X360 byte offset. Only the
// locations the constructor actually writes are reproduced (the sub-objects' trivial inline
// ctors in this build emit only their vtable stores; MapManager has a real out-of-line ctor).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)

namespace BrnGui
{
    // MapManager's real out-of-line ctor is called on the sub-object embedded inside
    // mMainMapComponent (declaration-only here; body links from BrnMapManager.cpp).
    class MapManager;

    // GuiNetworkRouteInfo : public CgsGui::GuiComponent (DWARF BrnGuiNetworkRouteInfo.h:56).
    // X360 `this` spans through +0x20E0 (the last ctor write is the mEvent vtable slot); the
    // trailing scalar members (mpGuiCache .. mbShowingMap) push the true sizeof a little past
    // that. The storage is sized to comfortably cover every ctor-touched location.
    class GuiNetworkRouteInfo : public CgsGui::GuiComponent
    {
    public:
        // BrnGuiNetworkRouteInfo.h:59
        enum EState
        {
            E_STATE_VISIBLE   = 0,
            E_STATE_INVISIBLE = 1,
            E_STATE_COUNT     = 2,
        };

        // BrnGuiNetworkRouteInfo.h:116
        enum EOptionComponent
        {
            E_OPTION_COMPONENT_GAMEMODE         = 0,
            E_OPTION_COMPONENT_TRAFFIC          = 1,
            E_OPTION_COMPONENT_VEHICLE_CLASS    = 2,
            E_OPTION_COMPONENT_INFINITE_BOOST   = 3,
            E_OPTION_COMPONENT_NUM_ROUNDS       = 4,
            E_OPTION_COMPONENT_TRAFFIC_CHECKING = 5,
            E_OPTION_COMPONENT_BOOST_TYPE       = 6,
            E_OPTION_COMPONENT_CRASH_LIMIT      = 7,
            E_OPTION_COMPONENT_TIME_LIMIT       = 8,
            E_OPTION_COMPONENT_COUNT            = 9,
        };

        // @0x82511568 -- default constructor. Installs the component vtable, brings up the
        // nine option-group animators + heading/value text fields and the main map component
        // (running the embedded MapManager ctor), then installs the game-mode event vtable.
        GuiNetworkRouteInfo();

    private:
        // ---- ctor-touched sub-object offsets (X360 byte offsets, from ctor @0x82511568) ----
        static const int KI_ANIMATOR_BASE      = 0x008C;  // maOptionsAnimator[0]
        static const int KI_ANIMATOR_STRIDE    = 0x008C;  // AnimationComponent stride
        static const int KI_HEADING_BASE       = 0x0578;  // maHeading[0]
        static const int KI_VALUE_BASE         = 0x0FE0;  // maValue[0]
        static const int KI_TEXTFIELD_STRIDE   = 0x0128;  // BrnGui::TextField stride
        static const int KU_OPTION_COMPONENTS  = 9;
        static const int KI_MAIN_MAP_COMPONENT = 0x1A50;  // mMainMapComponent vtable slot
        static const int KI_MAP_MANAGER        = 0x1ADC;  // MapManager sub-object (MainMap +0x8C)
        static const int KI_EVENT              = 0x20E0;  // mEvent vtable slot

        // Backing storage for the (not-yet-named) embedded sub-objects. Sized to cover every
        // ctor-touched location; access is by X360 byte offset through a char* view of `this`.
        // The base sub-object occupies 0x00..0x8B; own members begin at KI_ANIMATOR_BASE.
        u8 maStorage[0x20E4 - 0x8C];  // 0x8C .. 0x20E3
    };
}
