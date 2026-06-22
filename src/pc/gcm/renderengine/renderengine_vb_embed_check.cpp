// Tiny embed check: include the renderengine VertexBuffer / facade / texture-handle homes and
// force ODR use of every reconstructed function so the compile gate exercises their signatures
// end to end. Compile-only (cl /c); the platform shims + dictionary find resolve at link.
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/RenderEngine.h"
#include "pc/gcm/renderengine/TextureResourceHandle.h"

namespace renderengine { class Texture; }

void renderengine_vb_embed_check()
{
    using namespace renderengine;

    VertexBufferHeader lHeader{};
    VertexBuffer::Wrapper lWrapper{};
    lWrapper.mpHeader = &lHeader;
    VertexBuffer::Parameters lParams{};
    VertexBuffer::LockInfo lLock{};

    (void)VertexBuffer::Initialize(&lWrapper, &lParams, 0, 0);
    (void)VertexBuffer::Lock(&lHeader, 0, &lLock, 0, 0);
    VertexBuffer::Destruct(&lHeader);
    (void)VertexBuffer::Release(&lHeader);

    u32 luOut[2] = {0u, 0u};
    (void)VertexBuffer::GetParameters(&lHeader, luOut);

    u64 lDesc[5] = {0, 0, 0, 0, 0};
    (void)VertexBuffer::GetResourceDescriptor(lDesc, 0);
    (void)VertexBuffer::Xbox2CheckPhysicalMemoryFlags(reinterpret_cast<u32*>(&lHeader));

    (void)VertexFormatGetStride(1712986);
    (void)TextureS(0, 0);

    TextureResourceHandle lHandle{};
    Texture* lpDummyHandleTarget = nullptr;
    lHandle.mpHandle = &lpDummyHandleTarget;
    Texture* lpTexture = static_cast<Texture*>(lHandle);
    (void)lpTexture;
}
