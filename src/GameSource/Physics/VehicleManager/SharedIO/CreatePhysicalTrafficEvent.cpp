#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// BrnPhysics::Vehicle::CreatePhysicalTrafficEvent copy assignment  @ 0x825B7AE8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body is a pure bitwise copy of the whole
// object: the VolumeInstanceId/EntityId head (ld/std @0 + lwz/stw @8), VMX (lvx128/stvx128)
// 16-byte block copies across the Matrix44Affine mInitialTransform + Vector3 velocities region
// [16..112), then the scalar tail. CORRECTED 2026-08-01 (physics wave 1): the tail is
// mCarAssetAttribKey @0x70 copied as ONE `ld/std` DOUBLEWORD (8 bytes, not 4), mModelHandle
// @0x78 as two words, meTrafficType @0x80, mbIsCab @0x84, mCgsID @0x88 as an `ld/std`. The old
// "@112 / @120 / @124 / @128 / @136" annotation was derived from a 4-byte Attribute::Key and is
// wrong -- see BrnVehicleEvents.h's RaceCarState banner. CreatePhysicalTrafficEvent is trivially copyable, so
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
