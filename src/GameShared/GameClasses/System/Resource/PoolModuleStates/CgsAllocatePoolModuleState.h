#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BasePoolModuleState / AllocListSet (fwd)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"                                   // ID (embedded by value)
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"                              // BundleV2::ResourceEntry (pointer member)
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"                            // EntryListResourceType (embedded by value)

// CgsResource::AllocatePoolModuleState -- the PoolModule's "allocating a resource list" step state.
// PoolModule::UpdateAllocating (X360 0x82904860) polls Update() once per frame and dispatches on the
// EAllocateResult it returns: SUCCESS/SIMPLEFRAG -> finalise + post the response; ERROR -> assert;
// PEND -> wait; INTELLIFRAG -> hand off to IntelliFragPoolModuleState.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX. Base + layout: the DecFIGS DWARF
// (CgsAllocatePoolModuleState.h) attests `AllocatePoolModuleState : public BasePoolModuleState` (the
// data-less pool-module-state base -- NOT the defragmenter base), and the X360 asm confirms it: the
// step token meState is read/written at this+0 (no leading vtable/base data). BeginAllocation
// (0x828DA568) pins the working-set field order stored from its args -- mpPool@+4, mListId(8B)@+8,
// mpEntries@+0x10, miNumEntries@+0x14, mpAllocListSet@+0x18, mpOutNeeds@+0x1C, mpOutResources@+0x20,
// miCountDown(=1)@+0x3C, mbAllowFailiure@+0x40, then the two cleared flags @+0x41/+0x42 -- and the
// create/undo passes pin miNeedCount@+0x28 and mpPoolModule@+0x38. Field widths follow the x64 PC
// target (pointers widen); field order/types are faithful. Identify members by name, not byte offset.
namespace CgsResource
{
    class Pool;
    class PoolModule;
    class Entry;

    class AllocatePoolModuleState : public BasePoolModuleState
    {
    public:
        // The per-frame poll result PoolModule::UpdateAllocating dispatches on. (Enum kept as the
        // driver reconstruction spells it; DWARF names the same 0..4 values
        // SUCCESS/ERROR/PEND/DEFRAGMENT/FAILED_SAFELY.)
        enum EAllocateResult
        {
            E_RESULT_SUCCESS    = 0,
            E_RESULT_ERROR      = 1,
            E_RESULT_PEND       = 2,
            E_RESULT_INTELLIFRAG = 3,
            E_RESULT_SIMPLEFRAG = 4,
        };

        // The step machine's internal state token (meState @ this+0). DWARF CgsAllocatePoolModuleState.h:55.
        enum EInternalState
        {
            E_STATE_IDLE                 = 0,
            E_STATE_CHECK_CREATE_ENTRIES = 1,
            E_STATE_ALLOCATE             = 2,
            E_STATE_MERGE_ALLOCATIONS    = 3,
            E_STATE_DEFRAG_WAITING       = 4,
        };

        // @ 0x828DA568 -- arm the state from an allocate-resource-list request: latch the pool, list id,
        // bundle-entry array + count, the batch working set and the caller's output arrays, then move to
        // E_STATE_CHECK_CREATE_ENTRIES. Asserts the machine was idle. mpOutResources is the caller's
        // output handle array (DWARF type ResourceHandle::Resource*, not yet homed -> pointer-only void*).
        void BeginAllocation(Pool* lpPool, ID lListId, const BundleV2::ResourceEntry* lpEntries,
                             s32 liNumEntries, AllocListSet* lpAllocListSet, bool* lpOutNeeds,
                             void* lpOutResources, bool lbAllowFailiure);

        // @ 0x82902640 -- run one allocation step; returns the EAllocateResult above.
        u32 Update();

        // Fill the caller's resource-list response record from this step's working set. Body lives in
        // this state's own TU (deferred); the driver delegates the working-set copy here.
        void GenerateResponse(void* lpOutResponse);

    private:
        // @ 0x828FF228 -- resolve every bundle entry's already-present dependency: per entry, look it up
        // (with its dependencies) and either bump its containing pool's ref-count (present) or flag it as
        // needing creation (mpOutNeeds[i]); returns the count that need creating, and latches whether the
        // entry-list resource itself needs creating (CheckEntryListDependency).
        s32 CheckListDependencies();

        // @ 0x828F7AE8 -- true if the list's own entry-list resource is absent (so it must be created);
        // when present, bumps its ref-count.
        bool CheckEntryListDependency();

        // @ 0x828FF480 -- reserve a batch of pool slots, then create an entry (+ alloc request) for every
        // entry flagged as needing creation, plus the entry-list resource itself if required. False on
        // any pool create failure.
        bool CreateResourceList();

        // @ 0x828F7BA0 -- find the (now created) entry-list resource and, if we created it this pass,
        // write the member id table into its resource memory. Returns the resolved entry.
        Entry* CreateEntryListResource();

        // @ 0x828F7CB0 -- roll back the ref-count increments CheckListDependencies made (for every entry
        // that was already present) when the allocation could not be completed.
        void UndoEntryCreations();

        // ---- Layout (field order/types from DWARF; offsets verified against the X360 asm) ----
        EInternalState                  meState;                    // +0x00  :93
        Pool*                           mpPool;                     // +0x04  :94
        ID                              mListId;                    // +0x08  :95
        const BundleV2::ResourceEntry*  mpEntries;                  // +0x10  :96
        s32                             miNumEntries;               // +0x14  :97
        AllocListSet*                   mpAllocListSet;             // +0x18  :98
        bool*                           mpOutNeeds;                 // +0x1C  :99
        void*                           mpOutResources;             // +0x20  :100 (DWARF ResourceHandle::Resource*)
        Entry*                          mpOutListEntry;             // +0x24  :101
        s16                             miNeedCount;                // +0x28  :102
        EntryListResourceType           mEntryListResourceType;     // +0x2C  :103
        PoolModule*                     mpPoolModule;               // +0x38  :104
        s32                             miCountDown;                // +0x3C  :105
        bool                            mbAllowFailiure;            // +0x40  :106
        bool                            mbCreateEntryListResource;  // +0x41  :107
        bool                            mbWaitingForPurgatory;      // +0x42  :108
    };
}
