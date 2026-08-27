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

        // ADDITIVE GROW ([stuntrace F2] wave, 2026-08-27). DWARF-declared
        // (BrnDistrictMarker.h:116 `void SetOnline(bool)`), but the X360 compiler INLINED
        // it at every call site -- there is no symbol for it in scratch/func_index.tsv --
        // so the body lands here rather than in the .cpp. Reconstructed from the one
        // attested call site, RaceMainHudState::UpdateWFInit @0x82480200:
        //     0x824805E4  lwz r11, 0x140(r31)     ; mpCache
        //     0x824805E8  lbz r11, 0x4B4C(r11)    ; GuiCache::mbOnlineStartInProgress
        //     0x824805EC  stb r11, 0x1062(r31)    ; == mDistrictMarker(+0xFFC) + 0x66
        // -- +0x66 is mbOnline. A bare named-member store; no assert on console.
        void SetOnline(bool lbOnline)
        {
            mbOnline = lbOnline;
        }

        void TransIn();
        void TransOut();
        void SetHideCountyIcon(bool lbHide);                              // @0x824733B8 (bodied; re-homed from the retired View slice)

    private:
        // FBurnMainHudState::OnLeave @0x82480B88 pokes the two container movies inline
        // (`this+0x840 / this+0x854` vcall slot 3 == FlaptIconComponent::SetState
        // "transout") -- the X360 state reaches the embedded members directly, with no
        // accessor in the ledger. Friend-granted rather than fabricating one.
        friend struct FBurnMainHudState;
        // RaceMainHudState::OnLeave @0x82479770 does the identical inline poke on the same
        // two container movies (`addi r30, r31, 0xFFC` == &mDistrictMarker, then
        // `lwz r11, 0xC(r30)` / `lwz r11, 0x20(r30)` -> vtable byte +0xC == slot 3 ==
        // FlaptIconComponent::SetState, with "transout") -- @0x824798F0..0x82479920. Same
        // reason as the sibling above: the X360 state reaches the embedded members
        // directly and the ledger carries no accessor, so friendship beats fabricating one.
        friend struct RaceMainHudState;
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
