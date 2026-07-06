#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"

#include "types.hpp"
#include <new>   // placement new (construct the owned MapManager sub-object at its byte offset)

// BrnGui::GuiNetworkRouteInfo::GuiNetworkRouteInfo -- constructor.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX @0x82511568.
//
// The X360 ctor is a flat sequence of vtable-slot stores (the embedded sub-objects'
// trivial inline ctors emit only their vtable install in this build) plus the one real
// out-of-line sub-object ctor (BrnGui::MapManager, embedded inside mMainMapComponent):
//
//   *(this+0x0000) = off_82076698                 ; GuiNetworkRouteInfo (component) vtable
//   *(this+0x008C + i*0x8C) = off_82072F68  (i=0..8) ; maOptionsAnimator[i] vtable
//   *(this+0x0578 + i*0x128)= off_82072F8C  (i=0..8) ; maHeading[i]  (TextField) vtable
//   *(this+0x0FE0 + i*0x128)= off_82072F8C  (i=0..8) ; maValue[i]    (TextField) vtable
//   *(this+0x1A50) = off_82076608                 ; mMainMapComponent vtable
//   BrnGui::MapManager::MapManager(this+0x1ADC)   ; MapManager embedded in mMainMapComponent
//   *(this+0x20E0) = off_82071824                 ; mEvent vtable
//   return this
//
// Because the surrounding sub-component classes are not yet modelled as named members, every
// touched location is addressed by its X360 byte offset through a char* view of `this` -- the
// same approach CgsGuiModule.cpp uses. Only the locations the ctor writes are reproduced.

namespace BrnGui
{
    // Real out-of-line MapManager ctor (body links from BrnMapManager.cpp). The per-TU
    // `cl /c` gate does not link, so the reference resolves at link time.
    class MapManager { public: MapManager(); };

    // Vtable symbols installed by the constructor (external data, defined elsewhere):
    //   gpNetworkRouteInfoVTable  -- the GuiNetworkRouteInfo (component) vtable (off_82076698)
    //   gpAnimationComponentVTable-- each maOptionsAnimator's AnimationComponent vtable (off_82072F68)
    //   gpTextFieldVTable         -- each heading/value TextField's vtable (off_82072F8C)
    //   gpMainMapComponentVTable  -- mMainMapComponent's vtable (off_82076608)
    //   gpEventVTable             -- mEvent's game-mode Event vtable (off_82071824)
    extern void* const gpNetworkRouteInfoVTable;
    extern void* const gpAnimationComponentVTable;
    extern void* const gpTextFieldVTable;
    extern void* const gpMainMapComponentVTable;
    extern void* const gpEventVTable;

    GuiNetworkRouteInfo::GuiNetworkRouteInfo()
    {
        char* const lp = reinterpret_cast<char*>(this);

        // --- CgsGui::GuiComponent base sub-object: install the component vtable @+0x00 ---
        *reinterpret_cast<void**>(lp + 0x00) = const_cast<void*>(gpNetworkRouteInfoVTable);

        // --- maOptionsAnimator[9]: each AnimationComponent installs its vtable (stride 0x8C) ---
        for (int liI = 0; liI < KU_OPTION_COMPONENTS; ++liI)
        {
            *reinterpret_cast<void**>(lp + KI_ANIMATOR_BASE + liI * KI_ANIMATOR_STRIDE) =
                const_cast<void*>(gpAnimationComponentVTable);
        }

        // --- maHeading[9] / maValue[9]: each TextField installs its vtable (stride 0x128) ---
        for (int liI = 0; liI < KU_OPTION_COMPONENTS; ++liI)
        {
            *reinterpret_cast<void**>(lp + KI_HEADING_BASE + liI * KI_TEXTFIELD_STRIDE) =
                const_cast<void*>(gpTextFieldVTable);
        }
        for (int liI = 0; liI < KU_OPTION_COMPONENTS; ++liI)
        {
            *reinterpret_cast<void**>(lp + KI_VALUE_BASE + liI * KI_TEXTFIELD_STRIDE) =
                const_cast<void*>(gpTextFieldVTable);
        }

        // --- mMainMapComponent: install its vtable, then construct the embedded MapManager ---
        *reinterpret_cast<void**>(lp + KI_MAIN_MAP_COMPONENT) =
            const_cast<void*>(gpMainMapComponentVTable);
        new (lp + KI_MAP_MANAGER) MapManager();

        // --- mEvent: install the game-mode event vtable ---
        *reinterpret_cast<void**>(lp + KI_EVENT) = const_cast<void*>(gpEventVTable);
    }
}
