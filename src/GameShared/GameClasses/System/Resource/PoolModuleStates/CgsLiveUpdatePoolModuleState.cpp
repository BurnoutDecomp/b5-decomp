#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsLiveUpdatePoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"        // Pool / NewResource / Entry / SmallResource
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::FindResourceType / GetPoolByIndex / KI_MAX_POOLS
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"        // Type::GetCachedId (type-id compare)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"    // Events::AllocateResourceListResponse
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags

#include <cstddef>   // offsetof (layout pin for the entry-list resource-memory view)
#include <cstring>   // memset (clear caller's "needs" flags when no pool holds the list)

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsLiveUpdatePoolModuleState.cpp) -- the working steps of the
// PoolModule's "live update" (in-place bundle hot-reload) state machine:
//   CgsResource::LiveUpdatePoolModuleState::BeginAllocation        @ 0x828DAAF0
//   CgsResource::LiveUpdatePoolModuleState::GenerateResponse       @ 0x828E4200
//   CgsResource::LiveUpdatePoolModuleState::CheckListDependencies  @ 0x828F8108
//   CgsResource::LiveUpdatePoolModuleState::FindPoolToUpdate       @ 0x828F8198
//   CgsResource::LiveUpdatePoolModuleState::DeleteOriginalResources@ 0x828FFAF0
//   CgsResource::LiveUpdatePoolModuleState::AllocateNewResources   @ 0x82902AD0
//   CgsResource::LiveUpdatePoolModuleState::CreateEntryListResource@ 0x82902FD8
// Each is bodied against the X360 asm; the pool/bundle/entry collaborators are reached through their
// owning types' named members and accessors, not raw offsets. The X360 asserts that stream a formatted
// message into the debug buffer are expressed through CGS_ASSERT (the streamed id, like the file/line, is
// dropped -- the same convention the sibling AllocatePoolModuleState uses). Debug prints go through the
// gpDebugPrint stream, gated on the message filter, exactly as the asm does.

namespace CgsResource
{
namespace
{
    // The entry-list resource's in-memory payload. CreateEntryListResource sizes the pool allocation as
    // alignUp(8 * miNumEntries + 263, poolAlign): a small header, the member count at +0x100, then the
    // packed array of member resource ids at +0x108. This is resource memory -- an allocated data blob
    // whose layout is fixed by the format, not a managed C++ object -- so it is addressed as a single
    // data view (the AGENTS.md serialised/resource-blob exception), not a chain of offset casts.
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

    // -------- BeginAllocation @ 0x828DAAF0 --------
    // Latch the request working set and move the machine out of idle. The X360 asserts the machine was
    // idle (streamed message) before overwriting the working set; the elapsed-wait counter is reset and
    // the step token is set to its start value (3).
    void LiveUpdatePoolModuleState::BeginAllocation(Pool* lpPool, ID lListId,
                                                    const BundleV2::ResourceEntry* lpEntries, s32 liNumEntries,
                                                    bool* lpOutNeeds, SmallResource* lpOutResources)
    {
        CGS_ASSERT(meState == E_STATE_IDLE, "Can not begin allocation during none-idle state\n");   // :75

        miElapsedWaitFrames = 0;              // stw 0, 0x38
        mpPool              = lpPool;          // stw r4, 0x04
        mListId             = lListId;         // std r5, 0x08
        mpEntries           = lpEntries;       // stw r6, 0x10
        miNumEntries        = liNumEntries;    // stw r7, 0x14
        meState             = E_STATE_WAIT;    // stw 3, 0x00
        mpOutNeeds          = lpOutNeeds;      // stw r8, 0x18
        mpOutResources      = lpOutResources;  // stw r9, 0x1C
    }

