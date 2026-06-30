#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// BrnPhysics::Vehicle::CreatePhysicalTrafficEvent copy assignment  @ 0x825B7AE8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body is a pure bitwise copy of the whole
// object: the VolumeInstanceId/EntityId head (ld/std @0 + lwz/stw @8), VMX (lvx128/stvx128)
// 16-byte block copies across the Matrix44Affine mInitialTransform + Vector3 velocities region
// [16..112), then the scalar tail (mCarAssetAttribKey @112, mModelHandle @120, meTrafficType @124,
// mbIsCab @128, padding, mCgsID @136). CreatePhysicalTrafficEvent is trivially copyable, so
// `= default` reproduces the X360 memberwise copy exactly; kept out-of-line so this ledger func
// has a definition site.
namespace BrnPhysics
{
namespace Vehicle
{
    CreatePhysicalTrafficEvent&
    CreatePhysicalTrafficEvent::operator=( const CreatePhysicalTrafficEvent& ) = default;
}
}
