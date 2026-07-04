#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line body of the write-lock-checked byte-offset accessor of
// BrnAI::RouteMapModuleIO::OutputBuffer that the X360 ARTIST build emitted
// out-of-line (X360 0x8276B0A0). Same idiom as the committed
// BrnAIModuleIO_OutputBuffer.cpp accessors: raw u8* return to `this + <attested
// offset>`, guarded by whichever lock bit the asm names. The X360 file/line
// assert args are dropped per project policy.

namespace BrnAI
{
namespace RouteMapModuleIO
{
    // X360 0x8276B0A0 (W, :617) -- write-lock (status bit 3, `lbz 0(this); extrwi
    // r11,r11,1,28` == (*p>>3)&1); on failure streams "Not locked for writing".
    // Returns this + 4 (`addi r3,this,4`), i.e. &mRouteResponseQueue. Caller
    // RouteMapModule::Update. Faithfully tests the WRITE bit (do not "fix" to read).
    u8* OutputBuffer::GetRouteResponseQueueForWrite()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_ROUTE_RESPONSE_QUEUE_OFFSET;
    }
}
}
