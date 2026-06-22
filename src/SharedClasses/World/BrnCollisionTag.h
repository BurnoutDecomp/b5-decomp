#pragma once

// BrnWorld::CollisionTag — the 32-bit per-surface collision classification packed into a
// group tag (u16) + material tag (u16). Reconstructed from the DecFIGS DWARF
// (SharedClasses/World/BrnCollisionTag.h: member names/types + the TrafficDirection enum
// + the collision-mask constants) + the X360 ARTIST spine (GetTrafficInfo body,
// authoritative on the bit layout it reads). This is a MINIMAL-COMPLETE slice: it homes
// the type + constants and bodies only GetTrafficInfo (this TU's single ledger function,
// 0x826763B0). The remaining accessors (GetMaterialTag/GetGroupTag/GetSurfaceId/
// IsDrivable/IsFatal/IsSuperFatal/IsInvisible/GetRawData/GetAISectionIndex/Construct) are
// declared (canonical interface, DWARF lines noted) but bodied in their own later slices.
//
// LAYOUT (DWARF :200-:201, X360-consistent): mu16GroupTag @0, mu16MaterialTag @2. The
// GetTrafficInfo asm reads `lhz r11,2(this)` (mu16MaterialTag) then `& 0xF`
// (KU_COLLISION_MASK_TRAFFIC_DATA) -> the 4-bit traffic-direction code.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
    // DWARF BrnCollisionTag.h:78.
    enum TrafficDirection
    {
        E_TRAFFIC_DIRECTION_VALID    = 0,
        E_TRAFFIC_DIRECTION_UNKNOWN  = 1,
        E_TRAFFIC_DIRECTION_NO_LANES = 2,
    };

    // DWARF BrnCollisionTag.h:45-66 (traffic-direction + collision-mask constants).
    const u16 KI_INVALID_SECTION_INDEX        = 32767; // :45
    const u8  KU_TRAFFIC_DIRECTION_MIN_ANGLE  = 2;     // :47
    const u8  KU_TRAFFIC_DIRECTION_MAX_ANGLE  = 15;    // :48
    const u8  KU_TRAFFIC_DIRECTION_ANGLE_RANGE = 14;   // :49
    const u8  KU_TRAFFIC_DIRECTION_UNKNOWN    = 1;     // :51
    const u16 KU_MAX_AI_SECTION_INDEX         = 32767; // :54
    const u8  KU_MAX_SURFACE_ID               = 63;    // :55
    const u16 KU_COLLISION_MASK_TOP_BIT       = 32768; // :58
    const u16 KU_COLLISION_MASK_TRAFFIC_DATA  = 15;    // :59
    const u16 KU_COLLISION_MASK_SURFACE_ID    = 1008;  // :60
    const u16 KU_COLLISION_FLAG_FATAL         = 16384; // :64
    const u16 KU_COLLISION_FLAG_DRIVEABLE     = 8192;  // :65
    const u16 KU_COLLISION_FLAG_SUPERFATAL    = 4096;  // :66

    // DWARF BrnCollisionTag.h:68 — defined (== 17) in BrnCollisionTag.cpp:27.
    extern const u8 KU8_COLLISION_INVISIBLE_SURFACE_ID;

    // DWARF BrnCollisionTag.h:104.
    struct CollisionTag
    {
        void Construct();                              // :110 (own slice)
        void Construct(u16 lu16Group, u16 lu16Material); // :115 (own slice)

        u32 GetRawData() const;                        // :121 (own slice)
        u16 GetMaterialTag() const;                    // :124 (own slice)
        u16 GetGroupTag() const;                       // :127 (own slice)

        // :134 — this TU's function (0x826763B0). Decodes the 4-bit traffic-direction
        // code from mu16MaterialTag and, for VALID lanes, writes the lane angle in
        // radians to *lpfTrafficAngle.
        TrafficDirection GetTrafficInfo(f32* lpfTrafficAngle) const;

        u16  GetAISectionIndex() const;                // :137 (own slice)
        u8   GetSurfaceId() const;                     // :140 (own slice)
        bool IsDrivable() const;                       // :146 (own slice)
        bool IsFatal() const;                          // :149 (own slice)
        bool IsSuperFatal() const;                     // :152 (own slice)
        bool IsInvisible() const;                      // :155 (own slice)

    private:
        u16 mu16GroupTag;     // :200, @0
        u16 mu16MaterialTag;  // :201, @2
    };
}
