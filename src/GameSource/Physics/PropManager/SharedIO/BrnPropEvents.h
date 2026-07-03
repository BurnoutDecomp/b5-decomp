#pragma once

// Prop-manager event payloads (the subset the event queues embed). Reconstructed
// from the DecFIGS DWARF (member names/types) + the X360 spine; 16-byte aligned to
// match the queues' inline-buffer alignment.
#include "BrnCommonTypes.h"                                  // Matrix44Affine, Vector3
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"    // BrnWorld::PropEntityID
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h" // BrnWorld::EPropState

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

        // FLAGGED ADDITIVE GROW (Prop-Physics group): DWARF homes this at BrnPropEvents.h:45.
        // Input event: promote a prop instance to a physical (simulated) body. The X360
        // AddEvent (@0x822C8308) stores Matrix44Affine mTransform (4x16B VMX blocks @0..0x3F)
        // then the scalar tail stw @0x40 (mEntityId) / stw @0x44 (meState, 4B) / sth @0x48
        // (miPropTypeId) / sth @0x4A (miSlot) / stb @0x4C (mbAddExtraComOffset). alignas(16)
        // (leading Matrix44Affine) => sizeof == 80, the stride the queue memcpy is faithful to
        // (BaseEventQueue<AddPhysicalPropEvent>::AddEvent @0x822C8308 / Append @0x827A7E50, and
        //  EventQueue<AddPhysicalPropEvent,50>::Construct @0x822E36E0).
        struct alignas(16) AddPhysicalPropEvent
        {
            Matrix44Affine         mTransform;            // BrnPropEvents.h:47  @0x00 (64B)
            BrnWorld::PropEntityID mEntityId;             // BrnPropEvents.h:48  @0x40 (4B)
            BrnWorld::EPropState   meState;               // BrnPropEvents.h:49  @0x44 (4B enum)
            s16                    miPropTypeId;          // BrnPropEvents.h:50  @0x48
            s16                    miSlot;                // BrnPropEvents.h:51  @0x4A
            bool                   mbAddExtraComOffset;   // BrnPropEvents.h:52  @0x4C
        };

        // FLAGGED ADDITIVE GROW (Prop-Physics group): DWARF homes this at BrnPropEvents.h:64.
        // Input event: promote a prop PART to a physical body. The X360 AddEvent (@0x822C8498)
        // stores Matrix44Affine mTransform (4x16B VMX blocks @0..0x3F) then the scalar tail
        // stw @0x40 (mEntityId) / sth @0x44 (miPropTypeId) / sth @0x46 (miPartId) / sth @0x48
        // (miSlot). alignas(16) => sizeof == 80, the stride the queue memcpy is faithful to
        // (BaseEventQueue<AddPhysicalPartEvent>::AddEvent @0x822C8498 / Append @0x827A7F40, and
        //  EventQueue<AddPhysicalPartEvent,50>::Construct @0x822E3750).
        struct alignas(16) AddPhysicalPartEvent
        {
            Matrix44Affine         mTransform;    // BrnPropEvents.h:66  @0x00 (64B)
            BrnWorld::PropEntityID mEntityId;     // BrnPropEvents.h:67  @0x40 (4B)
            s16                    miPropTypeId;  // BrnPropEvents.h:68  @0x44
            s16                    miPartId;      // BrnPropEvents.h:69  @0x46
            s16                    miSlot;        // BrnPropEvents.h:70  @0x48
        };

        // FLAGGED ADDITIVE GROW (Prop-Physics group): DWARF homes this at BrnPropEvents.h:104.
        // Input event: remove a whole physical PROP instance by its physical-index slot.
        // 8-byte / 4-aligned record. The X360 BaseEventQueue<RemovePhysicalPropEvent>::AddEvent
        // (@0x822C8620) indexes with `slwi r11,miLength,3` (stride 8) and copies the record as
        // two 4-byte word stores (word0 = mEntityId @+0, word1 = the s32 index @+4); Append
        // (@0x827A8030) XMemCpys at the same 8-byte stride, and EventQueue<...,300>::Construct
        // (@0x822E37C0) reserves maEvents at base+0xC (no padding => align 4). PropEntityID is a
        // single 4-byte packed word (BrnPropEntityID.h). DO NOT add alignas(16) here: the slwi,3
        // stride would desync if sizeof padded to 16. Member name miPhysicalIndex is DWARF
        // source-line attested (BrnPropEvents.h:107).
        struct RemovePhysicalPropEvent
        {
            BrnWorld::PropEntityID mEntityId;        // BrnPropEvents.h:106  @+0x0 (stw word0)
            s32                    miPhysicalIndex;  // BrnPropEvents.h:107  @+0x4 (stw word1)
        };
        static_assert(sizeof(RemovePhysicalPropEvent) == 8,
                      "RemovePhysicalPropEvent stride 8 (slwi,3)");

        // FLAGGED ADDITIVE GROW (Prop-Physics group): DWARF homes this at BrnPropEvents.h:119.
        // Input event: remove a single physical PART of a prop by its physical-index slot.
        // Same 8-byte / 4-aligned layout as the whole-prop remove event. The X360
        // BaseEventQueue<RemovePhysicalPartEvent>::AddEvent (@0x822C8768) uses `slwi r11,3`
        // (stride 8) + two word stores; Append (@0x827A8110) XMemCpys at stride 8; and
        // EventQueue<...,100>::Construct (@0x822E3830) reserves maEvents at base+0xC. Member name
        // miPhysicalIndex is DWARF source-line attested (BrnPropEvents.h:122).
        struct RemovePhysicalPartEvent
        {
            BrnWorld::PropEntityID mEntityId;        // BrnPropEvents.h:121  @+0x0 (stw word0)
            s32                    miPhysicalIndex;  // BrnPropEvents.h:122  @+0x4 (stw word1)
        };
        static_assert(sizeof(RemovePhysicalPartEvent) == 8,
                      "RemovePhysicalPartEvent stride 8 (slwi,3)");

        // FLAGGED ADDITIVE GROW (Prop-Physics group): the DWARF home of PropRaceCarContact is
        // BrnPropManager.h:56, but that TU is an intentional BLOCKED stub (its class layout is
        // ungroundable until CgsSceneManager::CgsCollision::BaseCollisionGenerator lands), so
        // the file must stay UNCHANGED. This standalone, pointer-free prop-event payload is
        // therefore homed here alongside its prop-event siblings so the CLEAN
        // EventQueue<PropRaceCarContact,30>::Construct (@0x825A81B8) instantiation has a complete
        // element type. Move to BrnPropManager.h if/when that TU is unblocked.
        //
        // DWARF layout (BrnPropManager.h:56-59): { Vector3 mForce@0; PropEntityID mPropEntityId@0x10 }.
        // Vector3 is alignas(16) => the record is 16-aligned; 16B + 4B == 20 used, padded to a
        // 32-byte stride. NOTE: the 32-byte stride is derived from the DWARF layout + alignment,
        // NOT confirmed by an AddEvent/Append memcpy (no such dossier exists for this type); 32 is
        // the only 16-aligned size >= 20. Flagged medium-confidence.
        struct alignas(16) PropRaceCarContact
        {
            Vector3                mForce;         // BrnPropManager.h:58  @0x00 (16B)
            BrnWorld::PropEntityID mPropEntityId;  // BrnPropManager.h:59  @0x10 (4B)
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
