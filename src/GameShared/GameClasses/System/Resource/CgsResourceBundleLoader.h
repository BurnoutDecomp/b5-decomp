#pragma once

#include "types.hpp"

namespace CgsResource
{
    class Pool;
    class Type;

    // Maps a bundle resource-type id (BundleV2::ResourceEntry::muResourceTypeId) to its
    // registered Type handler. Supplied by the caller (the resource-type registry); may be
    // null, in which case resources are created/copied but not fixed up.
    typedef const Type* (*FTypeResolver)(u32 luResourceTypeId);

    // CgsResource::BundleLoader - the PC synchronous bundle loader.
    //
    // The X360 ships an asynchronous streaming BundleLoaderModule (a ~150-member FSM driving
    // the 360 disk stream: StreamHeader -> StreamEntryList -> StreamData -> ...). For the PC
    // build this loads a BundleV2 in one shot: read the file -> validate the header -> create
    // and allocate each resource in a Pool -> copy its data -> fix up its pointers and resolve
    // its imports. It is faithful to the bundle FORMAT (CgsResourceBundle2) and to the
    // per-resource create/fixup/import logic (CgsResource::Pool, decompiled from the X360);
    // only the IO is PC-shaped (a CRT file read replacing the 360 async stream) -- the "PC IO
    // layer" of the resource system.
    //
    // The on-disk data is treated as uncompressed and native-endian here; converting the 360
    // bundle bytes (decompression + big-endian swizzle via BundleV2::EndianSwap) is the data
    // pipeline's job and is handled separately.
    class BundleLoader
    {
    public:
        // Load lpcFileName into lpPool. Returns the number of resources loaded, or -1 on a
        // read / format error. If lpfnResolveType is null (or a resource's type is unknown),
        // that resource is created and its data copied, but it is not fixed up.
        s32 LoadBundle(const char* lpcFileName, Pool* lpPool, FTypeResolver lpfnResolveType);

        // Unload lpcFileName's resources from lpPool: re-read the bundle's resource id list and
        // ref-count-release each (Pool::RemoveReference -> frees memory + slot at refcount 0). Returns the
        // number of resources unloaded, or -1 on a read/format error. [The X360 async unload tracks loaded
        // bundles + drives the DeAllocate state machine; this PC synchronous form re-derives the id list
        // from the file and releases directly.]
        s32 UnloadBundle(const char* lpcFileName, Pool* lpPool);
    };
}
