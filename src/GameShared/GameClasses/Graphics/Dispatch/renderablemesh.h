#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"  // CgsGraphics::PackedOobb (16B)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor

// renderablemesh.h -- the global ::RenderableMesh dispatch primitive.
//
// Reconstructed from the DecFIGS DWARF (.../Dispatch/renderablemesh.h). Only the leading
// data layout (through mpMaterialAssembly) is load-bearing for the consumers reconstructed
// so far -- Renderable::UsesTexture reads mesh->mpMaterialAssembly. The mesh's own accessor
// methods (GetVertexBuffer / GetIndexBuffer / vertex-descriptor helpers / Initialize /
// Release) are left to renderablemesh.cpp's own TU and intentionally not declared here.
//
// X360 member layout confirmed against the Renderable::UsesTexture asm (members accessed BY
// NAME; the PC host pointer width differs from the 32-bit X360 target):
//   +0x00 mPackedBoundingBox      PackedOobb                       (16B)
//   +0x10 mDrawIndexedParameters  DrawIndexedParameters            (16B)
//   +0x20 mpMaterialAssembly      CgsGraphics::MaterialAssembly*   (lwz 0x20(mesh))

namespace CgsGraphics { struct MaterialAssembly; }

// DWARF renderablemesh.h:141 -- the per-mesh indexed-draw descriptor (4x u32). Defined at
// its point of use until a canonical shared home is established.
struct DrawIndexedParameters
{
    u32 mePrimitiveType;
    u32 muBaseVertexIndex;
    u32 muMinVertexIndex;
    u32 muNumVertices;
};

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

    // DWARF renderablemesh.h:83 -- per-mesh serialised vertex-descriptor block sizer, used by
    // CgsResource::RwRenderableResourceType::GetSerialisedResourceDescriptor (@0x828A9AB0).
    // Distinct from the alignptr.cpp static GetResourceDescriptor. Declared here (wave44
    // prerequisite); its body is homed in its own wave.
    CgsResource::ResourceDescriptor GetResourceDescriptor(uint32_t luNumVertexBuffers, uint32_t luNumVertexDescriptors);
};
