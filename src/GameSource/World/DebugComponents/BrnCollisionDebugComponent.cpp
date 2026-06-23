#include "GameSource/World/DebugComponents/BrnCollisionDebugComponent.h"

// BrnWorld::CollisionDebugComponent::GetTrafficColour @ 0x822A8C50.
//
// Maps a scalar (a traffic-density / angle value) onto an 8-step debug colour wheel and writes
// the packed colour word through the result pointer. The X360 scales the input by 4/pi
// (flt_820176A8 == 1.2732395) so a half-turn of angle spans the eight sectors, then walks a
// cascade of `fcmpu`/`bge` comparisons against the unit thresholds 1.0 .. 7.0 (the rodata
// floats flt_82001C98 / flt_82014984 / flt_82004270 / flt_82004EF4 / flt_820149B4 /
// flt_82014A54 / flt_820054D0), selecting one packed 0x20xxxxxx colour per sector:
//
//   scaled <  1.0 -> 0x200000FF   scaled <  5.0 -> 0x20009900
//   scaled <  2.0 -> 0x20000099   scaled <  6.0 -> 0x20FFFF00
//   scaled <  3.0 -> 0x2000FFFF   scaled <  7.0 -> 0x20FF0000
//   scaled <  4.0 -> 0x2000FF00   scaled >= 7.0 -> 0x20FF00FF
//
// Reproduced branch-for-branch / constant-for-constant from the asm. The X360 returns the
// colour through an out pointer (r3); this home returns it by value.

namespace BrnWorld
{
    u32 CollisionDebugComponent::GetTrafficColour(f32 lfValue) const
    {
        // flt_820176A8 == 4/pi (the pseudocode-decoded 1.2732395f).
        const f32 lfScaled = lfValue * 1.2732395f;

        if (lfScaled < 1.0f) return 0x200000FFu;
        if (lfScaled < 2.0f) return 0x20000099u;
        if (lfScaled < 3.0f) return 0x2000FFFFu;
        if (lfScaled < 4.0f) return 0x2000FF00u;
        if (lfScaled < 5.0f) return 0x20009900u;
        if (lfScaled < 6.0f) return 0x20FFFF00u;
        if (lfScaled < 7.0f) return 0x20FF0000u;
        return 0x20FF00FFu;
    }
}
