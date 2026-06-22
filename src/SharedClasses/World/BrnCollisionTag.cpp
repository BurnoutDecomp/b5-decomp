#include "SharedClasses/World/BrnCollisionTag.h"

// BrnWorld::CollisionTag, reconstructed from BURNOUT_X360_ARTIST.XEX. This TU defines the
// invisible-surface-id global and bodies GetTrafficInfo (0x826763B0); the other accessors
// live in their own slices (declared in BrnCollisionTag.h).

namespace BrnWorld
{
    // DWARF BrnCollisionTag.cpp:27 — the surface id used to mark invisible collision.
    extern const u8 KU8_COLLISION_INVISIBLE_SURFACE_ID = 17;

    // 0x826763B0: read the 4-bit traffic-direction code from mu16MaterialTag
    // (`lhz r11,2(this); clrlwi r11,r11,28` == mu16MaterialTag & KU_COLLISION_MASK_TRAFFIC_DATA):
    //   code == E_TRAFFIC_DIRECTION_UNKNOWN (1) -> return UNKNOWN, no angle written
    //   code == 0                                -> return NO_LANES (E_TRAFFIC_DIRECTION_NO_LANES == 2)
    //   otherwise (a valid 2..15 lane code):
    //     *lpfTrafficAngle = (code - KU_TRAFFIC_DIRECTION_MIN_ANGLE)
    //                        * (1.0f / KU_TRAFFIC_DIRECTION_ANGLE_RANGE) * (2 * PI)
    //     then assert 0 <= angle < 2*PI; return VALID (0).
    // The asm widens (code - 2) to a 64-bit signed int, fcfid->frsp to f32, then
    // `fmuls f0, f13, (1/14)` and `fmuls f0, f0, (2*PI)`, stores f32 to *lpfTrafficAngle,
    // and compares the stored f32 against 0 / 2*PI. Constants 0.071428575 == 1/14 and
    // 6.2831855 == 2*PI are reproduced as the symbolic ANGLE_RANGE / 2*RwMath::PI values.
    TrafficDirection CollisionTag::GetTrafficInfo(f32* lpfTrafficAngle) const
    {
        const u32 luTrafficInfo = mu16MaterialTag & KU_COLLISION_MASK_TRAFFIC_DATA;

        if (luTrafficInfo == E_TRAFFIC_DIRECTION_UNKNOWN)
            return E_TRAFFIC_DIRECTION_UNKNOWN;
        if (luTrafficInfo == 0)
            return E_TRAFFIC_DIRECTION_NO_LANES;

        const f32 KF_TWO_PI = 6.2831855f;   // 2.0f * RwMath::PI
        const f32 lfAngleRatio =
            static_cast<f32>(static_cast<s32>(luTrafficInfo) - KU_TRAFFIC_DIRECTION_MIN_ANGLE)
            * (1.0f / static_cast<f32>(KU_TRAFFIC_DIRECTION_ANGLE_RANGE));
        *lpfTrafficAngle = lfAngleRatio * KF_TWO_PI;

        CGS_ASSERT(*lpfTrafficAngle >= 0.0f && *lpfTrafficAngle < KF_TWO_PI,
                   "*lpfTrafficAngle >= 0.0f && *lpfTrafficAngle < (2.0f * RwMath::PI)");

        return E_TRAFFIC_DIRECTION_VALID;
    }
}
