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

        // lpcName is the console's parallel name column (RegisterResourceTypes keeps a
        // handler table and a name table side by side); it is debug-only. Register runs
        // Type::InitCachedValues on the handler first (the X360 does the same after each
        // handler's construction), so the parameter is non-const.
        void        Register(Type* lpType, const char* lpcName = 0);   // by the cached type id
        const Type* GetType(u32 luResourceTypeId);         // null if unregistered
        void        Clear();

        // Enumerate the shared handler catalog in registration order. The native
        // GameDataModule owns its handler/name arrays and supplies the game-specific
        // entries through PoolModule::InitOptions for asynchronous resource requests.
        u32         GetCount();
        const Type* GetByIndex(u32 luIndex);               // null when out of range
        const char* GetNameByIndex(u32 luIndex);           // "" when unnamed/out of range
    }

    // Shared catalog lookup by resource type id.
    const Type* ResolveResourceType(u32 luResourceTypeId);
}
