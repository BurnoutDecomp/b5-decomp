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
    CGS_STATIC_ASSERT((offsetof(GetZoneRequest, mbUseVelocity) == 0x24), "mbUseVelocity offset drift");
    CGS_STATIC_ASSERT((offsetof(GetZoneRequest, mVelocity)     == 0x30), "mVelocity offset drift");

    void GetZoneRequest::GetVelocity(Vector4* lpOutVelocity, bool* lpbOutUseVelocity) const
    {
        // X360: if (!lpbOutUseVelocity) Begin/Fire/EndAssert("lpbOutUseVelocity is NULL", "...h", 208);
        CGS_ASSERT(lpbOutUseVelocity != 0, "lpbOutUseVelocity is NULL\n");

        *lpOutVelocity     = mVelocity;       // lvx128 0x30(this) -> stvx128 *lpOutVelocity
        *lpbOutUseVelocity = mbUseVelocity != 0;  // lbz 0x24(this) -> stb *lpbOutUseVelocity
    }

    void OutputBuffer::_AssertLayout()
    {
#if defined(D_PLATFORM_X360) && D_PLATFORM_X360
        CGS_STATIC_ASSERT((sizeof(GetZoneResponse) == 0x2E0), "response stride");
        CGS_STATIC_ASSERT((__alignof(GetZoneResponse) == 8), "response alignment");
        CGS_STATIC_ASSERT((sizeof(OutputInterface::ZoneResponseQueue) == 0x1710), "response queue extent");
        CGS_STATIC_ASSERT((offsetof(OutputInterface, muTotalZones) == 0x1710), "total zones offset");
        CGS_STATIC_ASSERT((sizeof(OutputInterface) == 0x1718), "output interface extent");
        CGS_STATIC_ASSERT((offsetof(OutputBuffer, mOutputInterface) == 0), "output interface offset");
        CGS_STATIC_ASSERT((offsetof(OutputBuffer, mGameDataRequestInterface) == 0x1718), "request interface offset");
        CGS_STATIC_ASSERT((sizeof(OutputBuffer) == 0x1930), "output buffer extent");
        CGS_STATIC_ASSERT((__alignof(OutputBuffer) == 16), "output buffer alignment");
#endif
    }

}
}
