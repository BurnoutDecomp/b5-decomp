#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineSelectRoute - the online route-selection screen state. This leaf header carries
// the class shape and the one inline resource accessor attributed to the header. The route list,
// the map/preview machinery and all the out-of-line state and virtual machinery are reconstructed
// with the class:BrnGui::OnlineSelectRoute TU. Layout/virtuals and the CgsGui::State derivation
// are from the DecFIGS DWARF (BrnOnlineSelectRoute.h), where this TU names its statics
// maResourceTuplesToLoad / miNumResourcesToLoad (count is int32 in the DWARF -> cast to u32).
//
// The compiler-emitted constructor (@0x8251AE30) brings up a large fan of embedded GUI
// sub-objects (two BrnGui::MenuComponents, a row of option animators + heading/value text
// fields, a HelpBar, the main map component with its embedded sat-nav MapManager, and a
// game-mode event record). There is NO DecFIGS DWARF and no Feb-2007 source for this TU, so the
// embedded sub-component classes are not modelled as named C++ members -- exactly as the
// committed BrnGui::GuiNetworkRouteInfo sibling (and CgsGuiModule.cpp) do, the aggregate is
// backed by explicit byte-storage after the CgsGui::State base and every location the ctor
// writes is addressed by its X360 byte offset. Only the ctor-touched locations are reproduced.
namespace BrnGui
{
    struct OnlineSelectRoute : public CgsGui::State
    {
        // @ 0x8251AE30 - compiler-emitted ctor: installs the screen's own vtable (+0x000) and a
        // secondary vtable (+0x038), constructs the two embedded menu components, the option
        // animator/heading/value sub-widgets, the help bar, the main map component (running the
        // embedded MapManager ctor) and the game-mode event record. Reconstructed store-for-store
        // from the X360 asm; see BrnOnlineSelectRoute.cpp.
        OnlineSelectRoute();

        // @ 0x8251AEF8 - hands the route-select screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F338 (unk_8205F338, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F348 (dword_8205F348, .rdata) == 2

        // Highest X360 byte offset the ctor writes is the +0x5910 secondary-event vtable slot, so
        // the object must span at least +0x5918 (that slot + one guest/x64 pointer). Backing
        // storage laid down after the CgsGui::State base guarantees the object comfortably covers
        // every ctor-touched location; the ctor writes each location through a char* view of
        // `this` at its X360 byte offset (the trailing scalar state members past +0x5918 are not
        // touched by the ctor and are not modelled here). The AssertLayout below pins the size.
        static const s32 KI_OBJECT_SPAN = 0x5918;   // last ctor store (+0x5910) + one pointer
        u8 maStorage[KI_OBJECT_SPAN];

        // Never called: pins that the object is large enough to hold every X360 offset the ctor
        // writes, so an edit to the base or storage cannot silently shrink it below +0x5918.
        static void AssertLayout()
        {
            static_assert(sizeof(OnlineSelectRoute) >= KI_OBJECT_SPAN,
                          "OnlineSelectRoute must span every ctor-touched X360 offset (>= 0x5918)");
        }
    };
}
