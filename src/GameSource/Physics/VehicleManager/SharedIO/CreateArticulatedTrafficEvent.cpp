#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// BrnPhysics::Vehicle::CreateArticulatedTrafficEvent copy assignment  @ 0x8270BF70
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body is a pure bitwise copy of the whole
// object: VMX (lvx128/stvx128) 16-byte block copies across the cab/trailer Matrix44Affine
// transforms + Vector3 velocity region [0..0xC0), then the scalar tail copied with ld/std and
// lwz/stw -- the two VolumeInstanceIds, the two Attribute::Keys, the two ResourceHandles, the two
// CgsIDs and meTrafficType (last store at +256). CreateArticulatedTrafficEvent is trivially
// copyable, so `= default` reproduces the X360 memberwise copy exactly; kept out-of-line so this
// ledger func has a definition site.
namespace BrnPhysics
{
namespace Vehicle
{
    CreateArticulatedTrafficEvent&
    CreateArticulatedTrafficEvent::operator=( const CreateArticulatedTrafficEvent& ) = default;
}
}
