#ifndef BRN_DISTRICT_MARKER_H
#define BRN_DISTRICT_MARKER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "SharedClasses/World/BrnWorldRegion.h"                                     // BrnWorld::ECounty / EDistrict + E_*_COUNT
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"       // BrnFlaptComponent (base, non-polymorphic)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptIconComponent (embedded x4)
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                   // BrnFlapt::FileRef (Prepare arg)

// ============================================================================
// GameSource/Gui/Flow/hud/Components/BrnDistrictMarker.h
//
// BrnGui::DistrictMarkerComponent -- the on-track "you have entered <County>/<District>"
// marker HUD widget. It owns two apt "container" movie clips (county panel + district
// panel) plus two icon clips, and runs each panel through a small transition state
// machine as the player crosses into a new county or district.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; class shape / enum / member names+order
// from the DecFIGS DWARF (BrnDistrictMarker.h). The derived class is NOT polymorphic in
// its own right -- it derives from the non-vtable BrnFlaptComponent base (mpStateInterface
// @+0x00, mAptRef @+0x04), then embeds four FlaptIconComponent movies.
//
// NOTE (X360 store-for-store quirk, preserved): SetCounty writes the "1" pending flag to
// mbDistrictChangePending even though it operates on the county state (stb r10,0x65 in
// SetCounty @0x82412740); reproduced verbatim rather than "fixed".
// ============================================================================

namespace BrnGui
{
    // County / district icon-label tables (X360 off_82F2C90C / off_820A7600). Contents not
    // recovered in this slice; declared extern here, defined (placeholder) in the .cpp.
    extern const char* const KAPC_COUNTY_ICON_NAMES[BrnWorld::E_COUNTY_COUNT];
    extern const char* const KAPC_DISTRICT_ICON_NAMES[BrnWorld::E_DISTRICT_COUNT];

    class DistrictMarkerComponent : public BrnFlaptComponent
    {
    public:
        // DWARF :42 -- the icon-component alias every embedded movie uses.
        typedef FlaptIconComponent BaseIconComponent;

        // DWARF :148 -- per-panel transition state machine.
        enum EMarkerState
        {
            E_MARKERSTATE_LOADING       = 0,
            E_MARKERSTATE_LOADED        = 1,
            E_MARKERSTATE_SHOWING       = 2,
            E_MARKERSTATE_TRANSITIONING = 3,
            E_MARKERSTATE_INVISIBLE     = 4,
            E_MARKERSTATE_HIDING        = 5,
            E_MARKERSTATE_COUNT         = 6,
        };

        // --- ledger methods (declaration-only unless bodied in this TU) ---
        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface, const char* lacParentName); // @0x82412550
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);                                        // @0x82420FB8
        void Update();                                                    // ICF-folded EMPTY on X360 (bodied; see .cpp)
        void SetCounty(BrnWorld::ECounty leCountyID);                                                             // @0x82412658
        void SetDistrict(BrnWorld::EDistrict leDistrictID);                                                       // @0x82412858
        void ProcessCountyTransitionComplete(const char* lpcComponentName);                                       // @0x82412A20
        void ProcessDistrictTransitionComplete(const char* lpcComponentName);                                     // @0x82412AB0
        void OnLoad(const char* lpcComponentName);
        void Hide();
        void SetOnline(bool lbOnline);
        void TransIn();
        void TransOut();
        void SetHideCountyIcon(bool lbHide);                              // @0x824733B8 (bodied; re-homed from the retired View slice)

    private:
        // FBurnMainHudState::OnLeave @0x82480B88 pokes the two container movies inline
        // (`this+0x840 / this+0x854` vcall slot 3 == FlaptIconComponent::SetState
        // "transout") -- the X360 state reaches the embedded members directly, with no
        // accessor in the ledger. Friend-granted rather than fabricating one.
        friend struct FBurnMainHudState;
        // Static frame-trigger callbacks (registered in Prepare); forward to the matching
        // Process*TransitionComplete on the user-data'd instance.
        static void CountyTransitionCompleteCallback(void* lpUserData, u16 luArg);   // @0x82412B40
        static void DistrictTransitionCompleteCallback(void* lpUserData, u16 luArg); // @0x82412BA0

        // --- members (X360 layout order) ---
        BaseIconComponent   mCountyContainerMovie;     // +0x0C
        BaseIconComponent   mDistrictContainerMovie;   // +0x20
        BaseIconComponent   mCountyIcon;               // +0x34
        BaseIconComponent   mDistrictIcon;             // +0x48
        EMarkerState        meCurrentCountyState;      // +0x5C
        EMarkerState        meCurrentDistrictState;    // +0x60
        bool                mbCountyChangePending;     // +0x64
        bool                mbDistrictChangePending;   // +0x65
        bool                mbOnline;                  // +0x66
        BrnWorld::ECounty   meCurrentCounty;           // +0x68
        BrnWorld::ECounty   mePendingCounty;           // +0x6C
        BrnWorld::EDistrict meCurrentDistrict;         // +0x70
        BrnWorld::EDistrict mePendingDistrict;         // +0x74
    };
}

#endif // BRN_DISTRICT_MARKER_H
