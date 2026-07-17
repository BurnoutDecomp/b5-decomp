#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // Type::GetTypeID

// The PC resource-type registry: a compact sparse table, matching the X360 module's
// sequential handler/id records while admitting high vendor ids such as 0x10020.

namespace CgsResource
{
    namespace TypeRegistry
    {
        struct ResourceTypeEntry
        {
            u32         muId;
            const Type* mpType;
        };

        static ResourceTypeEntry saResourceTypes[KU_MAX_REGISTERED_RESOURCE_TYPES] = {};
        static u32 suNumResourceTypes = 0;

        void Register(const Type* lpType)
        {
            if (lpType == 0)
                return;
            const u32 luId = lpType->GetTypeID();
            for (u32 lu = 0; lu < suNumResourceTypes; ++lu)
            {
                if (saResourceTypes[lu].muId == luId)
                {
                    saResourceTypes[lu].mpType = lpType;
                    return;
                }
            }

            if (suNumResourceTypes < KU_MAX_REGISTERED_RESOURCE_TYPES)
            {
                saResourceTypes[suNumResourceTypes].muId   = luId;
                saResourceTypes[suNumResourceTypes].mpType = lpType;
                ++suNumResourceTypes;
            }
        }

        const Type* GetType(u32 luResourceTypeId)
        {
            for (u32 lu = 0; lu < suNumResourceTypes; ++lu)
            {
                if (saResourceTypes[lu].muId == luResourceTypeId)
                    return saResourceTypes[lu].mpType;
            }
            return 0;
        }

        void Clear()
        {
            for (u32 lu = 0; lu < suNumResourceTypes; ++lu)
            {
                saResourceTypes[lu].muId   = 0;
                saResourceTypes[lu].mpType = 0;
            }
            suNumResourceTypes = 0;
        }
    }

    const Type* ResolveResourceType(u32 luResourceTypeId)
    {
        return TypeRegistry::GetType(luResourceTypeId);
    }
}
