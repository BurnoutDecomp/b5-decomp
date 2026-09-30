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
        // Load lpcFileName into lpPool. Returns the number of newly created member resources,
        // or a negative error; allocation failure rolls back this load's references. If
        // lpfnResolveType is null (or a resource's type is unknown),
        // that resource is created and its data copied, but it is not fixed up.
        // Stored-id form: the RAW on-disc 64-bit entry id, UNTAGGED -- the X360 store path
        // (AllocatePoolModuleState::CreateResourceList 0x828FF480) registers `*(u64*)entry`
        // verbatim, and every game-side acquire emits the raw zero-extended HashString
        // return (the pool rides the acquire event's miPoolId field, not the id's high
        // dword -- DoAcquireResourceRequest 0x828FCD48 `lwz r4,8(r31)`).
        // [FLAG PC bring-up] LoadBundle's negative returns. The X360 loader has one failure
        // path; this port needs to tell "the converted file is not in build/game at all" apart
        // from "it is there and this loader refuses it", because only the second is a defect.
        // DELETE-WHEN every bundle the game asks for is ported.
        enum { KI_LOAD_FAILED = -1, KI_LOAD_FILE_MISSING = -2 };

        s32 LoadBundle(const char* lpcFileName, Pool* lpPool, FTypeResolver lpfnResolveType);

        // Release one low-level load using the pool-resident entry list, then release the list.
        // Returns the number of member resources freed (excluding the list), or -1 if no valid
        // resident list exists. Shared resources survive until their last reference is released.
        s32 UnloadBundle(const char* lpcFileName, Pool* lpPool);
    };
}
