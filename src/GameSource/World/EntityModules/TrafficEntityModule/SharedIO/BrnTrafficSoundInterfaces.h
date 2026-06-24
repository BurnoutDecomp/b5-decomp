#pragma once

// =============================================================================
// BrnTrafficSoundInterfaces.h  (NEW OWNING HEADER -- partial)
//
// DWARF home (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h) of the traffic->sound
// shared-IO types. This slice owns the output interface the traffic module fills
// for the sound system, plus its element record:
//
//   BrnTraffic::BrnTrafficIO::TrafficSoundEntity            (struct @ :46)
//   BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface   (struct @ :111)
//       -> AddTrafficEntity     @ 0x827107A8   (this slice)
//       -> GetTrafficEntityCount@ 0x82681E70   (this slice)
//   (Construct/Clear/GetTrafficEntity/GetTrafficEntityIndex are attested in the
//    DWARF but bodied by their own TUs; declared here so the home is faithful.)
//
// The X360 bakes this header path into GetTrafficEntityCount's assert string
// ("...SharedIO/BrnTrafficSoundInterfaces.h", line 232), fixing the home.
//
// LAYOUT (DWARF-authoritative, member order + offsets pinned by the AddTrafficEntity
// asm @ 0x827107A8):
//   TrafficSoundEntity (sizeof == 80 / 0x50; the asm element stride is 80):
//     Matrix44Affine mLocalTransform   :90  +0x00 (64B, 4 SIMD lanes -- 4x stvx128)
//     EntityId       mEntityId         :91  +0x40 (stw)
//     f32            mfSpeed           :92  +0x44 (stfs)
//     u16            mu16EntityIndex   :93  +0x48 (sth)
//     u8             muVehicleClass    :94  +0x4A (stb)
//     bool           mbIsEngineOn      :95  +0x4B (stb)
//     bool           mbIsHooting       :96  +0x4C (stb)
//     bool           mbIsCrashed       :97  +0x4D (stb)
//     bool           mbIsPhysical      :98  +0x4E (stb)
//     u8             muAlarmType       :99  +0x4F (stb)
//
//   TrafficSoundOutputInterface (the asm reads the count at +0 and the element array
//   at +0x10; the 16-byte gap is the natural alignment pad the 16-aligned element
//   buffer forces after the u16 count):
//     u16                mu16EntityCount     :138  +0x00
//     TrafficSoundEntity maActiveEntityList[32] :139  +0x10
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Matrix44Affine, EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // BrnTrafficSoundInterfaces.h:46 -- one traffic vehicle handed to the sound system.
    struct alignas(16) TrafficSoundEntity
    {
        // BrnTrafficSoundInterfaces.h:49 -- alarm flavour for this vehicle.
        enum AlarmType
        {
            E_ALARM_NONE    = 0,
            E_ALARM_CLASSIC = 1,
            E_ALARM_HORN    = 2,
        };

        Matrix44Affine mLocalTransform;  // :90  +0x00
        EntityId       mEntityId;        // :91  +0x40
        f32            mfSpeed;          // :92  +0x44
        u16            mu16EntityIndex;  // :93  +0x48
        u8             muVehicleClass;   // :94  +0x4A
        bool           mbIsEngineOn;     // :95  +0x4B
        bool           mbIsHooting;      // :96  +0x4C
        bool           mbIsCrashed;      // :97  +0x4D
        bool           mbIsPhysical;     // :98  +0x4E
        u8             muAlarmType;      // :99  +0x4F  (TrafficSoundEntity::AlarmType, 1-byte)

        // BrnTrafficSoundInterfaces.h:87 -- bodied by its own TU (declared-only here).
        AlarmType GetAlarmType() const;
    };

    // BrnTrafficSoundInterfaces.h:111 -- the per-frame traffic->sound output buffer.
    struct TrafficSoundOutputInterface
    {
        // BrnTrafficSoundInterfaces.h:113
        static const u32 KU_MAX_ENTITIES = 32;

        // BrnTrafficSoundInterfaces.h:117 / :121 / :125 / :128 / :131 -- attested in the
        // DWARF; bodied by their own TUs. Declared-only so the home is faithful.
        void Construct();
        void Clear();
        const TrafficSoundEntity& GetTrafficEntity(u16 lu16Index) const;
        const TrafficSoundEntity* GetTrafficEntityIndex(u16 lu16Index) const;

        // BrnTrafficSoundInterfaces.h:125 -- AddTrafficEntity @ 0x827107A8 (this slice).
        // asm: if ((mu16EntityCount + 1) < KU_MAX_ENTITIES) {
        //          maActiveEntityList[mu16EntityCount] = lrEntity;   (80-byte memberwise copy)
        //          ++mu16EntityCount;
        //      }
        void AddTrafficEntity(const TrafficSoundEntity& lrEntity);

        // BrnTrafficSoundInterfaces.h:134 -- GetTrafficEntityCount @ 0x82681E70 (this slice).
        // asm: assert(mu16EntityCount < KU_MAX_ENTITIES); return mu16EntityCount;
        u16 GetTrafficEntityCount() const;

        u16                mu16EntityCount;                       // :138  +0x00
        TrafficSoundEntity maActiveEntityList[KU_MAX_ENTITIES];   // :139  +0x10
    };
}
}
