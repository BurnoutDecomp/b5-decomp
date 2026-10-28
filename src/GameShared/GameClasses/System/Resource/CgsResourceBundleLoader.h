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
    // Its current synchronous scheduling still differs from the original asynchronous FSM.
    // File reads use DeviceManager, and each load retains the original type-29 entry list in
    // the destination pool so unloading needs no file access.
    //
    // The on-disk data is treated as uncompressed and native-endian here; converting the 360
    // bundle bytes (decompression + big-endian swizzle via BundleV2::EndianSwap) is the data
    // pipeline's job and is handled separately.
    class BundleLoader
    {
    public:

        s32 LoadBundle(const char* lpcFileName, Pool* lpPool, FTypeResolver lpfnResolveType);

        // Release one low-level load using the pool-resident entry list, then release the list.
        // Returns the number of member resources freed (excluding the list), or -1 if no valid
        // resident list exists. Shared resources survive until their last reference is released.
        s32 UnloadBundle(const char* lpcFileName, Pool* lpPool);
    };
}
