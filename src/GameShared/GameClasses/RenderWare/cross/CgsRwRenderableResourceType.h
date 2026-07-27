#ifndef CGS_RW_RENDERABLE_RESOURCE_TYPE_H
#define CGS_RW_RENDERABLE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace renderengine { struct VertexBufferHeader; struct IndexBufferHeader; }
namespace CgsGraphics   { struct MaterialAssembly; }

// Forward declarations for the two serialised-descriptor bodies added in wave44
// (GetRenderableBasicResourceDescriptor / GetSerialisedResourceDescriptor). The
// concrete layouts live in Renderable.h / renderablemesh.h; the .cpp includes those.
struct Renderable;
struct RenderableMesh;

namespace CgsResource
{
    // The fix-up relocation block FixUp/FixUpRenderableMesh are handed. On the X360 this IS the
    // rw::Resource view Pool::FixUpEntry (@0x828EB860) builds on its stack -- five words seeded
    // { small[0] (main memory), 0, small[1] (graphics memory), 0, 0 } -- and the two bodies read
    // it as `*a3` and `a3[2]`. Named here so the two deltas are used by role:
    //   muPointerBase  == rw::Resource::m_baseResources[0], added to every serialised pointer
    //                     slot inside the resource. WIDENED to host width: the converted
    //                     platform-4 blob carries 8-byte pointer slots (the X360 delta is 32-bit
    //                     only because its pointers are).
    //   muBufferOffset == rw::Resource::m_baseResources[2] (= the resource's GRAPHICS block,
    //                     bundle memory type 1 -- SmallResource::ConvertToRWResource maps
    //                     small[1] -> rw[2]), folded into each GPU buffer header's base-address
    //                     word. Stays 32-bit: muBaseAddress is a console u32 slot, resolved
    //                     through the project's low-4 GB reservation.
    struct RwRenderableFixUpData
    {
        uintptr_t muPointerBase;   // a3[0]  serialised-pointer relocation base (host width)
        u32       muBufferOffset;  // a3[2]  GPU buffer-block base (console u32 slot)
    };

    // One serialised renderable mesh the PS3/Xbox2 fix-up walks.
    // SEAM (platform-4 widened data): the converted x64 blob carries the NATURAL widening of
    // the real ::RenderableMesh (GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h --
    // the type's primary committed consumer); this struct mirrors it member-for-member so the
    // fix-up walks the same layout the data now has. X360 cross-reference (32-bit ptrs,
    // FixUpRenderableMesh asm @0x828A8968): MaterialAssembly* u32 @+0x20, the four u8s
    // @+0x24..+0x27 (vertex-buffer loop count = lbz 0x26), buffer table @+0x28 (IB header ptr
    // @+0x28, VB header ptrs @+0x2C..). On x64 the pointer slots widen to u64: u8s land at
    // +0x28..+0x2B and the buffer table starts at +0x30.
    struct RwRenderableMesh
    {
        u8                             maPackedBoundingBox[0x10];     // +0x00  PackedOobb (16B, opaque here)
        u8                             maDrawIndexedParameters[0x10]; // +0x10  DrawIndexedParameters (4x u32, opaque here)
        CgsGraphics::MaterialAssembly* mpMaterialAssembly;            // +0x20  (X360 +0x20, u32 slot)
        u8                             muNumVertexDescriptors;        // +0x28  (X360 +0x24)
        u8                             muInstanceCount;               // +0x29  (X360 +0x25)
        u8                             muNumVertexBuffers;            // +0x2A  (X360 +0x26 -- the lbz 0x26 loop count)
        u8                             muFlags;                       // +0x2B  (X360 +0x27)
        // 4 bytes implicit pad -> the pointer table is 8-aligned
        void*                          mapBuffers[1];                 // +0x30  (X360 +0x28): [0] = IndexBufferHeader*,
                                                                      //         [1..numVB] = VertexBufferHeader*s,
                                                                      //         then vertex-descriptor import slots
    };

// Serialised RenderWare renderable handler. Derives from CgsResource::Type; the
// listed virtuals are overrides. GetTypeID returns the registry id (12); FixDown and
// GetImportPointer assert (renderables are not fixed down / have no import pointers at
// runtime); DebugValidate walks the serialised renderable's per-mesh material
// assemblies. The four bodies are defined in CgsRwRenderableResourceType.cpp; the rest
// (DeSerialise/FixUp/ReBase/...) are inherited from the non-pure base. Base/signatures
// recovered from the DecFIGS DWARF (CgsRwRenderableResourceType.h).
class RwRenderableResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;

    // @0x828A9498 -- override (X360 vtable slot #4). Relocates the whole serialised renderable
    // graph to its loaded base: the mesh pointer table, the object-scope texture-info block
    // (its three interior tables + every texture-name string pointer), then each mesh pointer,
    // handing every mesh to FixUpRenderableMesh. Ends by setting the "fixed up" bit (8) in
    // mu16Flags. Without this override nothing in a streamed renderable is walkable.
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;

    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    bool     DebugValidate(const void* lpResource) const override;

    // @0x828A9AB0 -- override (X360 vtable slot #2). Total serialised allocation for one
    // renderable = fixed per-renderable overhead + per-mesh vertex-descriptor / index /
    // vertex-buffer descriptors. Defined in CgsRwRenderableResourceType.cpp (wave44).
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;

    // PS3/Xbox2 fix-up helper (defined in CgsRwRenderableResourceTypePS3.cpp, @0x828A8968):
    // relocate one serialised mesh's buffer-header pointers + GPU base addresses, then
    // re-validate each buffer's physical-memory flags. Called per-mesh by FixUp.
    void     FixUpRenderableMesh(RwRenderableMesh* lpMesh, const RwRenderableFixUpData* lpFixUp) const;

private:
    // @0x828A96F0 -- fixed per-renderable overhead descriptor (mesh table + object-scope
    // texture-info block + texture-name string pool). DWARF CgsRwRenderableResourceType.h:47.
    CgsResource::ResourceDescriptor GetRenderableBasicResourceDescriptor(const Renderable* lpRenderable) const;
};
}

#endif
