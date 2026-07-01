#include "GameSource/Sound/Vehicles/Engines/BrnAIPhysicsControl.h"

// =============================================================================
// BrnSound::Vehicles::Engines::AIPhysicsControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObject(u32)              @ 0x826E4558
//   `vector deleting destructor'   @ 0x826B49F8  (-> ~AIPhysicsControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// AIPhysicsControl::CreateObject(u32)  @ 0x826E4558   (the RTTI factory hook)
//
// The X360 allocates a 1264-byte (0x4F0) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "AIPhysicsControl" and inline-constructs an
// AIPhysicsControl, returning it upcast to CgsSound::Logic::EffectControl*. `a1` only
// selects the operator-new flavour (0/1); both arms use the same size + ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the observable result matches. Mirrors
// the committed CollisionControl::CreateObject. The 0x4F0 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* AIPhysicsControl::CreateObject( u32 /*luType*/ )
{
    return new AIPhysicsControl();
}

// ---------------------------------------------------------------------------
// ~AIPhysicsControl  @ 0x826B49F8  (anchor for the X360 `vector deleting destructor').
// The X360 leaf installs off_820AF4B4 @ +0 and off_820AF480 @ +4 then chains
// PhysicsControl::~PhysicsControl; AIPhysicsControl adds no member teardown of its own,
// so this leaf body is empty. The (a2 & 1) allocator-free tail is re-synthesised by the
// host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
AIPhysicsControl::~AIPhysicsControl()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
