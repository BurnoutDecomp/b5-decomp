#ifndef CGS_RW_RENDERABLE_RESOURCE_TYPE_H
#define CGS_RW_RENDERABLE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace renderengine { struct VertexBufferHeader; struct IndexBufferHeader; }

// Forward declarations for the two serialised-descriptor bodies added in wave44
// (GetRenderableBasicResourceDescriptor / GetSerialisedResourceDescriptor). The
// concrete layouts live in Renderable.h / renderablemesh.h; the .cpp includes those.
struct Renderable;
struct RenderableMesh;

namespace CgsResource
{
    // The fix-up relocation block FixUp/FixUpRenderableMesh are handed (a3): two relocation
    // deltas read by index -- [0] relocates serialised pointers to their loaded base,
    // [2] is added to each GPU buffer header's base-address word. Modelled by name.
    struct RwRenderableFixUpData
    {
        u32 muPointerBase;   // +0  (a3[0]) added to serialised buffer-header pointers
        u32 muField04;       // +4  (a3[1])
        u32 muBufferOffset;  // +8  (a3[2]) added to each buffer header's base-address word
    };

    // One serialised renderable mesh the PS3/Xbox2 fix-up walks. X360 offsets (32-bit ptrs):
    // a u8 vertex-buffer count @+0x26, the index-buffer header pointer @+0x28, and the
    // vertex-buffer header pointer array @+0x2C (FixUpRenderableMesh asm @0x828A8968).
    // On the 64-bit host the pointer members widen to 8 bytes, so the pointer offsets drift
    // past the X360 values -- access is by NAME (the fix-up logic is offset-independent), and
    // the count field that governs both loops stays at +0x26.
    struct RwRenderableMesh
    {
        u8                            maHead[0x26];          // +0x00  opaque serialised header
        u8                            muNumVertexBuffers;    // +0x26  loop count (lbz 0x26)
        u8                            maPad27;               // +0x27
        renderengine::IndexBufferHeader* mpIndexBuffer;      // +0x28  index-buffer header ptr
        renderengine::VertexBufferHeader* mapVertexBuffers[1]; // +0x2C  vertex-buffer header ptr array
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
