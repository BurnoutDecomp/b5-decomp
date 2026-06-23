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

        // A resolved contact involving a traffic vehicle. The X360
        // BaseEventQueue<TrafficContact>::AddEventSafe @ 0x825A33F8 copies each element as
        // exactly twelve 64-bit block moves (ctr = 12, std loop) at a 96-byte (0x60) stride
        // (`v4*3*32`, `slwi ..,5`), i.e. sizeof(TrafficContact) == 96 -- identical to
        // BaseContact (EntityId@0 + EntityId@4 + CollisionTag@8, then five 16-byte Vector3s
        // @16/32/48/64/80, alignas(16) => 96). The contact carries the same resolved-contact
        // fields as BaseContact and adds no members of its own, so it is modelled as a
        // BaseContact specialisation (no extra storage; sizeof stays 96).
        struct alignas(16) TrafficContact : public BaseContact
        {
        };
    }
}
