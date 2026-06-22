#include "pc/gcm/renderengine/TextureResourceHandle.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// 0x82286130 -- CgsResourceHandle<renderengine::Texture>::operator renderengine::Texture*. The
// X360 body loads **this (the resource handle's mpResourceMemory) and asserts it is non-null
// before returning it. The double indirection and the assert site are preserved; CGS_ASSERT
// carries the X360 failure message ("Can not get resource pointer - it has no main memory
// resource") in place of the inline Begin/Fire/End assert stream.

namespace renderengine
{
    TextureResourceHandle::operator Texture*() const
    {
        Texture* lpTexture = *mpHandle;   // result = **a1
        CGS_ASSERT(lpTexture != nullptr,
                   "Can not get resource pointer - it has no main memory resource\n");
        return lpTexture;
    }
}
