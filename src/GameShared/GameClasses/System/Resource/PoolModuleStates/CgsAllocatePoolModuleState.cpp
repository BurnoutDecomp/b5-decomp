#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsAllocatePoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"        // Pool / AllocListSet / NewResource / Entry / SmallResource
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::FindResourceType / KI_MAX_ALLOCATION_REQUESTS
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags

#include <cstddef>   // offsetof (layout pin for the resource-memory view)

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsAllocatePoolModuleState.cpp) -- the working steps of the
// PoolModule's "allocate a resource list" state machine:
//   CgsResource::AllocatePoolModuleState::BeginAllocation          @ 0x828DA568
//   CgsResource::AllocatePoolModuleState::CheckListDependencies    @ 0x828FF228
//   CgsResource::AllocatePoolModuleState::CheckEntryListDependency @ 0x828F7AE8
//   CgsResource::AllocatePoolModuleState::CreateResourceList       @ 0x828FF480
//   CgsResource::AllocatePoolModuleState::CreateEntryListResource  @ 0x828F7BA0
//   CgsResource::AllocatePoolModuleState::UndoEntryCreations       @ 0x828F7CB0
// Each is bodied against the X360 asm; the pool/bundle/entry collaborators are reached through their
// owning types' named members and accessors (Pool ref-count / import-count / batch-slot / create /
// alloc-request API, BundleV2::ResourceEntry size/type getters, Entry::mResource), not raw offsets.
// The X360 asserts that stream a formatted message into the debug buffer are expressed through
// CGS_ASSERT (the streamed id, like the file/line, is dropped -- the same convention the sibling
// De-/IntelliFrag states use). Debug prints go through the gpDebugPrint stream, gated on the message
// filter, exactly as the asm does.

namespace CgsResource
{
namespace
{
    // The entry-list resource's in-memory payload. CreateResourceList sizes the pool allocation as
    // alignUp(8 * miNumEntries + 263): a small header, the member count at +0x100, then the packed
    // array of member resource ids at +0x108. This is resource memory -- an allocated data blob whose
    // layout is fixed by the format, not a managed C++ object -- so it is addressed as a single data
    // view (the AGENTS.md serialised/resource-blob exception), not a chain of offset casts.
    struct EntryListResourceData
    {
        u8   muHeaderFlag;      // +0x000  cleared on (re)build
        u8   maPad0[0xFF];      // +0x001
        u32  muNumEntries;      // +0x100
        u8   maPad1[4];         // +0x104
        ID   maEntryIds[1];     // +0x108  (flexible: miNumEntries ids follow)

        static void AssertLayout()
        {
            static_assert(offsetof(EntryListResourceData, muNumEntries) == 0x100, "count @ +0x100");
            static_assert(offsetof(EntryListResourceData, maEntryIds)   == 0x108, "ids @ +0x108");
        }
    };
}

    // -------- BeginAllocation @ 0x828DA568 --------
    // Latch the request working set and move the machine out of idle. The X360 asserts the machine was
    // idle (streamed message) before overwriting the working set; miCountDown is armed to 1 and the two
    // create/purgatory flags are cleared.
    void AllocatePoolModuleState::BeginAllocation(Pool* lpPool, ID lListId,
                                                  const BundleV2::ResourceEntry* lpEntries, s32 liNumEntries,
                                                  AllocListSet* lpAllocListSet, bool* lpOutNeeds,
                                                  void* lpOutResources, bool lbAllowFailiure)
    {
        CGS_ASSERT(meState == E_STATE_IDLE, "Can not begin allocation during none-idle state\n");   // :88

        mpPool         = lpPool;
        mListId        = lListId;
        mpEntries      = lpEntries;
        miNumEntries   = liNumEntries;
        meState        = E_STATE_CHECK_CREATE_ENTRIES;   // stw 1, 0(this)
        mpAllocListSet = lpAllocListSet;
        mpOutNeeds     = lpOutNeeds;
        mpOutResources = lpOutResources;
        miCountDown    = 1;                               // stw 1, 0x3C
        mbAllowFailiure           = lbAllowFailiure;
        mbCreateEntryListResource = false;
        mbWaitingForPurgatory     = false;
    }

