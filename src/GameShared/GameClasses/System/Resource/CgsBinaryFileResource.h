#ifndef CGS_BINARY_FILE_RESOURCE_H
#define CGS_BINARY_FILE_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // Resource-type base for plain binary-file resources (the common case). It
    // overrides the relocation + serialisation virtuals; concrete handlers that
    // derive from it usually only add their GetTypeID. Recovered from the DecFIGS
    // DWARF (CgsBinaryFileResource.h).
    class BinaryFileResourceType : public Type
    {
    public:
        BinaryFileResourceType();

        uint32_t           GetTypeID() const override;
        ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
        void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
        void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    };
}

#endif
