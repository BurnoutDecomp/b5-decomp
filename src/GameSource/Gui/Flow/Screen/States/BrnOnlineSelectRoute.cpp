#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"

#include "types.hpp"
#include <new>   // placement new (the real out-of-line sub-object ctors, at their byte offsets)

// BrnGui::OnlineSelectRoute::OnlineSelectRoute -- constructor.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX @0x8251AE30.
//
// The X360 ctor is a flat sequence of vtable-slot stores (the embedded sub-objects' trivial
// inline ctors emit only their vtable install in this build) interleaved with the real
// out-of-line sub-object ctors (two MenuComponents, a HelpBar and the sat-nav MapManager
// embedded inside the main map component). Offsets are the X360 byte offsets from the ASM
// (r31 = this, r29 = this+0x38):
//
//   *(this+0x0000) = off_82077AAC                 ; OnlineSelectRoute (State) vtable
//   *(this+0x0038) = off_82074810                 ; secondary base/mixin vtable
//   BrnGui::MenuComponent::MenuComponent(this+0x0768)
//   *(this+0x1828) = off_82072F68  (stride 0x8C, x2) ; option animator vtables
//   *(this+0x18B4) = off_82072F68
//   BrnGui::MenuComponent::MenuComponent(this+0x1978)
//   *(this+0x2A38 + i*0x128) = off_82072F8C  (i=0..4) ; heading/value TextField vtables
//   *(this+0x3000 + i*0x8C)  = off_82072F68  (i=0..5) ; option animator vtables
//   BrnGui::HelpBar::HelpBar(this+0x3350)
//   *(this+0x51F0) = off_82076608                 ; mMainMapComponent vtable
//   BrnGui::MapManager::MapManager(this+0x527C)   ; MapManager embedded in the main map component (+0x8C)
//   *(this+0x5880) = off_82071824                 ; game-mode event vtable
//   *(this+0x5910) = off_82071828                 ; game-mode event secondary vtable
//   return this
//
// Because the surrounding sub-component classes are not modelled as named members (no DWARF, no
// Feb-2007 source for this TU), every touched location is addressed by its X360 byte offset
// through a char* view of `this` -- the same approach the committed BrnGui::GuiNetworkRouteInfo
// sibling and CgsGuiModule.cpp use. Only the locations the ctor writes are reproduced.

namespace BrnGui
{
    // Real out-of-line sub-object ctors (bodies link from their own TUs -- BrnMenuComponent.cpp,
    // BrnHelpBar.cpp, BrnMapManager.cpp). Declared minimally here (the concrete sub-component
    // classes are not included, to avoid pulling their full layout into this un-DWARF'd TU); the
    // per-TU `cl /c` gate does not link, so the references resolve at link time. Same precedent as
    // BrnGuiNetworkRouteInfo.cpp's local MapManager declaration.
    class MenuComponent { public: MenuComponent(); };
    class HelpBar       { public: HelpBar(); };
    class MapManager    { public: MapManager(); };

    // Vtable symbols installed by the constructor (external data, defined elsewhere). The
    // animator / TextField / main-map / game-mode-event symbols are the same ones the committed
    // BrnGuiNetworkRouteInfo ctor installs.
    //   gpOnlineSelectRouteVTable    -- the screen state's own vtable        (off_82077AAC)
    //   gpStateSecondaryVTable       -- the +0x38 secondary base/mixin vtable (off_82074810)
    //   gpAnimationComponentVTable   -- each option animator's vtable        (off_82072F68)
    //   gpTextFieldVTable            -- each heading/value TextField vtable   (off_82072F8C)
    //   gpMainMapComponentVTable     -- the main map component's vtable       (off_82076608)
    //   gpEventVTable                -- the game-mode event's vtable          (off_82071824)
    //   gpEventSecondaryVTable       -- the game-mode event's secondary vtable(off_82071828)
    extern void* const gpOnlineSelectRouteVTable;
    extern void* const gpStateSecondaryVTable;
    extern void* const gpAnimationComponentVTable;
    extern void* const gpTextFieldVTable;
    extern void* const gpMainMapComponentVTable;
    extern void* const gpEventVTable;
    extern void* const gpEventSecondaryVTable;

