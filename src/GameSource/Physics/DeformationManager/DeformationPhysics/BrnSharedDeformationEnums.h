#pragma once

// BrnPhysics::Deformation shared enums, homed at their mirrored DWARF path
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnSharedDeformationEnums.h:32).
//
// This pass reconstructs ONLY ENextSensorDirection -- the six signed-axis directions a
// deformation impulse can be steered along. It is the type of ImpulseParams::meImpulseDirection
// (BrnCollidableBody.h) and CollidableBody::GetDirectionVector / DeformationSensor::GetNextSensor.
// Values are DWARF-authoritative. GROW this header with the remaining shared deformation enums
// as their consuming TUs land -- do not fork.

namespace BrnPhysics
{
namespace Deformation
{
    // DWARF BrnSharedDeformationEnums.h:32. The signed body axis a sensor impulse / next-sensor
    // walk is directed along (positive then negative X/Y/Z), terminated by the count sentinel.
    enum ENextSensorDirection
    {
        E_NSD_POS_XAXIS = 0,
        E_NSD_NEG_XAXIS = 1,
        E_NSD_POS_YAXIS = 2,
        E_NSD_NEG_YAXIS = 3,
        E_NSD_POS_ZAXIS = 4,
        E_NSD_NEG_ZAXIS = 5,
        E_NSD_NUM       = 6
    };
}
}
