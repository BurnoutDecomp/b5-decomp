#ifndef CGS_MATERIAL_TECHNIQUE_RESOURCE_TYPE_H
#define CGS_MATERIAL_TECHNIQUE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised material technique. Derives from
// CgsResource::Type; the listed methods are virtual overrides (GetImportPointer
// asserts — not called at runtime). Base/signatures recovered from the DecFIGS
// DWARF (CgsRwMaterialResourceType.h).
class MaterialTechniqueResourceType : public Type
{
public:
    uint32_t           GetTypeID() const ;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const ;
    void     PostFixUp(void* lpResource, const rw::Resource& lrResource) const ;
};
}

#endif
