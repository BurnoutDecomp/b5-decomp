#ifndef CGS_KD_TREE_RESOURCE_TYPE_H
#define CGS_KD_TREE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised rw::collision::TriangleKDTreeProcedural.
// Derives from CgsResource::Type; the listed methods are virtual overrides
// (GetImportPointer asserts — KD-trees have no import pointers). Base/signatures
// recovered from the DecFIGS DWARF (CgsKdTreeResourceType.h).
class KdTreeResourceType : public Type
{
public:
    uint32_t           GetTypeID() const ;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    void               GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const ;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
};
}

#endif
