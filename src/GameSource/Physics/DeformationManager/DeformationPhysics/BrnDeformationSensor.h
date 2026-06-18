#pragma once

#include "types.hpp"

// Minimal reconstructed slice of BrnPhysics::Deformation::DeformationSensor, homed at
// its mirrored DWARF path. The full sensor derives CollidableBody and carries the spec
// pointer, point displacement, stored contacts, a contact spy, a volume-instance id and
// scratch state; only the three trailing members TagPoint::Construct reads are modelled
// here. Names + types are from the DWARF (BrnDeformationSensor.h): the trailing members
// are mpLocalSpaceSphere (Sphere*), mpWorldSpaceSphere (Sphere*) and mfScratchAmount
// (f32, accessor GetScratchAmount()). On the X360 (4-byte pointers) they land at
// +412 / +416 / +420 and the sensor-array stride is 432 bytes; the leading bytes are the
// CollidableBody base + sensor fields. GROW this header into the full layout as further
// sensor TUs land — do not fork it.

namespace BrnPhysics
{
namespace Deformation
{
	// Collision sphere; its leading 16 bytes are centre.xyz + radius.w. Its canonical home
	// lives in the collision code — TagPoint::Construct only needs the centre, which it
	// reads as the sphere's leading Vector4, so a forward declaration suffices here and we
	// avoid forking the real type.
	struct Sphere;

	struct DeformationSensor
	{
		// Opaque leading run (CollidableBody base + spec ptr + displacement + stored
		// contacts + contact spy + volume id + ...) preceding the trailing members below.
		// Sized from the X360 console offset; GROW into the real members later.
		u8 maReserved0[412];

		Sphere* mpLocalSpaceSphere;   // console +412
		Sphere* mpWorldSpaceSphere;   // console +416
		f32     mfScratchAmount;      // console +420 — accumulated scratch / damage amount

		const Sphere* GetLocalSpaceSphere() const { return mpLocalSpaceSphere; }
		const Sphere* GetWorldSpaceSphere() const { return mpWorldSpaceSphere; }
		f32           GetScratchAmount()   const { return mfScratchAmount; }
	};
}
}
