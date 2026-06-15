#pragma once

// Prop-manager event payloads (the subset the event queues embed). Reconstructed
// from the DecFIGS DWARF (member names/types) + the X360 spine; 16-byte aligned to
// match the queues' inline-buffer alignment.
#include "BrnCommonTypes.h"                                  // Matrix44Affine, Vector3
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"    // BrnWorld::PropEntityID

namespace BrnPhysics
{
    namespace Props
    {
        // Input event: update a prop's physics state.
        struct alignas(16) UpdatePropEvent
        {
            Matrix44Affine         mTransform;
            Vector3                mLinearVelocity;
            Vector3                mAngularVelocity;
            BrnWorld::PropEntityID mEntityId;
            s16                    miPhysicsSlot;
            s16                    miTypeId;
            bool                   mbFrozen;
        };
    }
}
