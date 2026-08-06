#pragma once

// BrnPhysics::ContactId -- a 32-bit packed contact identity (queue id in the high byte, an
// 8-bit user-data field, and a 16-bit event index), homed at its mirrored DWARF path
// (references/DecFIGS/dwarfdump/GameSource/Physics/ContactSpies/BrnContactId.h, struct @ :66).
//
// StoredImpulseContact (BrnDeformationSensor.h) embeds a ContactId BY VALUE (mContactId), so the
// car-car-impulse group needs the real one-word layout here rather than an opaque blob. The bit
// layout and the queue-type enum are DWARF-authoritative; the pack/unpack method bodies belong to
// the ContactId TU and are declared-only. GROW with the full method set as that TU lands.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetQueueId's own :171 tripwire)

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    // DWARF BrnContactId.h:37. The custom contact-queue categories a ContactId tags.
    enum ECustomQueueTypes
    {
        E_QUEUE_TYPE_EXTERNAL_FROM_SCENE_CONTACTS = 0,
        E_QUEUE_TYPE_HINGED_BODYPART_WITH_WORLD   = 1,
        E_QUEUE_TYPE_HINGED_BODYPART_WITH_CAR     = 2,
        E_QUEUE_TYPE_DETACHED_BODYPART_WITH_CAR   = 3,
        E_QUEUE_TYPE_DETACHED_WHEEL_WITH_CAR      = 4,
        E_QUEUE_TYPE_RACECAR_WITH_WORLD           = 5,
        E_QUEUE_TYPE_RACECAR_WITH_WORLD_VALIDATED = 6,
        E_QUEUE_TYPE_RACECAR_WITH_RACECAR         = 7,
        E_QUEUE_TYPE_RACECAR_WITH_TRAFFIC         = 8,
        E_QUEUE_TYPE_TRAFFIC_WITH_WORLD           = 9,
        E_QUEUE_TYPE_SIMPLE_TRAFFIC_WITH_WORLD    = 10,
        E_QUEUE_TYPE_SIMPLE_TRAFFIC_WITH_CAR      = 11,
        E_QUEUE_TYPE_TRAFFIC_ARTICULATED_JOINTS   = 12,
        E_QUEUE_TYPE_TRAFFIC_WITH_TRAFFIC         = 13,
        E_NUM_CUSTOM_QUEUE_TYPES                  = 14
    };
}

    struct ContactId
    {
        // DWARF BrnContactId.h:104-106. The bit fields packed into the single id word:
        //   bits 24..31 (0xFF000000) -- queue id (ECustomQueueTypes)
        //   bits 16..23 (0x00FF0000) -- user data
        //   bits  0..15 (0x0000FFFF) -- event index
        static const u32 KU_QUEUE_ID_MASK    = 0xFF000000u;
        static const u32 KU_USER_DATA_MASK   = 0x00FF0000u;
        static const u32 KU_EVENT_INDEX_MASK = 0x0000FFFFu;

        // DWARF :98-100. Constructed empty or from a raw packed word; the rest of the pack/unpack
        // surface is owned by the ContactId TU (declared-only).
        ContactId() : muId(0) {}
        explicit ContactId(u32 luId) : muId(luId) {}

        void Set(PhysicsModuleIO::ECustomQueueTypes leQueue, s32 liEventIndex, u8 lu8UserData);
        void SetQueueId(PhysicsModuleIO::ECustomQueueTypes leQueue);
        void SetEventIndex(u16 lu16EventIndex);
        void SetUserData(u8 lu8UserData);

        // GetQueueId / GetEventIndex: INLINE, newly attested 2026-08-06 by
        // PhysicsModuleIO::PotentialContactInterface::GetEvent(ContactId) @0x825A06A0, which
        // inlines both (srwi r,24 for the queue id, clrlwi 16 for the event index) and carries
        // GetQueueId's own bounds tripwire verbatim -- FireAssert cites THIS header, line 171
        // ("luQueueId < static_cast<uint32_t>( PhysicsModuleIO::E_NUM_CUSTOM_QUEUE_TYPES )"),
        // proving the bodies were header-inline on the console (no out-of-line emission exists).
        // Fire-and-continue, exactly as the caller's asm does.
        PhysicsModuleIO::ECustomQueueTypes GetQueueId() const
        {
            const u32 luQueueId = muId >> 24;
            CGS_ASSERT(luQueueId < static_cast<u32>(PhysicsModuleIO::E_NUM_CUSTOM_QUEUE_TYPES),
                       "luQueueId < static_cast<uint32_t>( PhysicsModuleIO::E_NUM_CUSTOM_QUEUE_TYPES )");
            return static_cast<PhysicsModuleIO::ECustomQueueTypes>(luQueueId);
        }
        u16 GetEventIndex() const { return static_cast<u16>(muId & KU_EVENT_INDEX_MASK); }
        u8  GetUserData()   const;

        // DWARF :100. Implicit conversion to the raw packed word.
        operator u32() const { return muId; }

    private:
        // DWARF BrnContactId.h:108. The single packed id word.
        u32 muId;
    };
}
