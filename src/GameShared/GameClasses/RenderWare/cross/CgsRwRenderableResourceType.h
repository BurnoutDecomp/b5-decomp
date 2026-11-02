#ifndef CGS_RW_RENDERABLE_RESOURCE_TYPE_H
#define CGS_RW_RENDERABLE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsGraphics   { struct MaterialAssembly; }

// Forward declarations for the two serialised-descriptor bodies added in wave44
// (GetRenderableBasicResourceDescriptor / GetSerialisedResourceDescriptor). The
// concrete layouts live in Renderable.h / renderablemesh.h; the .cpp includes those.
struct Renderable;
struct RenderableMesh;

namespace CgsResource
{
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
    uint32_t GetTypeID() const ;

    // @0x828A9498 -- override (X360 vtable slot #4). Relocates the whole serialised renderable
    // graph to its loaded base: the mesh pointer table, the object-scope texture-info block
    // (its three interior tables + every texture-name string pointer), then each mesh pointer,
    // handing every mesh to FixUpRenderableMesh. Ends by setting the "fixed up" bit (8) in
    // mu16Flags. Without this override nothing in a streamed renderable is walkable.
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const ;

    void     FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const ;
    bool     DebugValidate(const void* lpResource) const ;

    // @0x828A9AB0 -- override (X360 vtable slot #2). Total serialised allocation for one
    // renderable = fixed per-renderable overhead + per-mesh vertex-descriptor / index /
    // vertex-buffer descriptors. Defined in CgsRwRenderableResourceType.cpp (wave44).
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;

    // PS3/Xbox2 fix-up helper (defined in CgsRwRenderableResourceTypePS3.cpp, @0x828A8968):
    // relocate one serialised mesh's buffer-header pointers + GPU base addresses, then
    // re-validate each buffer's physical-memory flags. Called per-mesh by FixUp.
    void     FixUpRenderableMesh(RenderableMesh* lpMesh, const rw::Resource& lrResource) const;

private:
    // @0x828A96F0 -- fixed per-renderable overhead descriptor (mesh table + object-scope
    // texture-info block + texture-name string pool). DWARF CgsRwRenderableResourceType.h:47.
    CgsResource::ResourceDescriptor GetRenderableBasicResourceDescriptor(const Renderable* lpRenderable) const;
};
}

#endif