    // -------- CheckListDependencies @ 0x828FF228 --------
    // For each bundle entry: resolve it (following dependency pools). If present, bump the containing
    // pool's ref-count (and, when logging is enabled, warn about a size mismatch between the resident
    // resource and the one the bundle asks for -- a hash collision). If absent, count it and mark it as
    // needing creation. Finally latch whether the entry-list resource itself must be created.
    s32 AllocatePoolModuleState::CheckListDependencies()
    {
        s32 liNotFoundCount = 0;

        for (s32 i = 0; i < miNumEntries; ++i)
        {
            const BundleV2::ResourceEntry& lrEntry = mpEntries[i];

            Pool* lpContainingPool;
            s32   liResourceIndex;
            Entry* lpResource = mpPool->FindResourceWithDependencies(lrEntry.mResourceId, &lpContainingPool,
                                                                     true, 3, &liResourceIndex);

            bool lbNeedsCreating;
            if (lpResource)
            {
                // Debug-only hash-collision sanity check: the resident resource's per-pool size should
                // match the bundle's requested uncompressed size.
                for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
                {
                    CGS_ASSERT(luMemType < BundleV2::E_MEMTYPE_NUMTYPES, "luMemType < E_MEMTYPE_NUMTYPES");

                    if (lpResource->mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_size
                            != lrEntry.GetUncompressedSize(luMemType)
                        && (CgsDev::Message::gxMessageFilterFlags & 1))
                    {
                        *CgsDev::Log::gpDebugPrint << "Hash conflict - resource ";
                        *CgsDev::Log::gpDebugPrint << lrEntry.mResourceId;   // CgsResource::operator<< (16 hex digits)
                        *CgsDev::Log::gpDebugPrint << ", ";
                        // The X360 streams the resident resource's debug name here through a debug-only
                        // helper (unrecovered); its own null-name branch prints "[NULL]".
                        *CgsDev::Log::gpDebugPrint << "[NULL]";
                        *CgsDev::Log::gpDebugPrint << " conlicts\n";   // verbatim X360 rodata (typo preserved)
                    }
                }

                s16 liRefCount = lpContainingPool->GetEntryRefCount(liResourceIndex);
                lpContainingPool->SetEntryRefCount(liResourceIndex, liRefCount > 0 ? liRefCount + 1 : 1);
                lbNeedsCreating = false;
            }
            else
            {
                ++liNotFoundCount;
                lbNeedsCreating = true;
            }

            mpOutNeeds[i] = lbNeedsCreating;
        }

        mbCreateEntryListResource = CheckEntryListDependency();
        return liNotFoundCount;
    }

    // -------- CheckEntryListDependency @ 0x828F7AE8 --------
    // True if the list's own entry-list resource is absent (needs creating). When present, bump its
    // ref-count (start it at 1 if it had lapsed to <= 0).
    bool AllocatePoolModuleState::CheckEntryListDependency()
    {
        s32 liIndex;
        if (mpPool->FindResource(mListId, true, 3, &liIndex) == nullptr)
        {
            return true;
        }

        s16 liRefCount = mpPool->GetEntryRefCount(liIndex);
        if (liRefCount <= 0)
        {
            mpPool->SetEntryRefCount(liIndex, 1);
        }
        else
        {
            mpPool->IncEntryRefCount(liIndex);
        }
        return false;
    }

