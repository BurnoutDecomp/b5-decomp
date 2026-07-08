#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BasePoolModuleState
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // ID (embedded by value)
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"   // BundleV2::ResourceEntry (pointer member)
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h" // EntryListResourceType (embedded by value)

// CgsResource::LiveUpdatePoolModuleState -- the PoolModule's "live update" step state. While a bundle
// is being hot-reloaded in place, PoolModule::UpdateLiveUpdate (X360 0x82906E70) polls Update() and on
// completion (result 0) calls GenerateResponse() to fill the response record it posts to the pool
// output queue. This is the sibling of the "allocate a resource list" state (CgsAllocatePoolModuleState);
// it reuses CgsResource::Pool / BundleV2::ResourceEntry / NewResource / Entry.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX. Base + layout: the DecFIGS DWARF
// (CgsLiveUpdatePoolModuleState.h) attests `LiveUpdatePoolModuleState : public BasePoolModuleState`
// (the data-less pool-module-state base), and the X360 asm confirms it: the step token meState is
// read/written at this+0. BeginAllocation (0x828DAAF0) pins the working-set field order stored from its
// args -- mpPool@+0x04, mListId(8B)@+0x08, mpEntries@+0x10, miNumEntries@+0x14, mpOutNeeds@+0x18,
// mpOutResources@+0x1C, miElapsedWaitFrames(=0)@+0x38 -- and GenerateResponse (0x828E4200) pins
// mpOutListEntry@+0x20; CreateEntryListResource (0x82902FD8) pins mpPoolModule@+0x34 and
// mEntryListResourceType@+0x28. Field widths follow the x64 PC target (pointers widen); field
// order/types are faithful. Identify members by name, not byte offset.
namespace CgsResource
{
    class Pool;
    class PoolModule;
    class Entry;
    struct SmallResource;   // ResourceHandle::Resource (3 base pointers) -- pointer member only

    class LiveUpdatePoolModuleState : public BasePoolModuleState
    {
    public:
        // The per-frame poll result PoolModule::UpdateLiveUpdate dispatches on (0=done, 1=error,
        // 2/3=still working). (DWARF names the same 0..3 values SUCCESS/ERROR/PEND/DEFRAGMENT.)
        enum ELiveUpdateResult
        {
            E_RESULT_DONE  = 0,
            E_RESULT_ERROR = 1,
            E_RESULT_BUSY  = 2,
            E_RESULT_BUSY2 = 3,
        };

        // The step machine's internal state token (meState @ this+0). DWARF CgsLiveUpdatePoolModuleState.h:58.
        enum EInternalState
        {
            E_STATE_IDLE               = 0,
            E_STATE_FREE_SOURCE_ENTRIES = 1,
            E_STATE_ALLOCATE           = 2,
            E_STATE_WAIT               = 3,
        };

        // @ 0x828DAAF0 -- arm the state from a live-update request: latch the pool, list id, bundle-entry
        // array + count and the caller's output arrays, reset the wait counter, and move the machine to
        // its start token (3). Asserts the machine was idle. mpOutResources is the caller's output handle
        // array (DWARF ResourceHandle::Resource* == a 3-base-pointer SmallResource per entry).
        void BeginAllocation(Pool* lpPool, ID lListId, const BundleV2::ResourceEntry* lpEntries,
                             s32 liNumEntries, bool* lpOutNeeds, SmallResource* lpOutResources);

        // @ 0x829066E0 -- run one live-update step; returns the ELiveUpdateResult above. (IDA names the
        // export "Up" -- a truncated "Update".) Body lives in this state's own TU.
        u32 Update();

        // @ 0x828E4200 -- fill the caller's 48-byte response record on completion from this step's
        // working set. Takes void* (the driver passes a raw response buffer); the body views it as an
        // Events::AllocateResourceListResponse.
        void GenerateResponse(void* lpOutResponse);

    private:
        // @ 0x828F8108 -- for every bundle entry, resolve it (following dependency pools) and flag it as
        // needing creation (mpOutNeeds[i] = not-found). Returns the number of entries processed.
        s32 CheckListDependencies();

        // @ 0x828FFAF0 -- delete the resource memory of every entry that already existed (mpOutNeeds[i]
        // == false), in the pool that owns it. Always returns true.
        bool DeleteOriginalResources();

        // @ 0x82902AD0 -- (re)allocate memory for every entry: create brand-new entries (mpOutNeeds[i]
        // == true) or reallocate existing ones, write the resulting resource handles into the caller's
        // output array, then re-resolve mpPool and every pool that depends on it. False on any pool
        // create/realloc failure.
        bool AllocateNewResources();

        // @ 0x82902FD8 -- (re)create the list's own entry-list resource: delete any stale one, create a
        // fresh one sized for the member id table, and write the member ids into its resource memory.
        // Returns the created entry (null on create failure).
        Entry* CreateEntryListResource();

        // @ 0x828F8198 -- pick the pool to live-update: poll every valid pool for how many of the list's
        // resources it holds and latch mpPool to the pool with the most matches (null if none match).
        void FindPoolToUpdate();

        // ---- Layout (field order/types from DWARF; offsets verified against the X360 asm) ----
        EInternalState                  meState;                 // +0x00  :93
        Pool*                           mpPool;                  // +0x04  :94
        ID                              mListId;                 // +0x08  :95
        const BundleV2::ResourceEntry*  mpEntries;               // +0x10  :96
        s32                             miNumEntries;            // +0x14  :97
        bool*                           mpOutNeeds;              // +0x18  :98
        SmallResource*                  mpOutResources;          // +0x1C  :99 (DWARF ResourceHandle::Resource*)
        Entry*                          mpOutListEntry;          // +0x20  :100
        s16                             miNeedCount;             // +0x24  :101
        EntryListResourceType           mEntryListResourceType;  // +0x28  :102
        PoolModule*                     mpPoolModule;            // +0x34  :103
        s32                             miElapsedWaitFrames;     // +0x38  :104
    };
}
