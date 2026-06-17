#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // Type::GetTypeID

// The PC resource-type registry: a flat id-indexed table of Type handlers, populated as each
// handler is brought up and read by the bundle loader's FTypeResolver. See the header for how
// this relates to the X360 per-resource type resolution in the batch-create path.

namespace CgsResource
{
    namespace TypeRegistry
    {
        static const Type* sapResourceTypes[KU_MAX_RESOURCE_TYPES] = { 0 };

        void Register(const Type* lpType)
        {
            if (lpType == 0)
                return;
            const u32 luId = lpType->GetTypeID();
            if (luId < KU_MAX_RESOURCE_TYPES)
                sapResourceTypes[luId] = lpType;
        }

        const Type* GetType(u32 luResourceTypeId)
        {
            return luResourceTypeId < KU_MAX_RESOURCE_TYPES ? sapResourceTypes[luResourceTypeId] : 0;
        }

        void Clear()
        {
            for (u32 luId = 0; luId < KU_MAX_RESOURCE_TYPES; ++luId)
                sapResourceTypes[luId] = 0;
        }
    }

    const Type* ResolveResourceType(u32 luResourceTypeId)
    {
        return TypeRegistry::GetType(luResourceTypeId);
    }
}