    // -------- CreateResourceList @ 0x828FF480 --------
    // Reserve a batch of pool slots (one per entry needing creation, plus one for the entry-list
    // resource if it too must be created). For every entry flagged as needing creation, build a
    // NewResource from the bundle entry and create it in its reserved slot, staging its allocation
    // request. Finally, if required, create the entry-list resource itself with a computed
    // size (its id table). Returns false on any pool create failure.
    bool AllocatePoolModuleState::CreateResourceList()
    {
        s16 liNumSlots = miNeedCount;
        if (mbCreateEntryListResource)
        {
            ++liNumSlots;
        }

        const s16* lpBatchSlots = mpPool->CreateBatchEntrySlots(liNumSlots);
        if (!lpBatchSlots)
        {
            return false;
        }

        CGS_ASSERT(miNeedCount <= PoolModule::KI_MAX_ALLOCATION_REQUESTS,
                   "There are more entries in this bundle than there are allocation requests - increment KI_MAX_ALLOCATION_REQUESTS\n");   // :390

        miNeedCount = 0;

        for (s32 i = 0; i < miNumEntries; ++i)
        {
            if (!mpOutNeeds[i])
            {
                continue;
            }

            const BundleV2::ResourceEntry& lrEntry = mpEntries[i];

            NewResource lNewResource;
            lNewResource.mID = lrEntry.mResourceId;
            // The X360 calls ResourceEntry::GetUncompresssedResourceDescriptor here to fill the three
            // in-memory (size, alignment) pairs of the 3-pool descriptor; inlined to the create path's
            // SmallResourceDescriptor form.
            for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
            {
                lNewResource.mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_size      = lrEntry.GetUncompressedSize(luMemType);
                lNewResource.mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_alignment = lrEntry.GetUncompresssedAlignment(luMemType);
            }
            lNewResource.mpResourceType = mpPoolModule->FindResourceType(lrEntry.muResourceTypeId);
            CGS_ASSERT(lNewResource.mpResourceType != nullptr, "Invalid resource type on resource ");   // :407
            lNewResource.miNumImports        = lrEntry.muImportCount;
            lNewResource.muImportTableOffset = lrEntry.muImportOffset;

            s16    liSlot = lpBatchSlots[miNeedCount];
            Entry* lpOutEntry;
            if (mpPool->CreateEntryInSlot(&lNewResource, &lpOutEntry, liSlot, false) != Pool::CREATERESULT_OK)
            {
                return false;
            }

            mpPool->BuildAllocRequestForEntry(mpAllocListSet, static_cast<u16>(liSlot), lpOutEntry);
            mpPool->SetEntryImportCount(liSlot, i);   // slot -> bundle-entry index reverse map
            ++miNeedCount;
        }

        if (mbCreateEntryListResource)
        {
            NewResource lNewResource;
            lNewResource.mID                 = mListId;
            lNewResource.miNumImports        = 0;
            lNewResource.muImportTableOffset = 0;
            lNewResource.mpResourceType      = &mEntryListResourceType;

            u32 luAlignment = mpPool->GetHeapAlignment(E_MEMTYPE_MAINMEMORY);
            u32 luSize      = (8u * static_cast<u32>(miNumEntries) + luAlignment + 263u) & ~(luAlignment - 1u);
            lNewResource.mResourceDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
            lNewResource.mResourceDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
            lNewResource.mResourceDescriptor.m_baseResourceDescriptors[1].m_size      = 0;
            lNewResource.mResourceDescriptor.m_baseResourceDescriptors[1].m_alignment = 0;
            lNewResource.mResourceDescriptor.m_baseResourceDescriptors[2].m_size      = 0;
            lNewResource.mResourceDescriptor.m_baseResourceDescriptors[2].m_alignment = 0;

            s16    liSlot = lpBatchSlots[miNeedCount];
            Entry* lpOutEntry;
            if (mpPool->CreateEntryInSlot(&lNewResource, &lpOutEntry, liSlot, false) != Pool::CREATERESULT_OK)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "Failed to create entry list resource\n";
                }
                return false;
            }

            mpPool->BuildAllocRequestForEntry(mpAllocListSet, static_cast<u16>(liSlot), lpOutEntry);
            mpPool->SetEntryImportCount(liSlot, -1);
        }

        return true;
    }

    // -------- CreateEntryListResource @ 0x828F7BA0 --------
    // Find the (now created) entry-list resource. If we created it this pass, write the member id table
    // into its resource memory (count + packed ids). Returns the resolved entry.
    Entry* AllocatePoolModuleState::CreateEntryListResource()
    {
        s32    liIndex;
        Entry* lpResource = mpPool->FindResource(mListId, true, 3, &liIndex);
        CGS_ASSERT(lpResource != nullptr, "Could not find entry list resource\n");   // :527

        if (mbCreateEntryListResource)
        {
            EntryListResourceData* lpData =
                static_cast<EntryListResourceData*>(lpResource->mResource.GetMemoryResource());

            lpData->muNumEntries = static_cast<u32>(miNumEntries);
            lpData->muHeaderFlag = 0;
            for (s32 i = 0; i < miNumEntries; ++i)
            {
                lpData->maEntryIds[i] = mpEntries[i].mResourceId;
            }
        }

        return lpResource;
    }

    // -------- UndoEntryCreations @ 0x828F7CB0 --------
    // Roll back the ref-count increments CheckListDependencies made: for every entry that was already
    // present (mpOutNeeds[i] == false), re-resolve it and decrement its ref-count on the pool.
    void AllocatePoolModuleState::UndoEntryCreations()
    {
        for (s32 i = 0; i < miNumEntries; ++i)
        {
            if (mpOutNeeds[i])
            {
                continue;
            }

            Pool* lpContainingPool;
            s32   liResourceIndex;
            Entry* lpResource = mpPool->FindResourceWithDependencies(mpEntries[i].mResourceId, &lpContainingPool,
                                                                     true, 3, &liResourceIndex);
            CGS_ASSERT(lpResource != nullptr, "Could not find resourc even though it was there last frame\n");   // :573

            mpPool->DecEntryRefCount(liResourceIndex);
        }
    }
}
