#pragma once

#include "types.hpp"

namespace CgsResource
{
    class Type;

    // The resource-type registry: a small sparse id -> Type* table. Game/vendor ids
    // are not all dense bytes (FLApt is 0x10020), so indexing a 256-entry array by
    // id silently discarded valid handlers.
    namespace TypeRegistry
    {
        enum { KU_MAX_REGISTERED_RESOURCE_TYPES = 256 };

        void        Register(const Type* lpType);          // by lpType->GetTypeID()
        const Type* GetType(u32 luResourceTypeId);         // null if unregistered
        void        Clear();
    }

    // Default FTypeResolver for BundleLoader::LoadBundle.
    const Type* ResolveResourceType(u32 luResourceTypeId);
}
