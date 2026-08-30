#ifndef CGS_ALIGNED_BINARY_FILE_RESOURCE_H
#define CGS_ALIGNED_BINARY_FILE_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
    // The aligned resource record has the same serialized header/accessor surface
    // as BinaryFileResource; only its resource-type allocator alignment differs.
    struct AlignedBinaryFileResource : public BinaryFileResource
    {
    };

    // Binary-file resource whose payload is alignment-sensitive. Adds no new
    // virtuals over BinaryFileResourceType. Recovered from the DecFIGS DWARF
    // (CgsAlignedBinaryFileResource.h).
    class AlignedBinaryFileResourceType : public BinaryFileResourceType
    {
    public:
        AlignedBinaryFileResourceType();
    };
}

#endif