    // -------- Update @ 0x829066E0 (IDA export "Up") --------
    // Run one step of the live-update state machine and return an ELiveUpdateResult. The machine walks
    // WAIT -> FREE_SOURCE_ENTRIES -> ALLOCATE -> IDLE, each state falling straight into the next within a
    // single call once its precondition is met (the X360 goto-chains the cases in exactly this order):
    //   WAIT: spin for 10 frames (returning BUSY) to let outstanding work settle, then advance.
    //   FREE_SOURCE_ENTRIES: pick the pool to update (if not already latched); if nothing holds any of the
    //     list's resources, clear the caller's "needs" flags and finish. Otherwise resolve dependencies
    //     (latching miNeedCount) and delete the original resource memory.
    //   ALLOCATE: allocate the new resource memory and (re)create the entry-list resource; on success
    //     latch mpOutListEntry and finish.
    // Any allocation failure drops the machine back to IDLE and returns ERROR.
    u32 LiveUpdatePoolModuleState::Update()
    {
        switch (meState)
        {
        case E_STATE_IDLE:
            return E_RESULT_DONE;

        case E_STATE_WAIT:
            if (++miElapsedWaitFrames < 10)
            {
                return E_RESULT_BUSY;
            }
            meState = E_STATE_FREE_SOURCE_ENTRIES;
            // fall through

        case E_STATE_FREE_SOURCE_ENTRIES:
            meState = E_STATE_FREE_SOURCE_ENTRIES;

            if (mpPool == nullptr)
            {
                FindPoolToUpdate();
                if (mpPool == nullptr)
                {
                    // No resident copy of any list resource -> nothing to live-update.
                    memset(mpOutNeeds, 0, miNumEntries);
                    meState = E_STATE_IDLE;
                    return E_RESULT_DONE;
                }
            }

            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "Live updating Pool [";
                *CgsDev::Log::gpDebugPrint << mpPool->GetName();
                *CgsDev::Log::gpDebugPrint << "]\n";
            }

            miNeedCount = static_cast<s16>(CheckListDependencies());
            if (miNeedCount == 0)
            {
                meState = E_STATE_IDLE;
                return E_RESULT_DONE;
            }

            if (!DeleteOriginalResources())
            {
                meState = E_STATE_IDLE;
                return E_RESULT_ERROR;
            }
            // fall through

        case E_STATE_ALLOCATE:
        {
            meState = E_STATE_ALLOCATE;

            if (!AllocateNewResources())
            {
                meState = E_STATE_IDLE;
                return E_RESULT_ERROR;
            }

            Entry* lpListEntry = CreateEntryListResource();
            if (lpListEntry != nullptr)
            {
                mpOutListEntry = lpListEntry;
                meState = E_STATE_IDLE;
                return E_RESULT_DONE;
            }

            CGS_ASSERT(lpListEntry != nullptr, "Could not create resource to hold entry list with entries\n");   // :179
            meState = E_STATE_IDLE;
            return E_RESULT_ERROR;
        }

        default:
            CGS_ASSERT(false, "Live update state is invalid\n");   // :193
            return E_RESULT_ERROR;
        }
    }

    // -------- GenerateResponse @ 0x828E4200 --------
    // Fill the caller's response record from this step's working set. The driver passes a raw response
    // buffer (posted to the pool output queue by explicit byte size); view it as the typed response event.
    void LiveUpdatePoolModuleState::GenerateResponse(void* lpOutResponse)
    {
        Events::AllocateResourceListResponse* lpResponse =
            static_cast<Events::AllocateResourceListResponse*>(lpOutResponse);

        lpResponse->mpUser    = nullptr;                              // +0x00  stw 0
        lpResponse->miEventId = 0;                                    // +0x04  stw 0 (driver re-stamps)
        lpResponse->miPoolId  = mpPool ? mpPool->GetId() : -1;        // +0x08  (GetId inlined; -1 if no pool)
        lpResponse->mListId       = mListId;         // +0x10
        lpResponse->mpEntries     = mpEntries;       // +0x18
        lpResponse->miNumEntries  = miNumEntries;    // +0x1C
        lpResponse->mpNeeds       = mpOutNeeds;      // +0x20
        lpResponse->mpResources   = mpOutResources;  // +0x24
        lpResponse->mpListEntry   = mpOutListEntry;  // +0x28
    }

    // -------- CheckListDependencies @ 0x828F8108 --------
    // For each bundle entry, resolve it (following dependency pools) and flag whether it needs creating
    // (mpOutNeeds[i] = not-found). Returns the number of entries processed.
    s32 LiveUpdatePoolModuleState::CheckListDependencies()
    {
        s32 liCount = 0;

        for (s32 i = 0; i < miNumEntries; ++i)
        {
            Pool*  lpContainingPool;
            s32    liResourceIndex;
            Entry* lpResource = mpPool->FindResourceWithDependencies(mpEntries[i].mResourceId, &lpContainingPool,
                                                                     false, 2, &liResourceIndex);
            mpOutNeeds[i] = (lpResource == nullptr);
            ++liCount;
        }

        return liCount;
    }

