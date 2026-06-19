#ifndef CGS_RW_RENDERABLE_RESOURCE_TYPE_H
#define CGS_RW_RENDERABLE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

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
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    bool     DebugValidate(const void* lpResource) const override;
};
}

#endif
