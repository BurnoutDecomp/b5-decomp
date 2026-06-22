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

        // FLAGGED ADDITIVE GROW (Traffic-Physics group): the DecFIGS DWARF homes
        // BrnPhysics::Props::PropUpdateNotification in this same BrnPropEvents.h
        // (struct @ BrnPropEvents.h:88; members at source lines 168-172), so its
        // home is here alongside its sibling UpdatePropEvent. Nothing above is
        // modified; this only adds the new event type.
        //
        // Output notification: a prop's post-physics state, queued back toward the
        // sound/world bridge. Member names/types are DWARF-authoritative. The X360
        // event-queue block-copy uses a 64-byte stride (`slwi r,r,6` in
        // BaseEventQueue<PropUpdateNotification>::Append @ 0x823C3DA8), so the
        // 16-byte alignment (Vector3 is alignas(16)) sizes the record to exactly 64
        // bytes (48 for the three Vector3 + 4 PropEntityID + 2 i16, padded to 64) --
        // the stride the Append memcpy is store-for-store faithful to.
        struct alignas(16) PropUpdateNotification
        {
            Vector3                mPosition;        // BrnPropEvents.h:168
            Vector3                mLinearVelocity;  // BrnPropEvents.h:169
            Vector3                mAngularVelocity; // BrnPropEvents.h:170
            BrnWorld::PropEntityID mEntityId;        // BrnPropEvents.h:171
            s16                    mi16TypeId;       // BrnPropEvents.h:172

            // Out-of-line accessors the DWARF attests at BrnPropEvents.h:144-153.
            // These live in their own (not-yet-reconstructed) TUs; declared here so
            // the home is faithful, bodied elsewhere. NOT owned by this slice.
            void    Construct(Vector3 lPosition, Vector3 lLinearVelocity,
                              Vector3 lAngularVelocity, BrnWorld::PropEntityID lEntityId,
                              s16 li16TypeId);                              // :144
            Vector3 GetPosition() const;                                   // :147
            Vector3 GetLinearVelocity() const;                             // :150
            Vector3 GetAngularVelocity() const;                            // :153
            // X360 0x826944E0: asserts the owner byte (mEntityId+0, bits[24..31]) == 3 then
            // returns mEntityId by value -- the inlined PropEntityID::AssertIsProp tripwire.
            BrnWorld::PropEntityID GetEntityId() const;                    // :156 / 0x826944E0
        };
    }
}
