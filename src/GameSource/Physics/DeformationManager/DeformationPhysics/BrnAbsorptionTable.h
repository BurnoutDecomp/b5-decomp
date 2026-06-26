#pragma once

// BrnPhysics::Deformation absorption-set enum, homed at its mirrored DWARF path
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnAbsorptionTable.h:37).
//
// This pass reconstructs ONLY EAbsorptionSets -- the five deformation-absorption profiles a
// vehicle can be in (normal, AI-crashing, player-extreme-crash, shutdown, invincible). It is the
// type of ImpulseParams::meAbsorptionSet (BrnCollidableBody.h) and DeformableObject::meAbsorptionSet.
// Values are DWARF-authoritative. GROW this header with the AbsorptionTable struct + its lookup
// machinery as that TU lands -- do not fork.

namespace BrnPhysics
{
namespace Deformation
{
    // DWARF BrnAbsorptionTable.h:37. Selects which row of absorption tuning a deformation impulse
    // is scaled against.
    enum EAbsorptionSets
    {
        E_ABSORPTIONSET_NORMAL              = 0,
        E_ABSORPTIONSET_AI_CRASHING         = 1,
        E_ABSORPTIONSET_PLAYER_EXTREME_CRASH = 2,
        E_ABSORPTIONSET_SHUTDOWN            = 3,
        E_ABSORPTIONSET_INVINCIBLE          = 4,
        E_ABSORPTIONSETS_NUM                = 5
    };
}
}
