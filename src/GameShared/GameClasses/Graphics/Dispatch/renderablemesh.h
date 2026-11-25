#pragma once

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"  // CgsGraphics::PackedOobb (16B)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor

// renderablemesh.h -- the global ::RenderableMesh dispatch primitive.
//
// DecFIGS renderablemesh.h:78–151 defines the serialized mesh and its accessors.
// ARTIST SetMeshBuffers at 827E68A0 reads the index buffer in the first slot of
// maBuffers followed by the native vertex buffers.
//
// X360 member layout confirmed against the Renderable::UsesTexture asm (members accessed BY
// NAME; the PC host pointer width differs from the 32-bit X360 target):
//   +0x00 mPackedBoundingBox      PackedOobb                       (16B)
//   +0x10 mDrawIndexedParameters  DrawIndexedParameters            (16B)
//   +0x20 mpMaterialAssembly      CgsGraphics::MaterialAssembly*   (lwz 0x20(mesh))

namespace CgsGraphics { struct MaterialAssembly; }
namespace renderengine { class IndexBuffer; class VertexBuffer; }

// Existing global spelling names the single genuine native SDK type.
typedef renderengine::DrawIndexedParameters DrawIndexedParameters;

struct RenderableMesh
{
    CgsGraphics::PackedOobb        mPackedBoundingBox;      // +0x00
    DrawIndexedParameters          mDrawIndexedParameters;  // +0x10
    CgsGraphics::MaterialAssembly* mpMaterialAssembly;      // +0x20
    u8                             mu8NumVertexDescriptors;
    u8                             mu8InstanceCount;
    u8                             mu8NumVertexBuffers;
    u8                             mu8Flags;
    void*                          maBuffers[1];            // trailing variable-length buffer table

    u32 GetNumVertexBuffers() const { return mu8NumVertexBuffers; }
    renderengine::IndexBuffer* GetIndexBuffer() const
    {
        return static_cast<renderengine::IndexBuffer*>(maBuffers[0]);
    }
    renderengine::VertexBuffer* GetVertexBuffer(u32 luIndex) const
    {
        return static_cast<renderengine::VertexBuffer*>(maBuffers[luIndex + 1]);
    }

    // DWARF renderablemesh.h:83 -- per-mesh serialised vertex-descriptor block sizer, used by
    // CgsResource::RwRenderableResourceType::GetSerialisedResourceDescriptor (@0x828A9AB0).
    // Static: the caller passes only the two counts, no mesh pointer. Body in renderablemesh.cpp.
    static CgsResource::ResourceDescriptor GetResourceDescriptor(uint32_t luNumVertexBuffers, uint32_t luNumVertexDescriptors);
};
