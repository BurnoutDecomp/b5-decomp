#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line bodies of BrnAI::RouteMapModuleIO::InputBuffer's two NON-const producer-side
// route-request-queue getters that the X360 ARTIST build emitted out-of-line. Each asserts the
// WRITE lock (status bit 3 == IsBufferLockedForWriting) and returns the address of its embedded
// queue member. The header non-const getters are reduced to declarations so these are the sole
// (ODR) definitions; the const overloads stay inline in the header. Lock-assert strings carry the
// trailing "\n" of the X360 rodata, reproduced verbatim (not "fixed").

namespace BrnAI
{
namespace RouteMapModuleIO
{
    // X360 0x8276AE00 (W) -- the race-route request queue (this+0x10). Producer-side
    // non-const getter: asserts the WRITE lock (status bit 3), reproduced verbatim.
    RaceRouteRequestQueue* InputBuffer::GetRaceRouteRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mRaceRouteRequestQueue;
    }

    // X360 0x8276AF50 (W) -- the extrapolated-route request queue (this+0xA0). Producer-side
    // non-const getter: asserts the WRITE lock (status bit 3), reproduced verbatim.
    ExtrapolatedRouteRequestQueue* InputBuffer::GetExtrapolatedRouteRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mExtrapolatedRouteRequestQueue;
    }
}
}
