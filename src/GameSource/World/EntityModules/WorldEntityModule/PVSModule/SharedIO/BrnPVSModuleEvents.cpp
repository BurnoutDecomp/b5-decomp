#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h"

#include <cstddef>   // offsetof
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::PVSIO::GetZoneRequest::GetVelocity @ 0x822A3A48
//
// Semantic parity (not byte-matching). The X360 asserts the out flag pointer is
// non-null, copies the velocity vector (one lvx128/stvx128 16-byte move -- a plain
// copy, not a VMX compute pipeline) and the use-velocity byte. Called by
// BrnWorld::PVSModule::Update.

namespace BrnWorld
{
namespace PVSIO
{
    // Lock the two field offsets the X360 getter reads (lbz 0x24, lvx128 0x30).
    static_assert(offsetof(GetZoneRequest, mbUseVelocity) == 0x24, "mbUseVelocity offset drift");
    static_assert(offsetof(GetZoneRequest, mVelocity)     == 0x30, "mVelocity offset drift");

    void GetZoneRequest::GetVelocity(Vector4* lpOutVelocity, bool* lpbOutUseVelocity) const
    {
        // X360: if (!lpbOutUseVelocity) Begin/Fire/EndAssert("lpbOutUseVelocity is NULL", "...h", 208);
        CGS_ASSERT(lpbOutUseVelocity != nullptr, "lpbOutUseVelocity is NULL\n");

        *lpOutVelocity     = mVelocity;       // lvx128 0x30(this) -> stvx128 *lpOutVelocity
        *lpbOutUseVelocity = mbUseVelocity != 0;  // lbz 0x24(this) -> stb *lpbOutUseVelocity
    }
}
}
