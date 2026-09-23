// BrnPhysics::Vehicle::BrnPlayerDriverControls ledger TU.
//
// Home of the one ledger function for BrnPlayerDriverControls: GetAftertouchValues
// @0x825B2E88. The class itself is reconstructed (additive grow) in BrnVehicleDriverControls.h.
// The other declared members (ctor / Clear / ResetType / GetType) carry no X360 address in
// this TU's ledger and are owned elsewhere -- they are NOT bodied here.
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // The quarter-scale stick-aftertouch modifier (0.25). The X360 leaf multiplies the steering
    // inputs by this (and by its negation) -- see GetAftertouchValues below. DWARF names it
    // KF_STICK_AFTERTOUCH_MODIFIER (BrnVehicleDriverControls.h:34).
    const f32 BrnPlayerDriverControls::KF_STICK_AFTERTOUCH_MODIFIER = 0.25f;

    // @0x825B2E88  BrnPhysics::Vehicle::BrnPlayerDriverControls::GetAftertouchValues
    //
    // Derives the three aftertouch axes from the player's steering/throttle inputs. Recovered
    // store-for-store from the X360 leaf:
    //   lfs f13,0x10(r3) ; fmuls f0,f13,flt_8208F834(=0.25) ; stfs f0,0(r4)
    //     -> outX = mfSteering * 0.25
    //   lbz r11,0x41(r3) ; cmplwi r11,0 ; bne <alt>
    //     -> branch on THIS object's mbIsSteeringWheel (+0x41; r3 is `this`, raw word
    //        0x89630041). The trailing bool the callers load into r7 (ProcessAftertouchEvents
    //        0x82633E1C..3C: meShowtimeBehaviour == 2; UpdateAftertouch 0x8262EE64: li r7,0)
    //        is NEVER READ by this 28-instruction leaf. Until 2026-09-23 the tree branched on
    //        that argument instead (crash parity G47-D1): a pad in showtime behaviour 2 got
    //        the wheel formula, and a steering wheel in UpdateAftertouch got the pad one.
    //   default (selector == 0):
    //     lfs f13,0x14(r3) ; fmuls f0,f13,flt_8200D56C(=-0.25) ; stfs f0,0(r5)
    //     lfs f0,flt_82001CC0(=0.0) ; stfs f0,0(r6)
    //       -> outY = mfForwardSteering * -0.25 ; outZ = 0.0
    //   alt (selector != 0):
    //     lfs f13,0x1C(r3) ; lfs f12,0x08(r3) ; fsubs f12,f13,f12
    //     fmuls f13,f12,flt_8200D56C(=-0.25) ; stfs f13,0(r5)
    //     lfs f0,flt_82001CC0(=0.0) ; stfs f0,0(r6)
    //       -> outY = (mfRequestedGas - mfBrake) * -0.25 ; outZ = 0.0
    //
    // The +/-0.25 magnitude is KF_STICK_AFTERTOUCH_MODIFIER; 0.0 (flt_82001CC0) is the
    // zeroed third axis.
    void BrnPlayerDriverControls::GetAftertouchValues(
            f32& lrfOutX,
            f32& lrfOutY,
            f32& lrfOutZ,
            bool lbUseRequestedGas) const
    {
        lrfOutX = mfSteering * KF_STICK_AFTERTOUCH_MODIFIER;

        (void)lbUseRequestedGas;   // passed by both callers, read by neither console body

        if ( mbIsSteeringWheel )   // lbz r11,0x41(r3)
            lrfOutY = (mfRequestedGas - mfBrake) * -KF_STICK_AFTERTOUCH_MODIFIER;
        else
            lrfOutY = mfForwardSteering * -KF_STICK_AFTERTOUCH_MODIFIER;

        lrfOutZ = 0.0f;
    }
}
}
