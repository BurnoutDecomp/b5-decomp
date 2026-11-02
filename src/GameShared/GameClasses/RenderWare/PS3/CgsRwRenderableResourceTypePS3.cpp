#include "GameShared/GameClasses/RenderWare/cross/CgsRwRenderableResourceType.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "rw/rwcore_structs.h"

CGS_STATIC_ASSERT((offsetof(RenderableMesh, mu8NumVertexBuffers) == 0x26), "ARTIST mesh vertex count");
CGS_STATIC_ASSERT((offsetof(RenderableMesh, maBuffers) == 0x28), "ARTIST mesh buffer table");
CGS_STATIC_ASSERT((sizeof(rw::Resource) == 20), "ARTIST five resource lanes");

// ARTIST 828A8968; DecFIGS CgsRwRenderableResourceType.h declares the mesh
// and the original five-lane resource, which the caller forwards unchanged.
namespace CgsResource
{
    void RwRenderableResourceType::FixUpRenderableMesh(RenderableMesh* lpMesh,
                                                       const rw::Resource& lrResource) const
    {
        const uintptr_t luPointerBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);
        const u32 luBufferOffset = reinterpret_cast<u32>(lrResource.m_baseResources[2]);

        for (u32 luI = 0; luI < lpMesh->mu8NumVertexBuffers; ++luI)
        {
            const uintptr_t luAddress = reinterpret_cast<uintptr_t>(lpMesh->maBuffers[1 + luI]) + luPointerBase;
            lpMesh->maBuffers[1 + luI] = reinterpret_cast<void*>(luAddress);
        }

        const uintptr_t luIndexAddress = reinterpret_cast<uintptr_t>(lpMesh->maBuffers[0]) + luPointerBase;
        lpMesh->maBuffers[0] = reinterpret_cast<void*>(luIndexAddress);
        renderengine::IndexBuffer* lpIndexBuffer =
            static_cast<renderengine::IndexBuffer*>(lpMesh->maBuffers[0]);
        lpIndexBuffer->Address += luBufferOffset;
        lpIndexBuffer->Xbox2CheckPhysicalMemoryFlags();

        for (u32 luI = 0; luI < lpMesh->mu8NumVertexBuffers; ++luI)
        {
            renderengine::VertexBuffer* lpVertexBuffer =
                static_cast<renderengine::VertexBuffer*>(lpMesh->maBuffers[1 + luI]);
            const u32 luOld = lpVertexBuffer->Format.dword[0];
            lpVertexBuffer->Format.dword[0] =
                (((luOld & ~3u) + luBufferOffset) & ~3u) | (luOld & 3u);
            lpVertexBuffer->Xbox2CheckPhysicalMemoryFlags();
        }
    }
}
