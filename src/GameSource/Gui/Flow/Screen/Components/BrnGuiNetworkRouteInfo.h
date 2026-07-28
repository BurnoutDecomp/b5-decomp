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
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                       // BrnGui::GuiFlow (AppendExpectedAptComponent)

// The state in-queue the Update surface drains (pointer-only; same modelling note as
// CgsGuiState.h -- InputBuffer is the global event-queue NAMESPACE).
namespace InputBuffer { class GuiEventQueue; }

namespace BrnGui
{
    // MapManager's real out-of-line ctor is called on the sub-object embedded inside
    // mMainMapComponent (declaration-only here; body links from BrnMapManager.cpp).
    class MapManager;

    class  GuiCache;                    // tail member / AppendExpectedAptComponent arg (pointer only)
    struct GuiEventNetworkGameParams;   // SetInfo arg (pointer only; home GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h)

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

        // The apt component count the loaded gate compares against (DWARF
        // BrnGuiNetworkRouteInfo.h KI_NUM_COMPONENTS_TO_LOAD == 11; the X360 keeps it
        // in .data @0x8204C7A0 and the game-room screen loads it from there).
        static const s32 KI_NUM_COMPONENTS_TO_LOAD = 11;

        // @0x82511568 -- default constructor. Installs the component vtable, brings up the
        // nine option-group animators + heading/value text fields and the main map component
        // (running the embedded MapManager ctor), then installs the game-mode event vtable.
        GuiNetworkRouteInfo();

        // ---- surface the composing screen states drive (DWARF rows; bodies are their
        //      own ledger TUs -- declarations only, wave-H keystone) -------------------
        // DWARF h:118 Construct(const char*, GuiEventRenderMainMap::EMapType,
        // StateInterface*, const char*); the map-type enum's home header is not pulled
        // in -- underlying s32 per this header's boundary convention.
        void Construct(const char* lpacName, s32 leMapType,
                       CgsGui::StateInterface* lpStateInterface, const char* lpacSatNavIconBaseName);
        void Destruct();                                          // DWARF :62
        void SetState(EState leState);                            // DWARF :81
        // DWARF :115 SetInfo(int32_t, const GuiEventNetworkGameParams*): the round
        // index + the params payload to render (fwd-declared; home
        // GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h).
        void SetInfo(s32 liRoundIndex, const GuiEventNetworkGameParams* lpGameParams);
        void Update(InputBuffer::GuiEventQueue* lpInGuiEventQueue); // DWARF :112
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache); // DWARF :89

        // DWARF :109 -- the loaded gate the game-room screen inlines
        // (miNumComponentsLoaded == KI_NUM_COMPONENTS_TO_LOAD).
        bool IsComponentLoaded() const { return miNumComponentsLoaded == KI_NUM_COMPONENTS_TO_LOAD; }

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

        // Backing storage for the (not-yet-named) embedded sub-objects. Sized to cover the
        // ctor-touched span AND the mEvent record tail (X360 0x20E0..0x219B, 0xBC bytes --
        // the named tail above starts at +0x219C); access is by X360 byte offset through a
        // char* view of `this`. The base sub-object occupies 0x00..0x8B; own members begin
        // at KI_ANIMATOR_BASE. (Wave-H keystone: grown from 0x20E4 so the DWARF tail
        // members could land as NAMED members after it.)
        u8 maStorage[0x219C - 0x8C];  // 0x8C .. 0x219B

    public:
        // ---- named tail members (DWARF order mpGuiCache..mbShowingMap; X360 offsets
        //      +0x219C..+0x21AD, pinned by the game-room screen's raw accesses:
        //      miNumComponentsLoaded @+0x21A0 is stored/compared at abs +84304 of the
        //      OGRPI object whose mRouteInfoDisplay sits at +75696; X360
        //      sizeof(GuiNetworkRouteInfo) == 0x21B0/8624). PUBLIC: the X360 game-room
        //      screen writes miNumComponentsLoaded directly (no setter row in DWARF).
        GuiCache* mpGuiCache;              // +0x219C (8604)
        s32       miNumComponentsLoaded;   // +0x21A0 (8608) == KI_NUM_COMPONENTS_TO_LOAD once loaded
        s32       meSelectedCounty;        // +0x21A4 (8612) BrnWorld::ECounty (underlying s32)
        EState    meState;                 // +0x21A8 (8616)
        bool      mbReceivedInfo;          // +0x21AC (8620)
        bool      mbShowingMap;            // +0x21AD (8621)
    };
}
