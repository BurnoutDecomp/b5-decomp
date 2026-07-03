#pragma once

#include "BrnCommonTypes.h"

// The scene-manager potential-contact record consumed by BaseContact::Construct
// (its canonical home is GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h).
// Forward-declared here so the header does not pull in the SceneManager IO tree; the
// factory .cpp includes the full definition.
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; } }

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

            // BrnContactSpyEvents.h:238 static factory (X360 0x8259F6F8). Stamps *lpResult
            // from the spy's five leading stress/geometry vectors (lpInSpy[0..4]) plus the
            // scene-manager potential contact's entity words (each VolumeInstanceId's HIGH
            // dword) and swapped/sentinel poly tag.
            static BaseContact* Construct(
                BaseContact* lpResult,
                const Vector3* lpInSpy,
                const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact);
        };

        // A resolved contact involving a race car. Adds no members of its own over
        // BaseContact (DWARF BrnContactSpyEvents.h:101 `struct RaceCarContact : public
        // BaseContact {}`). The X360 BaseEventQueue<RaceCarContact>::AddEventSafe @ 0x825A3338
        // copies each element as exactly twelve 64-bit block moves (ctr = 12) at a 96-byte
        // (0x60) stride (`v4*3*32`, `slwi ..,5`), i.e. sizeof(RaceCarContact) == 96 -- identical
        // to BaseContact, so no extra storage; sizeof stays 96.
        struct alignas(16) RaceCarContact : public BaseContact
        {
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

        // A resolved contact involving a smashable prop (DWARF BrnContactSpyEvents.h:165).
        // The X360 BaseEventQueue<PropContact>::AddEventSafe @ 0x825A3570 copies each element
        // as exactly fourteen 64-bit block moves (ctr = 14) at a 112-byte (0x70) stride
        // (`v4*0x70`), i.e. sizeof(PropContact) == 112: BaseContact(96) + the four trailing
        // small fields below, alignas(16) => 112.
        struct alignas(16) PropContact : public BaseContact
        {
            static const u16 KU_UNKNOWN_PROP_TYPE = 65535;   // BrnContactSpyEvents.h:167
            static const u8  KU_FLAG_SMASH_GATE   = 1;       // BrnContactSpyEvents.h:169
            static const u8  KU_FLAG_BILLBOARD    = 2;       // BrnContactSpyEvents.h:170

            u16 muType;        // @96
            u8  muState;       // @98
            u8  muFlags;       // @99
            u8  muBeganMoving; // @100
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
