#include "GameShared/GameClasses/RenderWare/cross/CgsRwRenderableResourceType.h"
#include "pc/gcm/renderengine/IndexBuffer.h"   // renderengine::IndexBuffer / IndexBufferHeader
#include "pc/gcm/renderengine/VertexBuffer.h"  // renderengine::VertexBuffer / VertexBufferHeader

// CgsResource::RwRenderableResourceType::FixUpRenderableMesh @ 0x828A8968.
//
// Relocates one just-loaded serialised renderable mesh and re-validates its GPU buffers.
// The mesh carries a u8 vertex-buffer count (+0x26), an index-buffer header pointer (+0x28)
// and an array of vertex-buffer header pointers (+0x2C). The fix-up block supplies two deltas:
// lpFixUp->muPointerBase (a3[0]) relocates the serialised pointers to their loaded base, and
// lpFixUp->muBufferOffset (a3[2]) is folded into each GPU buffer header's base-address word.
//
// Store-for-store from the asm:
//   1) for each vertex-buffer pointer slot: *slot += muPointerBase   (relocate the ptr array)
//   2) mpIndexBuffer += muPointerBase                                (relocate the IB ptr)
//   3) indexBuffer->muBaseAddress += muBufferOffset                  (straight add)
//      then IndexBuffer::Xbox2CheckPhysicalMemoryFlags(indexBuffer)
//   4) for each vertex buffer: muBaseAddress =
//          (((muBaseAddress & ~3u) + muBufferOffset) & ~3u) | (muBaseAddress & 3u)
//      (clrrwi + add + insrwi: relocate the base while preserving the low-2-bit memory flags)
//      then VertexBuffer::Xbox2CheckPhysicalMemoryFlags(vertexBuffer)

namespace CgsResource
{
    void RwRenderableResourceType::FixUpRenderableMesh(RwRenderableMesh* lpMesh,
                                                       const RwRenderableFixUpData* lpFixUp) const
    {
        const u32 luPointerBase  = lpFixUp->muPointerBase;   // a3[0]
        const u32 luBufferOffset = lpFixUp->muBufferOffset;  // a3[2]

        // 1) Relocate each serialised vertex-buffer-header pointer to its loaded base.
        for (u32 luI = 0; luI < lpMesh->muNumVertexBuffers; ++luI)
        {
            // Pointer arithmetic on the stored 32-bit address: add the relocation base.
            uintptr_t luAddr = reinterpret_cast<uintptr_t>(lpMesh->mapVertexBuffers[luI]) + luPointerBase;
            lpMesh->mapVertexBuffers[luI] = reinterpret_cast<renderengine::VertexBufferHeader*>(luAddr);
        }

        // 2) Relocate the index-buffer-header pointer the same way.
        {
            uintptr_t luAddr = reinterpret_cast<uintptr_t>(lpMesh->mpIndexBuffer) + luPointerBase;
            lpMesh->mpIndexBuffer = reinterpret_cast<renderengine::IndexBufferHeader*>(luAddr);
        }

        // 3) Fold the buffer offset into the index buffer's GPU base-address word (straight add),
        //    then re-check its physical-memory flags.
        renderengine::IndexBufferHeader* lpIndexBuffer = lpMesh->mpIndexBuffer;
        lpIndexBuffer->muBaseAddress += luBufferOffset;
        renderengine::IndexBuffer::Xbox2CheckPhysicalMemoryFlags(reinterpret_cast<u32*>(lpIndexBuffer));

        // 4) Same fold for each vertex buffer, but preserving the low-2-bit memory flags
        //    across the relocation, then re-check the flags.
        for (u32 luI = 0; luI < lpMesh->muNumVertexBuffers; ++luI)
        {
            renderengine::VertexBufferHeader* lpVertexBuffer = lpMesh->mapVertexBuffers[luI];
            const u32 luOld = lpVertexBuffer->muBaseAddress;
            lpVertexBuffer->muBaseAddress =
                (((luOld & ~3u) + luBufferOffset) & ~3u) | (luOld & 3u);
            renderengine::VertexBuffer::Xbox2CheckPhysicalMemoryFlags(reinterpret_cast<u32*>(lpVertexBuffer));
        }
    }
}
