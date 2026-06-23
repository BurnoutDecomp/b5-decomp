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
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{

bool AbstractPoolVoidHandle::Prepare(IAbstractPoolFreeObject* lpFreeObjectInterface,
                                     void* lpObject, s32 liIndex, s32 liSize)
{
    CGS_ASSERT( lpObject != 0,              "lpObject != NULL" );
    CGS_ASSERT( lpFreeObjectInterface != 0, "lpFreeObjectInterface != NULL" );
    CGS_ASSERT( liIndex >= 0,               "liIndex >= 0" );
    CGS_ASSERT( liSize >= 0,                "liSize >= 0" );

    mpObject              = lpObject;
    mpFreeObjectInterface = lpFreeObjectInterface;
    miIndex               = liIndex;
    miSize                = liSize;
    return true;
}

} // namespace BrnDirector
