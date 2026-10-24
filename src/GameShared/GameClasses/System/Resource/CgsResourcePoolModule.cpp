#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/System/PC/CgsResourcePoolBindingsPC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h" // original formatted pool assertions
#include "rw/rwcore_structs.h"                        // rw::IResourceAllocator / Resource (CreatePool test driver)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [5b TEST] trace
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"  // CreateResourceRequest/Response + MemoryIO buffers (async dispatch)
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"  // PoolIO::OutputBuffer (request target)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // AcquireResourceRequest/Response
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // ResourceHandle (acquire-list output array)
#include "GameShared/GameClasses/System/Resource/CgsResourceIdList.h"    // ResourceIdList (acquire-list payload)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"       // ResourcePtr<ResourceIdList>
#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include <new>
#include "pc/gcm/renderengine/FrameProfile.h"

// Original pool manager lifecycle and native storage. The resource-list
// dispatch is integrated separately from construction of its working state.
namespace CgsResource
{
    // Original initial monitor words are -1; trace IDs are reset to zero by Construct.
    s32 PoolModule::_miAllocateListTraceId = 0; // target82F33FF4
    s32 PoolModule::_miFixupListTraceId = 0; // target82F33FF8
    s32 PoolModule::_miAllocateStateTraceId = 0; // target82F33FFC
    s32 PoolModule::_miIntellifragStateTraceId = 0; // target82F34000
    namespace
    {
        const char* const KPC_POOLMODULE_CREATE_SRC =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsPoolModule.cpp";

        void FirePoolCreateAssert(const char* lpcMessage, int liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcMessage, KPC_POOLMODULE_CREATE_SRC, liLine);
            CgsDev::Assert::EndAssert();
        }

