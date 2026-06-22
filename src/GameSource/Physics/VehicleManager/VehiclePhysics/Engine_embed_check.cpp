// Embed check for the Vehicle-physics Engine TU: include the home + exercise the bodied funcs.
// Compile-only (the gate runs `cl /c`), so the declared-only externals (EngineAttribs::Construct,
// Engine::Reset) need no definitions here.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"

#include <cstddef>

using BrnPhysics::Vehicle::Engine;
using BrnPhysics::Vehicle::EngineAttribs;

// The attribs block Prepare memcpy's must be exactly 160 bytes (== committed
// VehicleAttribs::EngineAttribs sizeof 0xA0) for the byte copy to be correct.
static_assert(sizeof(EngineAttribs) == 0xA0, "EngineAttribs slice size drift");

void Engine_embed_check()
{
    Engine lEngine;
    lEngine.Construct();

    EngineAttribs lAttribs;
    lEngine.Prepare(&lAttribs);
}
