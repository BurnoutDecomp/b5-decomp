#include "GameSource/Effects/Particles/EffectsVertexBufferManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// EffectsVertexBufferManager::GetVertexBuffer  X360 0x82278320
// Asserts the manager is not locked for write, then returns the vertex buffer
// for the current double-buffer slot: mapVertexBuffer[muCurrentBuffer].
// Asm: lbz r11,0x15 (mbLocked) -> assert; lbz r11,0x14 (muCurrentBuffer);
//      rotlwi r11,r11,2 (index*4); lwzx r3,r11,r31 -> mapVertexBuffer[index].
renderengine::VertexBuffer* EffectsVertexBufferManager::GetVertexBuffer() const
{
    CGS_ASSERT(!mbLocked, "!mbLocked");
    return mapVertexBuffer[muCurrentBuffer];
}
