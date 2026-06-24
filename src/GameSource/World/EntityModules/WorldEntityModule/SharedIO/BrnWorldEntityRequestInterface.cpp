#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h"

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::RequestInterface members, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The object holds two single-byte request flags:
//   +0 mbInvalidateCollisionWorld   +1 mbValidateCollisionWorld
//
// InvalidateCollisionWorld @ 0x823A7DC8 -- `lbz r11,1(this)` tests the validate flag; on set it
//   streams "Can not invalidate collision world when already validating!\n" and fires
//   FireAssert at BrnWorldEntityRequestInterface.h:85, then `li r11,1; stb r11,0(this)` raises
//   the invalidate flag.
// ValidateCollisionWorld   @ 0x823A7E70 -- `lbz r11,0(this)` tests the invalidate flag; same
//   streamed text (line 92), then `stb r11,1(this)` raises the validate flag.
// Append                   @ 0x8279BA10 -- ORs each flag of lrOther into this (validate first:
//   `this+1 = this+1 || other+1`, then invalidate: `this+0 = this+0 || other+0`); when BOTH end
//   up set it streams "Append resulted in requests to validate and invalidate the collision
//   world\n" and fires at line 114. The order of the two byte stores is faithful to the asm
//   (validate flag stored first, then invalidate), though the result is order-independent.
//
// The streamed-message asserts map to the house CGS_ASSERT (the trailing "\n" is dropped from
// the stringized condition, matching the sibling WorldEntityIO accessors).

namespace BrnWorld
{
namespace WorldEntityIO
{
    void RequestInterface::InvalidateCollisionWorld()
    {
        static_assert(offsetof(RequestInterface, mbInvalidateCollisionWorld) == 0,
                      "mbInvalidateCollisionWorld @0");
        static_assert(offsetof(RequestInterface, mbValidateCollisionWorld) == 1,
                      "mbValidateCollisionWorld @1");

        // X360 0x823A7DC8: cannot invalidate while a validate request is pending.
        CGS_ASSERT(!mbValidateCollisionWorld,
                   "Can not invalidate collision world when already validating!");
        mbInvalidateCollisionWorld = true;
    }

    void RequestInterface::ValidateCollisionWorld()
    {
        // X360 0x823A7E70: cannot validate while an invalidate request is pending. (The X360
        // streams the same message text as InvalidateCollisionWorld; line 92.)
        CGS_ASSERT(!mbInvalidateCollisionWorld,
                   "Can not invalidate collision world when already validating!");
        mbValidateCollisionWorld = true;
    }

    void RequestInterface::Append(const RequestInterface& lrOther)
    {
        // X360 0x8279BA10: merge each pending request (logical OR). The asm stores the validate
        // flag first, then the invalidate flag.
        mbValidateCollisionWorld   = mbValidateCollisionWorld   || lrOther.mbValidateCollisionWorld;
        mbInvalidateCollisionWorld = mbInvalidateCollisionWorld || lrOther.mbInvalidateCollisionWorld;

        // After merging, both requests must not be raised at once.
        CGS_ASSERT(!(mbValidateCollisionWorld && mbInvalidateCollisionWorld),
                   "Append resulted in requests to validate and invalidate the collision world");
    }
}
}
