// BrnPhysics::Vehicle::VehiclePhysics::ShuntEffect ledger TU.
//
// Home of the one ledger function for ShuntEffect: IsActive @0x8236AFC0. The nested
// ShuntEffect struct (and its sibling SlamEffect) are reconstructed in VehiclePhysics.h;
// this TU only provides the out-of-line body for IsActive. The other ShuntEffect accessors
// (Clear / GetDesiredSpeed / ForceSlip / GetLife / GetSpeedIncreaseToQuit / SetLife /
// SetSpeedIncreaseToQuit) are declared-only in the header and owned elsewhere -- they carry
// no X360 address in this TU's ledger, so they are deliberately NOT bodied here.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // @0x8236AFC0  BrnPhysics::Vehicle::VehiclePhysics::ShuntEffect::IsActive
    //
    // Store-for-store from the X360 leaf:
    //   lvx128 v13,r0,r3 ; vspltw v13,v13,3 ; vcmpgtfp. v13,v13,{0,0,0,0}
    //     -> load the register at +0x00 (mDirectionPlusDesiredSpeed), broadcast lane 3
    //        (the "plus" / desired-speed w lane) and test > 0. If not greater, return false.
    //   addi r11,r3,0x10 ; lvx128 v13,r0,r11 ; vspltw v13,v13,0 ; vcmpgtfp. v0,v13,{0,...}
    //     -> load the register at +0x10 (mv4_Life_SpeedIncreaseToQuit), broadcast lane 0
    //        (the life x lane) and test > 0; that result is the return value.
    // i.e. the shunt is active iff its desired speed AND its remaining life are both positive.
    bool VehiclePhysics::ShuntEffect::IsActive() const
    {
        if ( !(mDirectionPlusDesiredSpeed.w > 0.0f) )
            return false;
        return mv4_Life_SpeedIncreaseToQuit.x > 0.0f;
    }
}
}
