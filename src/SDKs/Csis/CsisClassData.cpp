#include "SDKs/Csis/CsisClassData.h"

#include <stdint.h> // intptr_t

// ===========================================================================
// SDKs/Csis/CsisClassData.cpp
//
// Csis::ClassData::SendParameters @ 0x82B0F160, reconstructed store-for-store from
// the X360 .XEX (BURNOUT_X360_ARTIST.XEX).
//
// asm:
//   r31 = mUpdateClients head   ; lwz r31, 8(r3)
//   r30 = pParameter            ; mr  r30, r4
//   while (r31 != 0):
//     r4 = node->mpContext      ; lwz r4, 0xC(r31)
//     r3 = pParameter           ; mr  r3, r30
//     r11 = node->mpfnNotify    ; lwz r11, 8(r31)
//     call r11(r3, r4)          ; bctrl     -> result lands in r3
//     r31 = node->mpNext        ; lwz r31, 0(r31)
//   return r3
//
// The X360 leaves the last callback's return value in r3 (or, for an empty list,
// the untouched incoming r3 == this); SendParameters is a notification broadcast,
// so that return is incidental. It is preserved here for faithfulness.
// ===========================================================================

namespace Csis
{

int ClassData::SendParameters(Parameter* pParameter)
{
    // The X360 returns the incoming r3 (== this) unchanged when the list is empty;
    // each callback then overwrites it with its own result.
    int liResult = static_cast<int>(reinterpret_cast<intptr_t>(this));

    for (ClassDataClient* lpClient = mUpdateClients.mpHead;
         lpClient != 0;
         lpClient = lpClient->mpNext)
    {
        liResult = lpClient->mpfnNotify(pParameter, lpClient->mpContext);
    }

    return liResult;
}

} // namespace Csis
