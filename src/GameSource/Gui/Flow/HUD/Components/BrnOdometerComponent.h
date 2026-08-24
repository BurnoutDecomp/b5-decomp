#ifndef BRN_GUI_ODOMETER_COMPONENT_H
#define BRN_GUI_ODOMETER_COMPONENT_H

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnOdometerComponent.h
//
//   BrnGui::OdometerComponent -- the in-race "odometer" HUD readout that flips
//   between three transient messages: the running offline mileage, an
//   "events found (X of Y)" flash when a new event is discovered, and a
//   "drive-thrus found (X of Y)" flash when a new drive-thru is discovered.
//   Each flash animates in, holds for a few seconds, then returns to the
//   mileage readout. Derives from BrnFlaptComponent.
//
// Class shape, the nested EOdometerState enum, and member NAMES + ORDER are from
// the DecFIGS DWARF (BrnOdometerComponent.h), gated on the X360 ledger. Byte
// offsets are X360-attested by Construct @0x82415088 / Prepare @0x82423F88 /
// Update @0x82424160 / HandleJunctionChange @0x8242BEB0 / HandleDriveThruDiscovered
// @0x8242C000 / TransIn @0x82424388 / TransOutActiveText @0x82424468. The
// FlaptAnimatorComponent stride 0x38 and TextFieldRef stride 0x0C are attested by
// the committed sibling headers. OdometerComponent is NON-polymorphic, so the
// BrnFlaptComponent base subobject sits at +0x00:
//     +0x00  base BrnFlaptComponent (mpStateInterface @+0x00, mAptRef @+0x04)  0x0C
//     +0x0C  mMileageAnimator          (FlaptAnimatorComponent, 0x38)
//     +0x44  mMileageTextField         (TextFieldRef, 0x0C)
//     +0x50  mEventsFoundAnimator      (FlaptAnimatorComponent, 0x38)
//     +0x88  mEventsFoundTextField     (TextFieldRef, 0x0C)
//     +0x94  mDriveThrusFoundAnimator  (FlaptAnimatorComponent, 0x38)
//     +0xCC  mDriveThrusFoundTextField (TextFieldRef, 0x0C)
//     +0xD8  meOdometerState           (EOdometerState)
//     +0xDC  mfEnteredStateTime        (f32)
//     +0xE0  mpGuiCache                (BrnGui::GuiCache*)
//     +0xE4  mpProfile                 (const BrnProgression::Profile*)
//   sizeof(OdometerComponent) == 0xE8.
//
// All member access is BY NAME. The per-child apt component names
// ("MileageAnim_cpt", "MileageText_cpt"/"MileageText_Txt", ...) are the KAC_*
// name statics from the DWARF; the X360 folds them to inline string literals, so
// they are used directly at the call sites (not declared as statics) -- the same
// idiom as the committed sibling BrnJunctionInfoComponent.
// ============================================================================

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // BrnFlapt::TextFieldRef (by value)
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                  // BrnFlapt::FileRef (Prepare arg)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"       // BrnFlaptComponent (base) + MovieClipRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptAnimatorComponent

namespace CgsGui { struct StateInterface; }
namespace BrnGui { class GuiCache; }
namespace BrnProgression { class Profile; }   // class, matching BrnProfile.h:208 (a struct fwd-decl mis-mangles the GetProfile link)

namespace BrnGui
{
    // GuiEventJunctionInfo / GuiEventDriveThruDiscovered are the canonical GUI-event
    // payload types (homed in BrnGuiEventTypeDefs.h / BrnGuiDemangledEventTypes.h);
    // referenced by pointer only here, so a forward declaration suffices.
    struct GuiEventJunctionInfo;
    struct GuiEventDriveThruDiscovered;

    class OdometerComponent : public BrnFlaptComponent
    {
    public:
        // DWARF BrnOdometerComponent.h:104 -- the readout's current transient state.
        enum EOdometerState
        {
            E_ODOMETERSTATE_NONE           = 0,
            E_ODOMETERSTATE_MILEAGE        = 1,
            E_ODOMETERSTATE_EVENTSFOUND    = 2,
            E_ODOMETERSTATE_DRIVETHRUFOUND = 3,
            E_ODOMETERSTATE_NUM            = 4,
        };

        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lacParentName, s32 liParentAptLayer);              // @0x82415088
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);            // @0x82423F88
        void HandleJunctionChange(const GuiEventJunctionInfo* lpEvent);               // @0x8242BEB0
        void HandleDriveThruDiscovered(const GuiEventDriveThruDiscovered* lpEvent);   // @0x8242C000
        void Update();                                                               // @0x82424160
        void TransIn();                                                              // @0x82424388
        void TransOutActiveText();                                                   // @0x82424468

    private:
        // ---- member layout (DWARF order; offsets above) --------------------
        FlaptAnimatorComponent mMileageAnimator;           // +0x0C
        BrnFlapt::TextFieldRef mMileageTextField;          // +0x44
        FlaptAnimatorComponent mEventsFoundAnimator;       // +0x50
        BrnFlapt::TextFieldRef mEventsFoundTextField;      // +0x88
        FlaptAnimatorComponent mDriveThrusFoundAnimator;   // +0x94
        BrnFlapt::TextFieldRef mDriveThrusFoundTextField;  // +0xCC
        EOdometerState         meOdometerState;            // +0xD8
        f32                    mfEnteredStateTime;         // +0xDC
        GuiCache*              mpGuiCache;                 // +0xE0
        const BrnProgression::Profile* mpProfile;          // +0xE4
    };
}

#endif // BRN_GUI_ODOMETER_COMPONENT_H
