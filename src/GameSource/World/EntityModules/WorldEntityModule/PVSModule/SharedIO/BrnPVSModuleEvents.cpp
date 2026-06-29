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

    // OutputBuffer member ORDER the PVS-module accessors attest:
    //   mZoneResponseQueue @ +0 (its inline maEvents at the 16-aligned +0x10, so the
    //   EventQueue<GetZoneResponse,8> sub-object spans 0x10 + 8*736 == 0x1710, 16-aligned
    //   to 0x1718 on the X360 32-bit layout), then RequestInterface<512>
    //   mGameDataRequestInterface, whose address BrnWorld::PVSModule::GetGameDataRequestInt
    //   @ 0x822BB068 returns (`addi r3, OutputStructure, 0x1718`).
    //
    // LAYOUT NOTE: the X360 +0x1718 offset is a 32-bit-pointer artifact (the EventQueue base
    // holds a T* + two s32, 12 bytes on X360 vs 16 on the 64-bit host). On the host the member
    // ORDER is what is load-bearing -- the getter returns &mGameDataRequestInterface BY NAME,
    // not by raw offset -- so only the member sequence (queue first, request interface second)
    // is pinned here, not the X360 absolute byte offset (matches the sibling
    // BrnWorldEntityModuleIO host-offset convention).
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mZoneResponseQueue) == 0x0,
                      "PVSIO::OutputBuffer::mZoneResponseQueue must be the first member");
        static_assert(offsetof(OutputBuffer, mGameDataRequestInterface) >
                      offsetof(OutputBuffer, mZoneResponseQueue),
                      "PVSIO::OutputBuffer::mGameDataRequestInterface must follow the response queue");
    }
}
}
