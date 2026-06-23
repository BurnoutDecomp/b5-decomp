#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"

// Out-of-line body for BrnPhysics::Deformation::DeformationSensor::ClearNonWorldContacts.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x825C1050. It does two
// things, in order:
//
//  1) Swap-remove compaction of the stored-contacts array: walk the live contacts by
//     index; whenever a contact's "non-world" flag (mu32NonWorldFlag) is set, overwrite
//     it with the last live contact (the asm copies the whole 64-byte record as eight
//     64-bit words from `(count<<6)+this-0x20`, i.e. contact[count-1]), decrement the live
//     count, and re-test the slot just refilled before advancing. The asm walks two
//     cursors (contact base and the flag word) in lock-step with the index; modelled here
//     as straightforward index-based swap-remove, which is the same observable transform.
//
//  2) Reset the post-physics scratch state: mfMaxPointDisplacement = 100.0
//     (flt_820049E0), zero the two 16-byte vectors (the `vspltisw v0,0` + two stvx128),
//     and zero mu32PostPhysicsReset (stw of 0 at +0x180).
//
// Caller (X360 xref): BrnPhysics::Deformation::DeformableObject::UpdatePostPhysics.

namespace BrnPhysics
{
namespace Deformation
{
	void DeformationSensor::ClearNonWorldContacts()
	{
		s32 liIndex = 0;
		if ( mi32NumStoredContacts > 0 )
		{
			do
			{
				if ( maStoredContacts[liIndex].mu32NonWorldFlag )
				{
					// Swap-remove: pull the last live contact into this slot, drop the
					// live count, and step back so the refilled slot is re-tested.
					maStoredContacts[liIndex] = maStoredContacts[mi32NumStoredContacts - 1];
					--liIndex;
					--mi32NumStoredContacts;
				}
				++liIndex;
			}
			while ( liIndex < mi32NumStoredContacts );
		}

		// Post-physics scratch reset.
		mfMaxPointDisplacement = 100.0f;
		for ( int i = 0; i < 4; ++i )
		{
			maPostPhysicsVec0[i] = 0.0f;
			maPostPhysicsVec1[i] = 0.0f;
		}
		mu32PostPhysicsReset = 0;
	}
}
}
