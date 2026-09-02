#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x825C0A00
//   (BrnPhysics::Deformation::WheelPhysicalStates::operator=)
//
// Compiler-generated copy-assignment. The X360 copies the whole 0x188-byte block:
// 24 quadwords (lvx128/stvx128) over 0x000..0x170 -- the four 96-byte WheelPhysicalState
// entries -- then 8 byte copies (lbz/stb) over 0x180..0x187 -- mabWheelExists[4] and
// mabWheelAttached[4]. Reproduced as the member-wise copy in the same order.

namespace BrnPhysics
{
namespace Deformation
{

WheelPhysicalStates& WheelPhysicalStates::operator=(const WheelPhysicalStates& lkrSource)
{
    // 0x000..0x17F : the four per-wheel SIMD entries (24 quadwords), copied first.
    for (u32 luWheel = 0; luWheel < KU_NUM_WHEELS; ++luWheel)
    {
        maStates[luWheel].mWorldSpaceTransform       = lkrSource.maStates[luWheel].mWorldSpaceTransform;
        maStates[luWheel].mWorldSpaceVelocity        = lkrSource.maStates[luWheel].mWorldSpaceVelocity;
        maStates[luWheel].mWorldSpaceAngularVelocity = lkrSource.maStates[luWheel].mWorldSpaceAngularVelocity;
    }

    // 0x180..0x187 : the 8 flag bytes, copied last (the lbz/stb run).
    for (u32 luWheel = 0; luWheel < KU_NUM_WHEELS; ++luWheel)
    {
        mabWheelExists[luWheel]   = lkrSource.mabWheelExists[luWheel];
        mabWheelAttached[luWheel] = lkrSource.mabWheelAttached[luWheel];
    }

    return *this;
}

} // namespace Deformation
} // namespace BrnPhysics
