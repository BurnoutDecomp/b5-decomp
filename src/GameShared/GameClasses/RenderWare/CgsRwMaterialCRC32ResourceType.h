#ifndef CGS_RW_MATERIAL_CRC32_RESOURCE_TYPE_H
#define CGS_RW_MATERIAL_CRC32_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The RwMaterialCRC32 resource-type handler (resource type id 0xB = 11). A MaterialCRC32 is a
    // RenderWare material identified by a CRC32; its serialised descriptor is sized by the engine
    // SDK's MaterialCRC32::GetResourceDescriptor, to which this handler's
    // GetSerialisedResourceDescriptor (X360 0x828A88B0) is a thin forwarder. GetTypeID lives in the
    // sibling PS3 home (CgsRwMaterialCRC32ResourceTypePS3.cpp, "return 11");
    // GetSerialisedResourceDescriptor is owned here.
    class RwMaterialCRC32ResourceType : public Type
    {
    public:
        virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
    };
}

#endif // CGS_RW_MATERIAL_CRC32_RESOURCE_TYPE_H