    namespace
    {
        // ctor-touched X360 byte offsets (from the ASM @0x8251AE30).
        const int KI_STATE_VTABLE      = 0x0000;   // off_82077AAC
        const int KI_STATE_SECONDARY   = 0x0038;   // off_82074810
        const int KI_MENU_COMPONENT_0  = 0x0768;   // MenuComponent ctor
        const int KI_ANIMATOR_A_BASE   = 0x1828;   // off_82072F68 x2, stride 0x8C
        const int KI_MENU_COMPONENT_1  = 0x1978;   // MenuComponent ctor
        const int KI_TEXTFIELD_BASE    = 0x2A38;   // off_82072F8C x5, stride 0x128
        const int KI_ANIMATOR_B_BASE   = 0x3000;   // off_82072F68 x6, stride 0x8C
        const int KI_HELP_BAR          = 0x3350;   // HelpBar ctor
        const int KI_MAIN_MAP          = 0x51F0;   // off_82076608
        const int KI_MAP_MANAGER       = 0x527C;   // MapManager ctor (main map component +0x8C)
        const int KI_EVENT             = 0x5880;   // off_82071824
        const int KI_EVENT_SECONDARY   = 0x5910;   // off_82071828

        const int KI_ANIMATOR_STRIDE   = 0x008C;
        const int KI_TEXTFIELD_STRIDE  = 0x0128;
        const int KU_ANIMATORS_A       = 2;
        const int KU_TEXTFIELDS        = 5;
        const int KU_ANIMATORS_B       = 6;
    }

    OnlineSelectRoute::OnlineSelectRoute()
    {
        char* const lp = reinterpret_cast<char*>(this);

        // --- screen state's own vtable (+0x00) + the secondary base/mixin vtable (+0x38) ---
        *reinterpret_cast<void**>(lp + KI_STATE_VTABLE)    = const_cast<void*>(gpOnlineSelectRouteVTable);
        *reinterpret_cast<void**>(lp + KI_STATE_SECONDARY) = const_cast<void*>(gpStateSecondaryVTable);

        // --- first embedded menu component ---
        new (lp + KI_MENU_COMPONENT_0) MenuComponent();

        // --- the two option animators sitting between the two menu components (stride 0x8C) ---
        for (int liI = 0; liI < KU_ANIMATORS_A; ++liI)
        {
            *reinterpret_cast<void**>(lp + KI_ANIMATOR_A_BASE + liI * KI_ANIMATOR_STRIDE) =
                const_cast<void*>(gpAnimationComponentVTable);
        }

        // --- second embedded menu component ---
        new (lp + KI_MENU_COMPONENT_1) MenuComponent();

        // --- the heading/value text fields (stride 0x128) ---
        for (int liI = 0; liI < KU_TEXTFIELDS; ++liI)
        {
            *reinterpret_cast<void**>(lp + KI_TEXTFIELD_BASE + liI * KI_TEXTFIELD_STRIDE) =
                const_cast<void*>(gpTextFieldVTable);
        }

        // --- the option animators after the text fields (stride 0x8C) ---
        for (int liI = 0; liI < KU_ANIMATORS_B; ++liI)
        {
            *reinterpret_cast<void**>(lp + KI_ANIMATOR_B_BASE + liI * KI_ANIMATOR_STRIDE) =
                const_cast<void*>(gpAnimationComponentVTable);
        }

        // --- help bar ---
        new (lp + KI_HELP_BAR) HelpBar();

        // --- main map component: install its vtable, then construct the embedded MapManager ---
        *reinterpret_cast<void**>(lp + KI_MAIN_MAP) = const_cast<void*>(gpMainMapComponentVTable);
        new (lp + KI_MAP_MANAGER) MapManager();

        // --- game-mode event record: primary + secondary vtables ---
        *reinterpret_cast<void**>(lp + KI_EVENT)           = const_cast<void*>(gpEventVTable);
        *reinterpret_cast<void**>(lp + KI_EVENT_SECONDARY) = const_cast<void*>(gpEventSecondaryVTable);
    }
}