        void FireInvalidPoolIdAssert(s32 liPoolId)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Pool id " << liPoolId << " is invalid\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                                       KPC_POOLMODULE_CREATE_SRC, 916);
            CgsDev::Assert::EndAssert();
        }

        void FireDuplicatePoolIdAssert(s32 liPoolId)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Pool id " << liPoolId << "is already in use\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                                       KPC_POOLMODULE_CREATE_SRC, 922);
            CgsDev::Assert::EndAssert();
        }

        void FireMissingDependencyAssert(s32 liDependencyId)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Failed to find dependency pool with id " << liDependencyId << "\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                                       KPC_POOLMODULE_CREATE_SRC, 1281);
            CgsDev::Assert::EndAssert();
        }
    }

    // @ 0x828D80E8 - linear search of the 128 pools for the one with this id; -1 if absent.
    s32 PoolModule::GetPoolIndex(s32 liPoolId)
    {
        CGS_ASSERT(liPoolId >= 0, "Invalid pool id");
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
        {
            if (maPools[li].GetId() == liPoolId)
                return li;
        }
        return -1;
    }

    // @ 0x828D8268 - linear search of the registered types for this id; null if absent.
    const Type* PoolModule::FindResourceType(u32 luTypeId)
    {
        for (s32 li = 0; li < mNumTypes; ++li)
        {
            if (maTypes[li].muTypeId == luTypeId)
                return maTypes[li].mpType;
        }
        return 0;
    }



    // ARTIST 0x82904B20. The original API returns void; a created Pool* is not part of the
    // caller contract. Assertions continue into the same slot search and filtered diagnostic.
    void PoolModule::CreatePool(const Pool::InitOptions* lpOptions)
    {
        if (lpOptions->miId < 0)
            FireInvalidPoolIdAssert(lpOptions->miId);

        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
        {
            if (maPools[li].GetId() == lpOptions->miId)
                FireDuplicatePoolIdAssert(lpOptions->miId);
        }

        bool lbFoundFreePool = false;
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
        {
            if (maPools[li].GetId() == -1)
            {
                maPools[li].InitPool(lpOptions);
                lbFoundFreePool = true;
                break;
            }
        }
        if (!lbFoundFreePool)
            FirePoolCreateAssert("Failed to find free pool - increase max pools or get rid of some other ones!\n", 937);

        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
        {
            const char* lpcName = lpOptions->mpcName ? lpOptions->mpcName : "<NULLSTRING>";
            *CgsDev::Log::gpDebugPrint << "Created pool " << lpcName << "\n";
        }
    }

    // @ 0x828E2C60 - resumable prepare stage machine: bring up the base module, then Prepare
    // all 128 pools (retrying next frame until all report ready), then clear the receiver
    // queue. Falls through the stages within a call; re-enters at the current stage if a
    // sub-step is not yet ready.
    bool PoolModule::Prepare()
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePoolPrepareStage)
        {
        case E_POOLPREPARE_START:
        case E_POOLPREPARE_BASE:
            mePoolPrepareStage = E_POOLPREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_POOLPREPARE_POOLS:
        {
            mePoolPrepareStage = E_POOLPREPARE_POOLS;
            bool lbAllReady = true;
            for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            {
                if (!maPools[li].Prepare())
                    lbAllReady = false;
            }
            if (!lbAllReady)
                return false;
            mReceiverQueue.Clear();
            // fall through
        }
        case E_POOLPREPARE_DONE:
            mePoolReleaseStage = E_POOLRELEASE_START;
            mePoolPrepareStage = E_POOLPREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid prepare stage");
            return false;
        }
    }

    // @ 0x828E2D78 - resumable release stage machine: Release all 128 pools, clear the
    // receiver queue, then tear down the base module.
    bool PoolModule::Release()
    {
        switch (mePoolReleaseStage)
        {
        case E_POOLRELEASE_START:
        case E_POOLRELEASE_POOLS:
        {
            mePoolReleaseStage = E_POOLRELEASE_POOLS;
            bool lbAllReleased = true;
            for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            {
                if (!maPools[li].Release())
                    lbAllReleased = false;
            }
            if (!lbAllReleased)
                return false;
            mReceiverQueue.Clear();
            // fall through
        }
        case E_POOLRELEASE_BASE:
            mePoolReleaseStage = E_POOLRELEASE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_POOLRELEASE_DONE:
            mePoolPrepareStage = E_POOLPREPARE_START;
            mePoolReleaseStage = E_POOLRELEASE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid release stage");
            return false;
        }
    }

    // @ 0x828E2E90 - clear the receiver queue and tear down the base module.
    void PoolModule::Destruct()
    {
        mReceiverQueue.Clear();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // ARTIST 828FC0B8: construct the pool bank, type registry, scratch staging,
    // allocation/relocation storage and the five request-processing states.
    // ARTIST828FC0B8. The debug allocator is part of the virtual declaration
    // shape, but this X360 body consumes only options/resource from r4/r5.
    void PoolModule::Construct(const InitOptions* lpOptions,
                               rw::IResourceAllocator* lpAllocator,
                               rw::IResourceAllocator* /*lpDebugAllocator*/)
    {
        CgsModule::ModuleSingleBuffered::Construct();
        mePoolPrepareStage = E_POOLPREPARE_START;
        mePoolReleaseStage = E_POOLRELEASE_DONE;
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            maPools[li].Construct();
        mReceiverQueue.Construct();
        mNumTypes = 0;
        const auto AssertMemory = [](bool lbAvailable, s32 liLine)
        {
            if (!lbAvailable)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::MessageBufferPC lBuffer;
                CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
                lStream << "Out of memory\n";
                CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsPoolModule.cpp",
                    liLine);
                CgsDev::Assert::EndAssert();
            }
        };
        // FLAG PC-platform leaf: preserve counts and original minimum alignments;
        // actual C++ record size/alignment supplies the widened native storage.
        void* const lpIdListMemory = lpAllocator->AllocateMemoryResource(
            sizeof(IdListResourceType), alignof(IdListResourceType), nullptr);
        IdListResourceType* const lpIdListType =
            lpIdListMemory ? ::new (lpIdListMemory) IdListResourceType : nullptr;
        lpIdListType->InitCachedValues();
        maTypes[mNumTypes].mpcName = "IDList";
        maTypes[mNumTypes].mpType = lpIdListType;
        maTypes[mNumTypes].muTypeId = lpIdListType->GetCachedId();
        ++mNumTypes;
        for (s32 li = 0; li < lpOptions->miNumGameSpecificTypes; ++li)
        {
            maTypes[mNumTypes].mpcName = lpOptions->mpGameSpecificTypes[li].mpcName;
            maTypes[mNumTypes].mpType = lpOptions->mpGameSpecificTypes[li].mpType;
            maTypes[mNumTypes].muTypeId = maTypes[mNumTypes].mpType->GetCachedId();
            ++mNumTypes;
        }

        ScratchPool::InitOptions lScratchOptions;
        lScratchOptions.muMaxEntries = static_cast<u32>(lpOptions->miMaxResourceToDefrag);
        lScratchOptions.miBankId = 999;
        lScratchOptions.mDescriptor = lpOptions->mDefragBufferDescriptor;
        lScratchOptions.mResource = lpOptions->mDefragBufferResource;
        lScratchOptions.muOverheadMemorySize = ScratchPool::GetOverheadMemoryRequired(&lScratchOptions);
        lScratchOptions.mpOverhead = lpAllocator->AllocateMemoryResource(
            lScratchOptions.muOverheadMemorySize, 16, nullptr);
        AssertMemory(lScratchOptions.mpOverhead != nullptr, 119);
        mScratchPool.Construct();
        mScratchPool.Prepare();
        mScratchPool.InitPool(&lScratchOptions);

        for (s32 liType = 0; liType < 3; ++liType)
        {
            mAllocListSet.manAllocRequestCounts[liType] = 0;
            mAllocListSet.mapAllocRequests[liType] = static_cast<AllocRequest*>(
                lpAllocator->AllocateMemoryResource(sizeof(AllocRequest) * KI_MAX_ALLOCATION_REQUESTS,
                                                    alignof(AllocRequest), nullptr));
            mAllocListSet.mapAllocResults[liType] = static_cast<AllocResult*>(
                lpAllocator->AllocateMemoryResource(sizeof(AllocResult) * KI_MAX_ALLOCATION_REQUESTS,
                                                    alignof(AllocResult), nullptr));
            AssertMemory(mAllocListSet.mapAllocRequests[liType] != nullptr, 130);
            AssertMemory(mAllocListSet.mapAllocResults[liType] != nullptr, 131);
        }
        mpAddressedAllocRequests = static_cast<AllocRequestAddressed*>(
            lpAllocator->AllocateMemoryResource(sizeof(AllocRequestAddressed) * KI_MAX_ALLOCATION_REQUESTS,
                                                alignof(AllocRequestAddressed), nullptr));
        mpRelocateRequests = static_cast<RelocateRequest*>(
            lpAllocator->AllocateMemoryResource(sizeof(RelocateRequest) * KU_MAX_POOL_ENTRIES, 128, nullptr));
        mpLinearHeapNodes = static_cast<LinearHeapNode*>(
            lpAllocator->AllocateMemoryResource(sizeof(LinearHeapNode) * (KU_MAX_LINEAR_HEAP_LENGTH + 8u),
                                                128, nullptr));
        const size_t luSharedStride = sizeof(DistributionEntry) > sizeof(RelocationEntry)
            ? sizeof(DistributionEntry) : sizeof(RelocationEntry);
        void* const lpDistributionAndRelocation = lpAllocator->AllocateMemoryResource(
            luSharedStride * KU_MAX_POOL_ENTRIES, 128, nullptr);
        // One original allocation is viewed by the two actual distribution/copy
        // phases. The existing native record types share this same storage owner.
        mpDistributionEntries = static_cast<DistributionEntry*>(lpDistributionAndRelocation);
        mpRelocationEntries = static_cast<RelocationEntry*>(lpDistributionAndRelocation);
        mpRelocateSources = static_cast<RelocateSource*>(
            lpAllocator->AllocateMemoryResource(sizeof(RelocateSource) * KU_MAX_POOL_ENTRIES, 128, nullptr));
        mRelocator.Construct();
        mRelocationParams.mpOps = mpRelocationEntries;
        mRelocationParams.miNumOps = 0;
        mRelocationParams.mpBounceBuffer = lScratchOptions.mResource.m_baseResources[0];
        mRelocationParams.miBounceBufferSize =
            static_cast<s32>(lScratchOptions.mDescriptor.m_baseResourceDescriptors[0].m_size);
        AssertMemory(mpAddressedAllocRequests != nullptr, 164);
        AssertMemory(mpRelocateRequests != nullptr, 165);
        AssertMemory(mpLinearHeapNodes != nullptr, 166);
        AssertMemory(mpDistributionEntries != nullptr, 167);
        AssertMemory(mpRelocateSources != nullptr, 168);
        mProcessState = E_UPDATESTATE_IDLE;
        mAllocateState.Construct(this);
        mDeAllocateState.Construct(this);
        mIntelliFragState.Construct(this);
        mLiveUpdateState.Construct(this);
        mEmergencyFragState.Construct(this);
        mPendingCreateRequests.Construct();
        mPendingAllocationRequests.Construct();
        mbIsNewModule = true;
    }

    void PoolModule::Construct(const void* lpInitOptions, void* lpAllocator)
    {
        mbIsNewModule = true;   // X360 *(this+4)=1 (set at the end of Construct; base Prepare skips old IO)
        CgsModule::ModuleSingleBuffered::Construct();          // X360: ModuleSingleBuffered::Construct(a1)
        mePoolPrepareStage = E_POOLPREPARE_START;              // X360 *(a1+6700)=0
        mePoolReleaseStage = E_POOLRELEASE_DONE;               // X360 *(a1+6704)=3
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)              // X360: Pool::Construct loop (stride 464)
            maPools[li].Construct();

        rw::IResourceAllocator* lpResourceAllocator = static_cast<rw::IResourceAllocator*>(lpAllocator);
        CGS_ASSERT(lpResourceAllocator != nullptr, "lpAllocator");

        // Register the original built-in ID-list handler before game-specific
        // types. Its cache is initialized through the same virtuals as the game types.
        mNumTypes = 0;
        rw::ResourceDescriptor lIdListDescriptor;
        lIdListDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(IdListResourceType);
        lIdListDescriptor.m_baseResourceDescriptors[0].m_alignment = alignof(IdListResourceType);
        void* lpIdListMemory = lpResourceAllocator->DoAllocate(lIdListDescriptor, nullptr).m_baseResources[0];
        CGS_ASSERT(lpIdListMemory != nullptr, "Out of memory\n");
        IdListResourceType* lpIdListType = ::new (lpIdListMemory) IdListResourceType;
        lpIdListType->InitCachedValues();
        maTypes[mNumTypes].mpType = lpIdListType;
        maTypes[mNumTypes].muTypeId = lpIdListType->GetCachedId();
        maTypes[mNumTypes].mpcName = "IDList";
        ++mNumTypes;
        const InitOptions* lpOptions = static_cast<const InitOptions*>(lpInitOptions);
        if (lpOptions != 0 && lpOptions->mpGameSpecificTypes != 0)
        {
            for (s32 li = 0; li < lpOptions->miNumGameSpecificTypes && mNumTypes < KI_MAX_TYPES; ++li)
            {
                const Type* lpType = lpOptions->mpGameSpecificTypes[li].mpType;
                if (lpType == 0)
                    continue;
                maTypes[mNumTypes].mpType   = lpType;
                // X360 keys on the cached id (*(type+8) == GetCachedId()); TypeRegistry::Register
                // runs InitCachedValues on every handler, so the cached id is live here.
                maTypes[mNumTypes].muTypeId = lpType->GetCachedId();
                maTypes[mNumTypes].mpcName  = lpOptions->mpGameSpecificTypes[li].mpcName;
                ++mNumTypes;
            }
        }

        // The original staging pool borrows the caller's resource buffers and
        // owns tables carved from its separately allocated overhead block below.
        mScratchPool.ConstructNativePC();

        // Bind the receiver queue's backing buffer (create/delete-pool events land here via
        // ProcessMemoryResponse, drained by ProcessReceiverQueue).
        mPendingCreateRequests.Construct();
        mReceiverQueue.Construct();

        // Clear the copy engine's busy latches before binding its operation
        // storage and bounce buffer below.
        mRelocator.Construct();

        // (The real pool set is created by GameDataModule::CreatePools, which drives CreatePool over the
        // extracted memory-map table -- not here. The earlier single [5b TEST] pool was retired once that
        // data-driven loader landed.)
        // ARTIST 828FC0B8 allocates 4096 request/result records for each memory
        // type. Native records contain 64-bit pointers, so retain the count and
        // use their host sizeof/alignment when carving the parent allocator.
        CGS_ASSERT(lpOptions != nullptr, "lpInitOptions");
        ScratchPool::InitOptions lScratchOptions = {};
        lScratchOptions.muMaxEntries = static_cast<u32>(lpOptions->miMaxResourceToDefrag);
        lScratchOptions.miBankId = 999;
        lScratchOptions.mDescriptor = lpOptions->mDefragBufferDescriptor;
        lScratchOptions.mResource = lpOptions->mDefragBufferResource;
        lScratchOptions.muOverheadMemorySize = ScratchPool::GetOverheadMemoryRequired(&lScratchOptions);
        rw::ResourceDescriptor lScratchDescriptor;
        lScratchDescriptor.m_baseResourceDescriptors[0].m_size = lScratchOptions.muOverheadMemorySize;
        lScratchDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        lScratchOptions.mpOverhead = lpResourceAllocator->DoAllocate(lScratchDescriptor, nullptr).m_baseResources[0];
        CGS_ASSERT(lScratchOptions.mpOverhead != nullptr, "Out of memory\n");
        mScratchPool.Prepare();
        mScratchPool.InitPool(&lScratchOptions);
        for (s32 t = 0; t < 3; ++t)
        {
            rw::ResourceDescriptor lDescriptor = {};
            lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(AllocRequest) * KI_MAX_ALLOCATION_REQUESTS;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = alignof(AllocRequest);
            mAllocListSet.mapAllocRequests[t] = static_cast<AllocRequest*>(
                lpResourceAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0]);
            lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(AllocResult) * KI_MAX_ALLOCATION_REQUESTS;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = alignof(AllocResult);
            mAllocListSet.mapAllocResults[t] = static_cast<AllocResult*>(
                lpResourceAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0]);
            CGS_ASSERT(mAllocListSet.mapAllocRequests[t] != nullptr, "Out of memory\n");
            CGS_ASSERT(mAllocListSet.mapAllocResults[t] != nullptr, "Out of memory\n");
        }
        mAllocListSet.ClearCountsAndResults();

        // ARTIST 828FC0B8: request arrays, relocation/source arrays, a linear
        // heap with eight look-ahead records, and shared distribution/job storage.
        // FLAG PC-platform leaf: allocation counts are original; native records
        // widen their pointers and therefore require host sizeof/alignment.
        const auto lAllocateWorkingMemory = [lpResourceAllocator](size_t luSize, u32 luAlignment) -> void*
        {
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
            void* lpMemory = lpResourceAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0];
            CGS_ASSERT(lpMemory != nullptr, "Out of memory\n");
            return lpMemory;
        };
        mpAddressedAllocRequests = static_cast<AllocRequestAddressed*>(lAllocateWorkingMemory(
            sizeof(AllocRequestAddressed) * KI_MAX_ALLOCATION_REQUESTS, alignof(AllocRequestAddressed)));
        mpRelocateRequests = static_cast<RelocateRequest*>(lAllocateWorkingMemory(
            sizeof(RelocateRequest) * KU_MAX_POOL_ENTRIES, 128));
        mpLinearHeapNodes = static_cast<LinearHeapNode*>(lAllocateWorkingMemory(
            sizeof(LinearHeapNode) * (KU_MAX_LINEAR_HEAP_LENGTH + 8u), 128));
        const size_t luSharedStride = sizeof(DistributionEntry) > sizeof(RelocationEntry)
            ? sizeof(DistributionEntry) : sizeof(RelocationEntry);
        mpDistributionEntries = static_cast<DistributionEntry*>(lAllocateWorkingMemory(
            luSharedStride * KU_MAX_POOL_ENTRIES, 128));
        mpRelocationEntries = reinterpret_cast<RelocationEntry*>(mpDistributionEntries);
        mpRelocateSources = static_cast<RelocateSource*>(lAllocateWorkingMemory(
            sizeof(RelocateSource) * KU_MAX_POOL_ENTRIES, 128));
        mRelocationParams.mpOps = mpRelocationEntries;
        mRelocationParams.miNumOps = 0;
        mRelocationParams.mpBounceBuffer = lScratchOptions.mResource.m_baseResources[0];
        mRelocationParams.miBounceBufferSize = lScratchOptions.mDescriptor.m_baseResourceDescriptors[0].m_size;
        mProcessState = E_UPDATESTATE_IDLE;
        mAllocateState.Construct(this);
        mDeAllocateState.Construct(this);
        mIntelliFragState.Construct(this);
        mLiveUpdateState.Construct(this);
        mEmergencyFragState.Construct(this);
        mPendingAllocationRequests.Construct();


    }
    // The existing host update transport now drives the original typed request and
    // receiver processors, then the same state machines and owned maPools sweep.
    // ARTIST 829076D8: original state dispatch and live-pool sweep.
    PoolModule::EUpdateResult PoolModule::Update(
        CgsModule::IOBufferStack* /*lpInputStack*/, CgsModule::IOBufferStack* /*lpOutputStack*/,
        const PoolIO::InputBuffer* lpInput, PoolIO::OutputBuffer* lpOutput)
    {
        lpOutput->LockForWrite();
        lpInput->LockForRead();
        ProcessInputBuffer(lpInput, lpOutput);
        lpInput->UnlockForRead();
        ProcessReceiverQueue(lpOutput);
        if (Pool::_miPoolModuleStatePerfMon >= 0)
            CgsDev::PerfMonCpu::StartMonitor(Pool::_miPoolModuleStatePerfMon);
        if (mProcessState == E_UPDATESTATE_IDLE)
        {
            const s32 liMonitor = Pool::_miPoolModuleIdle;
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMonitor);
            Events::AllocateResourceListRequest lRequest;
            if (mPendingAllocationRequests.Pop(&lRequest))
                DoAllocateResourceListRequest(&lRequest);
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        }

        switch (mProcessState)
        {
        case E_UPDATESTATE_ALLOCATING_LIST:
        {
            const s32 liMonitor = Pool::_miPoolModuleAllocating;
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMonitor);
            UpdateAllocating(lpOutput);
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
            break;
        }
        case E_UPDATESTATE_DEALLOCATING_LIST:
        {
            const s32 liMonitor = Pool::_miPoolModuleDeallocating;
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMonitor);
            UpdateDeAllocating(lpOutput);
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
            break;
        }
        case E_UPDATESTATE_LIVEUPDATE:
        {
            const s32 liMonitor = Pool::_miPoolModuleLiveupdate;
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMonitor);
            UpdateLiveUpdate(lpOutput);
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
            break;
        }
        }

        if (mProcessState == E_UPDATESTATE_INTELLIFRAG)
        {
            const s32 liMonitor = Pool::_miPoolModuleIntellifrag;
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMonitor);
            UpdateIntelliFrag(lpOutput);
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        }
        if (mProcessState == E_UPDATESTATE_EMERGENCYFRAG)
        {
            const s32 liMonitor = Pool::_miPoolModuleEmergencyFrag;
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMonitor);
            UpdateEmergencyFrag(lpOutput);
            if (liMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        }
        if (Pool::_miPoolModuleStatePerfMon >= 0)
            CgsDev::PerfMonCpu::StopMonitor(Pool::_miPoolModuleStatePerfMon);
        lpOutput->UnlockForWrite();

        if (Pool::_miPoolUpdatePerfMon >= 0)
            CgsDev::PerfMonCpu::StartMonitor(Pool::_miPoolUpdatePerfMon);
        for (s32 liPool = 0; liPool < KI_MAX_POOLS; ++liPool)
            if (maPools[liPool].GetId() != -1)
                maPools[liPool].Update();
        if (Pool::_miPoolUpdatePerfMon >= 0)
            CgsDev::PerfMonCpu::StopMonitor(Pool::_miPoolUpdatePerfMon);
        return mProcessState == E_UPDATESTATE_EMERGENCYFRAG ? E_RESULT_REQUIRE_STALL : E_RESULT_OK;
    }


    bool PoolModule::Update(void* lpInputBuffer, void* lpOutputBuffer)
    {
        PoolIO::InputBuffer*  lpIn  = static_cast<PoolIO::InputBuffer*>(lpInputBuffer);
        PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);

        {
            renderengine::FrameProfile::CycleScope lProfile(renderengine::FrameProfile::POOL_REQUESTS);
            lpOut->LockForWrite();
            lpIn->LockForRead();
            ProcessInputBuffer(static_cast<const PoolIO::InputBuffer*>(lpIn), lpOut);
            lpIn->UnlockForRead();
            ProcessReceiverQueue(lpOut);
            if (mProcessState == E_UPDATESTATE_IDLE)
            {
                Events::AllocateResourceListRequest lRequest;
                if (mPendingAllocationRequests.Pop(&lRequest))
                    DoAllocateResourceListRequest(&lRequest);
            }
        }
        {
            renderengine::FrameProfile::CycleScope lProfile(renderengine::FrameProfile::POOL_STATES);
            switch (mProcessState)
            {
            case E_UPDATESTATE_ALLOCATING_LIST: UpdateAllocating(lpOut); break;
            case E_UPDATESTATE_DEALLOCATING_LIST: UpdateDeAllocating(lpOut); break;
            case E_UPDATESTATE_LIVEUPDATE: UpdateLiveUpdate(lpOut); break;
            }
            if (mProcessState == E_UPDATESTATE_INTELLIFRAG)
                UpdateIntelliFrag(lpOut);
            if (mProcessState == E_UPDATESTATE_EMERGENCYFRAG)
                UpdateEmergencyFrag(lpOut);
            lpOut->UnlockForWrite();
        }
        {
            renderengine::FrameProfile::CycleScope lProfile(renderengine::FrameProfile::POOL_RETIRE);
            for (s32 li = 0; li < KI_MAX_POOLS; ++li)
                if (maPools[li].GetId() != -1)
                    maPools[li].Update();
        }
        return mProcessState == E_UPDATESTATE_EMERGENCYFRAG;
    }







    // Look up a created pool by id (null if absent).
    Pool* PoolModule::GetPool(s32 liPoolId)
    {
        // FLAG PC-platform leaf: module-scheduled requests must see resources
        // loaded by the synchronous GUI transport under their original pool id.
        if (Pool* lpPool = PCPoolBindings::Find(liPoolId))
            return lpPool;
        const s32 li = GetPoolIndex(liPoolId);
        return (li >= 0) ? &maPools[li] : 0;
    }

    // @ 0x828FCD48 - acquire a single resource by id from its pool and reply with its handle. Find the pool
    // (GetPoolIndex), look up the resource (Pool::FindResource with status mask 2 = loaded), build the
    // resolved ResourceHandle pair (mpResourceMemory -> the entry's main-memory slot; mpSourceEntry -> the
    // entry) -- or both null if absent -- echo the request's reply target/ids, and post the response on the
    // pool output queue (tag 6). The X360 routes that response back to the requester via the shuttle.
    void PoolModule::DoAcquireResourceRequest(const Events::AcquireResourceRequest* lpRequest, PoolIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "lpOutputBuffer");

        Events::AcquireResourceResponse lResponse;
        static_cast<Events::PoolEvent&>(lResponse) = static_cast<const Events::PoolEvent&>(*lpRequest);   // echo user/id/pool
        lResponse.mResourceId      = lpRequest->mResourceId;
        lResponse.mpResourceMemory = 0;
        lResponse.mpSourceEntry    = 0;

        Pool* lpPool = GetPool(lpRequest->miPoolId);
        if (lpPool != nullptr)
        {
            // ARTIST828FCD88..C0 passes literal false and null; neither is request data.
            Entry* lpEntry = lpPool->FindResource(lpRequest->mResourceId, false, 2, 0);
            if (lpEntry != 0)
            {
                lResponse.mpResourceMemory = &lpEntry->mResource.m_baseResources[E_MEMTYPE_MAINMEMORY];
                lResponse.mpSourceEntry    = lpEntry;
            }
        }

        lpOutput->GetPoolOutputQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 6,
                                                 static_cast<s32>(sizeof(lResponse)));
    }

    // @ 0x828FCE40 - acquire EVERY resource named by a resource-ID-LIST resource into the caller's handle
    // array, then echo the request back with the count it filled. This is the protocol the world's
    // per-zone collision uses: WorldEntityModule::PrepareZoneCollision @0x82302C38 posts one
    // AcquireResourceListRequest per zone naming "TRK_CLIL<n>", and the reply's handles are what
    // AddCollisionZoneToSceneManager turns into InEventAddPolySoupList events.
    //
    // Faithful to the X360 spine, read from the ASM (the pseudocode fuses the 64-bit id across the
    // register pair and mis-attributes it as a status-mask argument -- both FindResource calls in fact
    // take r5=0 / r6=2 / r7=0, r6 being loaded ONCE at 0x828FCE98 and reused by the loop):
    //     listEntry = maPools[GetPoolIndex(req->miPoolId)].FindResource(req->mListResourceId, 0, 2, 0)
    //     ResourcePtr<ResourceIdList> ptr(listEntry's handle)
    //     count = ptr->GetNumIds();  assert(count <= req->miMaxHandles)
    //     for i: e = pool.FindResource(ptr->GetId(i), 0, 2, 0); req->mpHandles[i] = e's handle
    //     reply {req echoed, miNumHandles = count} on the pool output queue, tag 7, 32 bytes
    // The X360 re-runs GetPoolIndex inside the loop (0x828FD0A0); kept, since the pool table is
    // mutable across the call in principle and the cost is a 128-entry scan the console also pays.
    void PoolModule::DoAcquireResourceListRequest(const Events::AcquireResourceListRequest* lpRequest,
                                                  PoolIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "lpOutputBuffer");   // X360 CgsPoolModule.cpp:1371
        if (lpOutput == 0)
            return;

        // -- resolve the LIST resource itself -------------------------------------------------
        const s32 liListPoolIndex = GetPoolIndex(lpRequest->miPoolId);
        Entry* lpListEntry = (liListPoolIndex >= 0)
            ? maPools[liListPoolIndex].FindResource(lpRequest->mListResourceId, false, 2, 0)
            : 0;
        // X360 line 1377, message "Failed to find list resouce " << id [sic -- the console's own
        // spelling; the streamed id is dropped from the stringized condition per house convention].
        CGS_ASSERT(lpListEntry != 0, "Failed to find list resouce ");
        if (lpListEntry == 0)
            return;

        ResourceHandle lListHandle;
        lListHandle.mpResourceMemory = &lpListEntry->mResource.m_baseResources[E_MEMTYPE_MAINMEMORY];
        lListHandle.mpSourceEntry    = lpListEntry;
        ResourcePtr<ResourceIdList> lList(lListHandle);

        ResourceHandle* const lpaHandles = lpRequest->mpHandles;
        const u32 luNumIds = lList->GetNumIds();
        CGS_ASSERT(static_cast<s32>(luNumIds) <= lpRequest->miMaxHandles,
                   "Too many handles in list\n");      // X360 line 1388

        // -- resolve every listed resource into the caller's array ----------------------------
        for (u32 lu = 0; lu < luNumIds; ++lu)
        {
            const ID lId = lList->GetId(lu);           // bounds assert CgsResourceIdList.h:145

            const s32 liPoolIndex = GetPoolIndex(lpRequest->miPoolId);
            Entry* lpEntry = (liPoolIndex >= 0)
                ? maPools[liPoolIndex].FindResource(lId, false, 2, 0)
                : 0;
            // X360 line 1395, "Failed to find resouce <16 hex digits> from list\n" [sic].
            CGS_ASSERT(lpEntry != 0, "Failed to find resouce  from list\n");

            if (lpaHandles != 0 && lpEntry != 0)
            {
                lpaHandles[lu].mpResourceMemory = &lpEntry->mResource.m_baseResources[E_MEMTYPE_MAINMEMORY];
                lpaHandles[lu].mpSourceEntry    = lpEntry;
            }
        }

        // -- echo the request back, with the count it filled ----------------------------------
        Events::AcquireResourceListResponse lResponse;
        static_cast<Events::PoolEvent&>(lResponse) = static_cast<const Events::PoolEvent&>(*lpRequest);
        lResponse.mListResourceId = lpRequest->mListResourceId;
        lResponse.mpHandles       = lpaHandles;
        lResponse.miNumHandles    = static_cast<s32>(luNumIds);

        lpOutput->GetPoolOutputQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 7,
                                                 static_cast<s32>(sizeof(lResponse)));
    }





    // ARTIST 0x82906FD0. Dispatch each response with the output buffer that the completion
    // handlers use, then clear the queue after draining it. Invalid ids assert and continue.
    void PoolModule::ProcessReceiverQueue(PoolIO::OutputBuffer* lpOutputBuffer)
    {
        if (mReceiverQueue.GetCount() > 0)
        {
            const CgsModule::Event* lpEvent = nullptr;
            s32 liSize = 0;
            s32 liId = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            while (lpEvent != nullptr)
            {
                if (liId == 10)
                    DoCreatePoolRequest(
                        reinterpret_cast<const CgsMemory::MemoryIO::CreateResourceResponse*>(lpEvent),
                        lpOutputBuffer);
                else if (liId == 13)
                    DoDeletePoolRequest(
                        reinterpret_cast<const CgsMemory::MemoryIO::DestroyBankResponse*>(lpEvent),
                        lpOutputBuffer);
                else
                    FirePoolCreateAssert("Invalid Event Id.\n", 886);

                const CgsModule::Event* lpNext = nullptr;
                liId = mReceiverQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
        }
        mReceiverQueue.Clear();
    }

    // ARTIST 0x82904E00. One canonical raw-request dispatcher, including the
    // original invalidate/validate pool tags11/12 and event-zero CreatePoolRequest.
    void PoolModule::ProcessInputBuffer(const PoolIO::InputBuffer* lpInputBuffer,
                                       PoolIO::OutputBuffer* lpOutputBuffer)
    {
        if (lpInputBuffer == nullptr)
            FirePoolCreateAssert("lpBuffer", 1069);

        const PoolIO::InputBuffer::PoolInputQueue* lpQueue = lpInputBuffer->GetPoolInputQueue();
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liId = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != nullptr)
        {
            if (liId == 0)
                SendCreatePoolMemoryRequest(
                    reinterpret_cast<const Events::CreatePoolRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 2)
                SendDeletePoolMemoryRequest(
                    reinterpret_cast<const Events::DeletePoolRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 4)
                DoAcquireResourceRequest(
                    reinterpret_cast<const Events::AcquireResourceRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 5)
                DoAcquireResourceListRequest(
                    reinterpret_cast<const Events::AcquireResourceListRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 8)
                DoUnacquireResourceRequest(
                    reinterpret_cast<const Events::UnacquireResourceRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 10)
            {
                // ARTIST jump-table slot 10 targets the next-event advance directly.
            }
            else if (liId == 11)
                DoInvalidatePoolRequest(
                    reinterpret_cast<const Events::InvalidatePoolRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 12)
                DoValidatePoolRequest(
                    reinterpret_cast<const Events::ValidatePoolRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 16)
                mPendingAllocationRequests.Push(
                    reinterpret_cast<const Events::AllocateResourceListRequest*>(lpEvent));
            else if (liId == 18)
                DoFixUpAndResolveResourceListRequest(
                    reinterpret_cast<const Events::FixUpAndResolveResourceListRequest*>(lpEvent), lpOutputBuffer);
            else if (liId == 20)
                DoUnloadResourceListRequest(
                    reinterpret_cast<const Events::UnloadResourceListRequest*>(lpEvent), lpOutputBuffer);
            else
                FirePoolCreateAssert("Invalid Event Id.\n", 1155);

            const CgsModule::Event* lpNext = nullptr;
            liId = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST 0x828F3A50. The raw request is queued before the output assertion; Push's full result
    // is intentionally ignored. The conversion/name-bank ownership limits remain explicit at callers.
    void PoolModule::SendCreatePoolMemoryRequest(const Events::CreatePoolRequest* lpRequest,
                                                 PoolIO::OutputBuffer* lpPoolOutput)
    {
        mPendingCreateRequests.Push(lpRequest);
        if (lpPoolOutput == nullptr)
            FirePoolCreateAssert("lpOutputBuffer", 1183);

        rw::ResourceDescriptor lDescriptor;
        GetRequiredResourceDescriptor(lpRequest, &lDescriptor);
        // FLAG PC-platform leaf: native-pointer management records use the exact
        // shared layout that InitPool actually carves, rather than console byte
        // constants. Heap budgets and RW0/2 lane ownership remain the request's.
        Pool::InitOptions lHostOptions;
        ConvertPoolRequestOptions(lpRequest, &lHostOptions);
        lDescriptor.m_baseResourceDescriptors[0].m_size =
            lHostOptions.maHeapInfo[0].muHeapMemorySize + Pool::GetOverheadMemoryRequired(&lHostOptions);
        const u32 luManagementAlignment = Pool::GetOverheadMemoryAlignment(&lHostOptions);
        if (lDescriptor.m_baseResourceDescriptors[0].m_alignment < luManagementAlignment)
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luManagementAlignment;

        // Value initialization preserves the original strncpy32 zero-padded bank-name tail.
        CgsMemory::MemoryIO::CreateResourceRequest lRequest = {};
        lRequest.Construct(&mReceiverQueue, lpRequest->miPoolId);
        lRequest.SetBankName(lpRequest->mpcName);
        lRequest.SetBankId(lpRequest->miBankId);
        lRequest.SetParentBankId(lpRequest->miParentBankId);
        lRequest.SetDescriptor(&lDescriptor);
        lpPoolOutput->GetPoolResourceRequestQueue()->AddEvent(
            &lRequest, 10, static_cast<s32>(sizeof(lRequest)));
    }

    // ARTIST 0x82905000. The original conversion has no bank-id store. The current
    // host InitPool consumes an explicit bank owner, supplied below from the same
    // raw request whose memory allocation is completing; this is a PC adaptation.
    void PoolModule::DoCreatePoolRequest(const CgsMemory::MemoryIO::CreateResourceResponse* lpResponse,
                                         PoolIO::OutputBuffer* lpPoolOutput)
    {
        SmallResource lResource = {};
        lResource.CreateFromRWResource(lpResponse->GetResource());

        SmallResourceDescriptor lDescriptor = {};
        for (s32 li = 0; li < 3; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.CreateFromRWDescriptor(lpResponse->GetDescriptor());

        Events::CreatePoolRequest lPendingRequest;
        if (!mPendingCreateRequests.Pop(&lPendingRequest))
            FirePoolCreateAssert("Failed to pop from pending create request queue\n", 1270);

        Pool::InitOptions lOptions;
        ConvertPoolRequestOptions(&lPendingRequest, &lOptions);
        // FLAG PC-platform leaf: retain the actual completed allocation's bank
        // identity in the existing host InitPool provider. No default bank or
        // alternate owner is synthesized; MemoryModule allocated this exact id.
        lOptions.miBankId = lPendingRequest.miBankId;
        for (s32 li = 0; li < lOptions.miNumDependencies; ++li)
        {
            const s32 liDependencyId = lPendingRequest.maiDependencyIds[li];
            const s32 liPoolIndex = GetPoolIndex(liDependencyId);
            if (liDependencyId == lPendingRequest.miPoolId)
                FirePoolCreateAssert("Pool can not be dependent on itself!\n", 1280);
            if (liPoolIndex < 0)
                FireMissingDependencyAssert(liDependencyId);
            lOptions.mapDependencies[li] = &maPools[liPoolIndex];
        }
        lOptions.mResource = lResource;
        lOptions.mDescriptor = lDescriptor;

        CreatePool(&lOptions);
        if (lpPoolOutput == nullptr)
            FirePoolCreateAssert("lpOutputBuffer", 1297);

        Events::CreatePoolResponse lResponse;
        static_cast<Events::PoolEvent&>(lResponse) =
            static_cast<const Events::PoolEvent&>(lPendingRequest);
        lpPoolOutput->GetPoolOutputQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), 1,
            static_cast<s32>(sizeof(lResponse)));
    }

}
