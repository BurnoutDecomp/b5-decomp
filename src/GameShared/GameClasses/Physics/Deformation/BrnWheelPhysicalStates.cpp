#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x825C0A00
//   (BrnPhysics::Deformation::WheelPhysicalStates::operator=)
//
// Compiler-generated copy-assignment. The X360 copies the whole 0x188-byte block:
// 24 quadwords (lvx128/stvx128) over 0x000..0x170, then 8 byte copies (lbz/stb)
// over 0x180..0x187. This is a plain field copy of the trivially-copyable block;
// reconstructed as a store-order-faithful memcpy of the two contiguous spans the
// asm walks (the SIMD region first, then the scalar tail), returning *this.

namespace BrnPhysics
{
namespace Deformation
{

WheelPhysicalStates& WheelPhysicalStates::operator=(const WheelPhysicalStates& lkrSource)
{
    // 0x000..0x17F : 24 quadwords (the 384-byte SIMD region), copied first.
    std::memcpy(maQuadwordRegion, lkrSource.maQuadwordRegion, sizeof(maQuadwordRegion));

    // 0x180..0x187 : the 8-byte scalar tail, copied last (the lbz/stb run).
    std::memcpy(maTail, lkrSource.maTail, sizeof(maTail));

    return *this;
}

} // namespace Deformation
} // namespace BrnPhysics
