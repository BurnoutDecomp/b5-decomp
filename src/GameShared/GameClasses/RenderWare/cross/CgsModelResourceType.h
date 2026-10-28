#ifndef CGS_MODEL_RESOURCE_TYPE_H
#define CGS_MODEL_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised model. Derives from CgsResource::Type; the
// listed methods are virtual overrides. Base/signatures recovered from the DecFIGS
// DWARF (CgsModelResourceType.h), which declares exactly:
//   GetTypeID / GetSerialisedResourceDescriptor / DeSerialise / FixDown / FixUp /
//   PostFixUp / GetImportCount / GetImportPointer / GetDebugResourceCategory.
class ModelResourceType : public Type
{
public:
    uint32_t           GetTypeID() const ;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const ;   // @0x828A8578
    void               PostFixUp(void* lpResource, const rw::Resource& lrResource) const ;   // @0x828A7A68
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    uint32_t           GetImportCount(const void* lpResource) const ;
    void               GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const ;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
};
}

#endif
