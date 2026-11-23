// BrnAbstractPool.cpp
// Definition home for BrnDirector::AbstractPoolVoidHandle::Prepare (X360 @0x821F76C0).
//
// X360 store-for-store: with the DWARF signature Prepare(lpFreeObjectInterface, lpObject,
// liIndex, liSize) the binary writes
//     +0x00 = lpObject              (r5)
//     +0x04 = lpFreeObjectInterface (r4)
//     +0x08 = liIndex               (r6)
//     +0x0C = liSize                (r7)
// after four non-fatal guards (lpObject != NULL; lpFreeObjectInterface != NULL;
// liIndex >= 0; liSize >= 0) and returns 1. The X360-baked assert file/line are dropped
// per project convention; the stringized conditions match the X360 message text.

#include "GameSource/Director/Utils/BrnAbstractPool.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

namespace BrnDirector
{

bool AbstractPoolVoidHandle::Prepare(IAbstractPoolFreeObject* lpFreeObjectInterface,
                                     void* lpObject, s32 liIndex, s32 liSize)
{
    CGS_ASSERT( lpObject != 0,
        CgsDev::Assert::MessageAt("lpObject != NULL", "..\\..\\..\\GameSource\\Director/Utils/BrnAbstractPool.cpp", 33 /* ARTIST 0x821F76F8 */));
    CGS_ASSERT( lpFreeObjectInterface != 0,
        CgsDev::Assert::MessageAt(
            "lpFreeObjectInterface != NULL", "..\\..\\..\\GameSource\\Director/Utils/BrnAbstractPool.cpp", 34 /* ARTIST 0x821F771C */));
    CGS_ASSERT( liIndex >= 0,
        CgsDev::Assert::MessageAt("liIndex >= 0", "..\\..\\..\\GameSource\\Director/Utils/BrnAbstractPool.cpp", 35 /* ARTIST 0x821F7740 */));
    CGS_ASSERT( liSize >= 0,
        CgsDev::Assert::MessageAt("liSize >= 0", "..\\..\\..\\GameSource\\Director/Utils/BrnAbstractPool.cpp", 36 /* ARTIST 0x821F7764 */));

    mpObject              = lpObject;
    mpFreeObjectInterface = lpFreeObjectInterface;
    miIndex               = liIndex;
    miSize                = liSize;
    return true;
}

} // namespace BrnDirector
