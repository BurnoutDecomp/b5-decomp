#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"     // ID (by value)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include <cstddef>

// CgsResource::Entry - one resource record inside a Pool: the resource's id, its
// in-memory descriptor (size/alignment per memory pool), its type handler, the byte
// offset of its import table within the pool's import buffer, the resource memory (one
// base pointer per memory pool), the per-pool heap node indices, and a debug-info pointer.
//
// CgsResource::BasePool is the data-less base shared by Pool and ScratchPool. All of the
// pool state (the three Heaps, the entry/status/refcount arrays, the hash table, the
// defrag state machine, ...) lives in the derived Pool; BasePool contributes only the
// KI_NUM_TYPES constant (and the resource memory-pool count it implies).
//
// DecFIGS CgsResourceBasePool.h declares the owning BaseResourcePtr as Entry::mPtr.
// Pool initializes that resource pointer in the allocated Entry array.

namespace CgsResource
{
    class  Type;               // resource-type handler (CgsResourceType.h) - by pointer
    struct BaseResourcePtr;    // intrusive resource-pointer ring node
    struct DebugResourceInfo;  // per-resource debug record - by pointer (debug builds)

    // CgsResourceBasePool.h:52
    struct Entry
    {
        typedef SmallResourceDescriptor ResourceDescriptor;  // the 3-pool in-memory form

        ID                 mID;                  // +0   identity (64-bit hash)
        ResourceDescriptor mResourceDescriptor;  // +8   size/alignment per memory pool (24B)
        const Type*        mpResourceType;       // +32  type handler (fixup/import virtuals)
        u32                muImportTableOffset;  // +36  byte offset into the pool import buffer
        BaseResourcePtr    mPtr;
        u16                mauHeapIndices[3];    // +72  heap node index per memory pool
        DebugResourceInfo* mpDebugInfo;          // +80  debug resource info (zeroed on create)

        SmallResource& GetResourceMemory() { return mPtr.mResourceMemory; }
        const SmallResource& GetResourceMemory() const { return mPtr.mResourceMemory; }
    };

    CGS_STATIC_ASSERT(sizeof(Entry) == 0x58, "ARTIST resource entry stride");
    CGS_STATIC_ASSERT(offsetof(Entry, mPtr) == 0x28, "ARTIST entry resource pointer");
    CGS_STATIC_ASSERT(offsetof(Entry, mauHeapIndices) == 0x48, "ARTIST entry heap nodes");
    CGS_STATIC_ASSERT(offsetof(Entry, mpDebugInfo) == 0x50, "ARTIST entry debug info");

    // CgsResourceBasePool.h:90 - data-less base of Pool / ScratchPool.
    struct BasePool
    {
        // :94 - resource memory-pool count (main / graphics-system / graphics-local).
        static const u32 KI_NUM_TYPES = 3;
    };
}
