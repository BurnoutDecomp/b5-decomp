#ifndef BRN_SOUND_VEHICLES_ENGINES_AI_PHYSICS_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_AI_PHYSICS_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"   // PhysicsControl base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::AIPhysicsControl
//   GameSource/Sound/Vehicles/Engines/BrnAIPhysicsControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnAIPhysicsControl.h:2):
//   AIPhysicsControl : public BrnSound::Vehicles::Engines::PhysicsControl
// The AI-car physics->engine-audio bridge; adds no data members over PhysicsControl.
// The two leaf vptr installs (primary/EffectControl @+0, IResourceRequester sub-object
// @+4) are produced structurally by the PhysicsControl base spine + the virtual dtor.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct AIPhysicsControl : public PhysicsControl
{
    AIPhysicsControl() {}
    virtual ~AIPhysicsControl();    // anchor for the vector deleting destructor @ 0x826B49F8

    // @ 0x826E4558 -- RTTI factory hook.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_AI_PHYSICS_CONTROL_H