    // -------- FindPoolToUpdate @ 0x828F8198 --------
    // Poll every valid pool for how many of the list's resources it currently holds, and latch mpPool to
    // the pool with the most matches (null if none of the list's resources are resident anywhere).
    void LiveUpdatePoolModuleState::FindPoolToUpdate()
    {
        s32  laiPoolPoll[PoolModule::KI_MAX_POOLS] = {};
        bool lbResourceFound = false;

        for (s32 liEntryIndex = 0; liEntryIndex < miNumEntries; ++liEntryIndex)
        {
            for (s32 liPoolIndex = 0; liPoolIndex < PoolModule::KI_MAX_POOLS; ++liPoolIndex)
            {
                Pool& lrPool = mpPoolModule->GetPoolByIndex(liPoolIndex);
                if (lrPool.GetId() >= 0 && lrPool.IsValid())
                {
                    s32 liResourceIndex;
                    if (lrPool.FindResource(mpEntries[liEntryIndex].mResourceId, false, 2, &liResourceIndex))
                    {
                        lbResourceFound = true;
                        ++laiPoolPoll[liPoolIndex];
                    }
                }
            }
        }

        s32 liHighestPoolIndex = 0;
        for (s32 liPoolIndex = 1; liPoolIndex < PoolModule::KI_MAX_POOLS; ++liPoolIndex)
        {
            if (laiPoolPoll[liPoolIndex] > laiPoolPoll[liHighestPoolIndex])
            {
                liHighestPoolIndex = liPoolIndex;
            }
        }

        mpPool = lbResourceFound ? &mpPoolModule->GetPoolByIndex(liHighestPoolIndex) : nullptr;
    }

    // -------- DeleteOriginalResources @ 0x828FFAF0 --------
    // For every entry that was already present (mpOutNeeds[i] == false), re-resolve it and delete its
    // resource memory in the pool that owns it (freeing the old allocation before the new one is built).
    // Always returns true.
    bool LiveUpdatePoolModuleState::DeleteOriginalResources()
    {
        for (s32 i = 0; i < miNumEntries; ++i)
        {
            if (mpOutNeeds[i])
            {
                continue;
            }

            Pool* lpContainingPool;
            s32   liResourceIndex;
            mpPool->FindResourceWithDependencies(mpEntries[i].mResourceId, &lpContainingPool,
                                                 false, 2, &liResourceIndex);
            lpContainingPool->DeleteMemoryForEntry(mpEntries[i].mResourceId);
        }

        return true;
    }

    // -------- AllocateNewResources @ 0x82902AD0 --------
    // Give every entry fresh memory: create brand-new entries (mpOutNeeds[i] == true) in mpPool, or
    // reallocate the memory of existing ones in their containing pool, writing the resulting resource
    // handle into the caller's output array. Then re-resolve mpPool and every pool that lists it as a
    // dependency so their cross-pool imports pick up the moved memory. Returns false on any pool
    // create/realloc failure.
    bool LiveUpdatePoolModuleState::AllocateNewResources()
    {
        bool lbSuccess = true;

        for (s32 i = 0; i < miNumEntries; ++i)
        {
            const BundleV2::ResourceEntry& lrEntry = mpEntries[i];

            // Fill the in-memory descriptor from the bundle entry. (The X360 reaches these same
            // per-pool uncompressed size/alignment values through the out-of-line
            // ResourceEntry::GetUncompresssedResourceDescriptor; expressed field-by-field here.)
            SmallResourceDescriptor lEntryDescriptor;
            for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
            {
                lEntryDescriptor.m_baseResourceDescriptors[luMemType].m_size      = lrEntry.GetUncompressedSize(luMemType);
                lEntryDescriptor.m_baseResourceDescriptors[luMemType].m_alignment = lrEntry.GetUncompresssedAlignment(luMemType);
            }

            const u16 luImportCount = lrEntry.muImportCount;

            SmallResource lOutResource;
            bool          lbHaveResult = false;

            if (mpOutNeeds[i])
            {
                // Brand-new resource -> create it in mpPool (allocating its memory immediately).
                NewResource lNewResource;
                lNewResource.mID                 = lrEntry.mResourceId;
                lNewResource.mResourceDescriptor = lEntryDescriptor;
                lNewResource.miNumImports        = luImportCount;
                lNewResource.mpResourceType      = mpPoolModule->FindResourceType(lrEntry.muResourceTypeId);
                lNewResource.muImportTableOffset = lrEntry.muImportOffset;

                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "Creating new entry for Resource [";
                    *CgsDev::Log::gpDebugPrint << lrEntry.mResourceId;
                    *CgsDev::Log::gpDebugPrint << "] in Pool [";
                    const char* lpcPoolName = mpPool->GetName();
                    *CgsDev::Log::gpDebugPrint << (lpcPoolName ? lpcPoolName : "<NULLSTRING>");
                    *CgsDev::Log::gpDebugPrint << "].\n";
                }

                Entry* lpNewEntry;
                s32    liNewIndex;
                if (mpPool->CreateEntry(&lNewResource, &lpNewEntry, &liNewIndex, true) == Pool::CREATERESULT_OK)
                {
                    lOutResource = lpNewEntry->mResource;
                    lbHaveResult = true;
                }
                else
                {
                    lbSuccess = false;
                }
            }
            else
            {
                // Already present -> reallocate its memory in the pool that owns it.
                Pool*  lpContainingPool;
                s32    liResourceIndex;
                Entry* lpFound = mpPool->FindResourceWithDependencies(lrEntry.mResourceId, &lpContainingPool,
                                                                      false, 2, &liResourceIndex);
                CGS_ASSERT(lpFound != nullptr, "Failed to find entry to delete memory for\n");                        // :423
                CGS_ASSERT(lpFound->mpResourceType->GetCachedId() == lrEntry.muResourceTypeId, "Type mismatch during live upate\n");   // :425

                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "Reallocating memory for Resource [";
                    *CgsDev::Log::gpDebugPrint << lrEntry.mResourceId;
                    *CgsDev::Log::gpDebugPrint << "] in Pool [";
                    const char* lpcPoolName = lpContainingPool->GetName();
                    *CgsDev::Log::gpDebugPrint << (lpcPoolName ? lpcPoolName : "<NULLSTRING>");
                    *CgsDev::Log::gpDebugPrint << "].\n";
                }

                if (lpContainingPool->ReAllocateMemoryForEntry(lrEntry.mResourceId, &lEntryDescriptor,
                                                               luImportCount, &lOutResource))
                {
                    mpOutNeeds[i] = true;
                    lbHaveResult  = true;
                }
                else
                {
                    lbSuccess = false;
                }
            }

