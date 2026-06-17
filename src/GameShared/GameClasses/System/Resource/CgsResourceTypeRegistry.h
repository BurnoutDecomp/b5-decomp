#pragma once

#include "types.hpp"

namespace CgsResource
{
    class Type;

    // The resource-type registry: a small id -> Type* table the bundle loader resolves a
    // resource's handler through. Each Type handler registers itself (keyed on its own
    // GetTypeID()); BundleLoader::LoadBundle takes ResolveResourceType as its FTypeResolver.
    //
    // The X360 resolves the per-resource Type by its bundle muResourceTypeId during the batch
    // create path; this is the PC equivalent of that lookup (a flat id-indexed table, since
    // resource-type ids are small and dense). Handlers are registered as each is brought up;
    // an unregistered id resolves to null (the loader then creates + copies but skips fixup).
    namespace TypeRegistry
    {
        enum { KU_MAX_RESOURCE_TYPES = 256 };

        void        Register(const Type* lpType);          // by lpType->GetTypeID()
        const Type* GetType(u32 luResourceTypeId);         // null if unregistered / out of range
        void        Clear();
    }

    // Default FTypeResolver for BundleLoader::LoadBundle.
    const Type* ResolveResourceType(u32 luResourceTypeId);
}
