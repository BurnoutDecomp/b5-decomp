#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"     // ID (by value)
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"  // SmallResourceDescriptor + SmallResource (by value)

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
// Reconstructed from the DecFIGS DWARF (CgsResourceBasePool.h). Entry is a plain record
// the Pool fills field-by-field into a raw allocation -- it is never C++-constructed.

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
        SmallResource      mResource;            // +40  resource memory: one base ptr per memtype
        // The X360 overlays mResource with the first three words of the entry's
        // BaseResourcePtr ring head.  On x64 those three resource pointers occupy
        // 24 bytes; these four fields are the pointer-width continuation of that
        // same owner node.  Keeping them adjacent makes &mResource a valid ring
        // head for BaseResourcePtr::CreateFromHandle without truncating or reading
        // through the following heap-index/debug fields.
        BaseResourcePtr*   mpResourceNext;
        BaseResourcePtr*   mpResourcePrev;
        BaseResourcePtr*   mpResourceThis;
        uintptr_t          muResourceThreadId;
        u16                mauHeapIndices[3];    // +72  heap node index per memory pool
        DebugResourceInfo* mpDebugInfo;          // +80  debug resource info (zeroed on create)
    };

    // CgsResourceBasePool.h:90 - data-less base of Pool / ScratchPool.
    struct BasePool
    {
        // :94 - resource memory-pool count (main / graphics-system / graphics-local).
        static const u32 KI_NUM_TYPES = 3;
    };
}
