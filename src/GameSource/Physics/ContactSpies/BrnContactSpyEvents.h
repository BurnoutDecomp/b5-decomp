#pragma once

#include "BrnCommonTypes.h"

namespace BrnPhysics
{
    namespace ContactSpy
    {
        // Wheel/limb identifier used by the deformable hinged parts.
        // Recovered from BrnIKBodyPart.h (DecFIGS DWARF).
        enum EBodyParts : s32
        {
            E_BODY_PART_NONE = -1
        };

        // Base class of the CgsModule event hierarchy. The X360 layout carries no
        // data members of its own for these contact events; it exists so the
        // queues can treat every contact uniformly.
        struct Event
        {
        };

        // BrnContactSpyEvents.h: a single resolved contact between two entities.
        struct alignas(16) BaseContact : public Event
        {
            EntityId     mEntityIdA;
            EntityId     mEntityIdB;
            CollisionTag mCollisionTagB;
            Vector3      mFrictionStress;
            Vector3      mNormalStress;
            Vector3      mNormal;
            Vector3      mPointOnA;
            Vector3      mPointOnB;
        };

        struct alignas(16) HingedPartContact : public BaseContact
        {
            EBodyParts meType;
        };

        struct alignas(16) PhysicalCarPartContact : public BaseContact
        {
            Vector3    mVelocity;
            EBodyParts meType;
            bool       mbIsHinged;
        };

        // A contact that was rejected before becoming a full BaseContact.
        struct alignas(16) DiscardedContact
        {
            EntityId mEntityIdA;
            EntityId mEntityIdB;
            f32      mfClosingVelocity;
            Vector3  mNormal;
            Vector3  mPointOnA;
            Vector3  mPointOnB;
        };
    }
}
