#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"
#include <cstddef>   // offsetof

// X360-attested shape (CarState::GetSensor): 80-byte per-sensor stride and the count byte
// at console +0x6A4.
static_assert(sizeof(BrnPhysics::Deformation::CarSensorState) == 0x50, "CarSensorState stride 80");
static_assert(offsetof(BrnPhysics::Deformation::CarState, mu8NumSensors) == 0x6A4, "mu8NumSensors @ +0x6A4");

// BrnPhysics::Deformation::CarState::GetSensor @ 0x825B3678
// Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative). A checked accessor:
//   - reads mu8NumSensors (byte at this+0x6A4)
//   - asserts the index is in range ("luSensorIndex < mu8NumSensors", baked
//     BrnDeformationState.h:62) -- a non-gating tripwire
//   - returns &maSensors[luSensorIndex] (this + 80 * luSensorIndex; the asm computes the
//     80-byte stride as index*5<<4).
// The X360 return is a pointer (ABI-returned T&); modelled as the CarSensorState& the
// member-by-name access yields.

namespace BrnPhysics
{
namespace Deformation
{
    CarSensorState& CarState::GetSensor(u8 luSensorIndex)
    {
        CGS_ASSERT(luSensorIndex < mu8NumSensors, "luSensorIndex < mu8NumSensors");
        return maSensors[luSensorIndex];
    }
}
}