            if (lbHaveResult)
            {
                mpOutResources[i] = lOutResource;
            }
        }

        mpPool->ResolveAllResources();

        // Re-resolve every pool that lists mpPool as a dependency, so their cross-pool imports refer to
        // the relocated/new memory.
        for (s32 liPoolIndex = 0; liPoolIndex < PoolModule::KI_MAX_POOLS; ++liPoolIndex)
        {
            Pool& lrPool = mpPoolModule->GetPoolByIndex(liPoolIndex);
            if (lrPool.GetId() == -1)
            {
                continue;
            }

            s32 liNumDependencies = lrPool.GetNumDependencies();
            if (liNumDependencies <= 0)
            {
                continue;
            }

            for (s32 liDependency = 0; liDependency < liNumDependencies; ++liDependency)
            {
                if (lrPool.GetDependency(liDependency) == mpPool)
                {
                    lrPool.GetDependency(liDependency)->ResolveAllResources();
                    break;
                }
            }
        }

        return lbSuccess;
    }

    // -------- CreateEntryListResource @ 0x82902FD8 --------
    // (Re)create the list's own entry-list resource: delete any stale one, create a fresh entry sized for
    // the member id table (alignUp(8 * miNumEntries + 263, poolAlign)), and write the member ids into its
    // resource memory. Returns the created entry (null on create failure).
    Entry* LiveUpdatePoolModuleState::CreateEntryListResource()
    {
        s32    liIndex;
        Entry* lpExisting = mpPool->FindResource(mListId, false, 2, &liIndex);
        if (lpExisting)
        {
            mpPool->DeleteEntry(static_cast<s16>(liIndex));
        }

        NewResource lNewResource;
        lNewResource.mID = mListId;

        const u32 luAlignment = mpPool->GetHeapAlignment(E_MEMTYPE_MAINMEMORY);
        const u32 luSize      = (8u * static_cast<u32>(miNumEntries) + luAlignment + 263u) & ~(luAlignment - 1u);
        lNewResource.mResourceDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lNewResource.mResourceDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
        lNewResource.mResourceDescriptor.m_baseResourceDescriptors[1].m_size      = 0;
        lNewResource.mResourceDescriptor.m_baseResourceDescriptors[1].m_alignment = 1;
        lNewResource.mResourceDescriptor.m_baseResourceDescriptors[2].m_size      = 0;
        lNewResource.mResourceDescriptor.m_baseResourceDescriptors[2].m_alignment = 1;

        lNewResource.miNumImports        = 0;
        lNewResource.mpResourceType      = &mEntryListResourceType;
        lNewResource.muImportTableOffset = 0;

        Entry* lpOutEntry;
        s32    liOutIndex;
        if (mpPool->CreateEntry(&lNewResource, &lpOutEntry, &liOutIndex, true) != Pool::CREATERESULT_OK)
        {
            return nullptr;
        }

        EntryListResourceData* lpData =
            static_cast<EntryListResourceData*>(lpOutEntry->mResource.GetMemoryResource());
        lpData->muNumEntries = static_cast<u32>(miNumEntries);
        lpData->muHeaderFlag = 0;
        for (s32 i = 0; i < miNumEntries; ++i)
        {
            lpData->maEntryIds[i] = mpEntries[i].mResourceId;
        }

        return lpOutEntry;
    }
}
