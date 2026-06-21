#include "GameShared/GameClasses/Memory/CgsMemoryModule.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"   // MemoryIO request/response types (dispatch)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"       // CgsMemory::HeapMalloc::Destruct (destroy-allocator)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/rwcore_structs.h"                        // rw::Resource / ResourceDescriptor / IResourceAllocator_vtbl

// CgsMemory::MemoryModule -- see the header. This pass reconstructs the two pure stage-machine lifecycle
// bodies (Prepare 0x8286BF18 / Release 0x82866FF0); both are faithful ports of the X360 (single call
// chains the stages via fall-through, resuming at the current stage if a sub-step is not yet complete).
// Construct + the bank/block bring-up + the allocation/request handlers land in the next pass (they need
// the MemoryBank/MemoryBlock runtime record layouts, which are being recovered from the X360).

namespace CgsMemory
{
    // @ 0x82866F48 - resolve a bank index to its record. (X360 returns 84*index + mpBanks; here it's
    // the named array index. Asserts the sentinel index 255 is never passed.)
    MemoryBank* MemoryModule::GetBank(u8 lu8BankId)
    {
        CGS_ASSERT(lu8BankId != MemoryBank::KU_INVALID_BANK, "Bank index out of range");
        return &mpBanks[lu8BankId];
    }

    // Map a bank id to its bank-array index. [inferred: inlined in the X360 -- mpu8IdMap is the bank id ->
    // index table InitBanks fills with KU_INVALID_BANK(255); CreateBank/DestroyBank test the result == 255.]
    u8 MemoryModule::GetBankIndexFromId(u32 luBankId) const
    {
        CGS_ASSERT(luBankId <= muHighestId, "Bank id out of range");
        return mpu8IdMap[luBankId];
    }

    // @ 0x828670D0 - reset every bank record to "unused" + clear its id-map slot and name buffer.
    void MemoryModule::InitBanks()
    {
        for (u32 lb = 0; lb < muMaxBanks; ++lb)
        {
            mpBanks[lb].Clear();
            mpu8IdMap[lb]    = static_cast<u8>(MemoryBank::KU_INVALID_BANK);   // 0xFF
            mppcNames[lb][0] = 0;
        }
    }

    // @ 0x82869398 - thread every block into one doubly-linked free list (block i: prev=i-1, next=i+1;
    // ends terminated with KU_INVALID_BLOCK) and reset each block's bank/type/status fields.
    void MemoryModule::InitBlocks()
    {
        muFirstFreeBlock = 0;
        muNumFreeBlocks  = muMaxBlocks;
        for (u32 li = 0; li < muMaxBlocks; ++li)
        {
            mpBlocks[li].muPrevBlock   = li - 1;          // wraps to KU_INVALID_BLOCK for li == 0
            mpBlocks[li].muNextBlock   = li + 1;
            mpBlocks[li].mu8ParentBank = static_cast<u8>(MemoryBank::KU_INVALID_BANK);
            mpBlocks[li].mu8ChildBank  = static_cast<u8>(MemoryBank::KU_INVALID_BANK);
            mpBlocks[li].mu8Type       = 0;
            mpBlocks[li].mxStatus.Clear();
        }
        if (muMaxBlocks > 0)
        {
            mpBlocks[0].muPrevBlock                = MemoryBlock::KU_INVALID_BLOCK;
            mpBlocks[muMaxBlocks - 1].muNextBlock  = MemoryBlock::KU_INVALID_BLOCK;
        }
    }

    // @ 0x8286AF68 - set up bank 0 (the root) directly from the module's root memory regions (maRootSets).
    // The root has no parent, so unlike CreateBank it doesn't sub-allocate from a parent -- each type's
    // blocks come straight off the global free list, and the type's data pointer is the root region itself.
    void MemoryModule::CreateRootBank()
    {
        MemoryBank& lrRoot = mpBanks[0];
        lrRoot.SetID(0);                                  // root bank id = 0
        lrRoot.SetParent(static_cast<u8>(MemoryBank::KU_INVALID_BANK));   // no parent
        lrRoot.SetAllowFragmentation(true);              // X360: flags = (flags & 0xFC) | 1
        lrRoot.SetIsLeaf(false);
        mpu8IdMap[0] = 0;                                 // bank id 0 -> slot 0

        // name = "ROOT"
        const char* lpcName = "ROOT";
        char*       lpcDst  = mppcNames[0];
        s32 li = 0;
        for (; lpcName[li] != 0; ++li)
            lpcDst[li] = lpcName[li];
        lpcDst[li] = 0;

        for (s32 lt = 0; lt < MemoryBank::KI_NUM_TYPES; ++lt)
        {
            const RootResourceSet& lrRegion = maRootSets[lt];
            const u32 luNumBlocks = lrRegion.muNumBlocks;
            CGS_ASSERT(luNumBlocks != 0xFFFFu, "Root resource set not initialised");

            u32 luBlockSizeBytes = 0;
            if (luNumBlocks != 0)
            {
                CGS_ASSERT(lrRegion.muDataSize % luNumBlocks == 0, "Root region size not a multiple of block count");
                luBlockSizeBytes = lrRegion.muDataSize / luNumBlocks;
            }
            // [VERIFY granularity vs asm when run: block sizes are stored in 1KB units (>>10); the X360 also
            // asserts 1KB alignment and a max block size.]
            CGS_ASSERT((luBlockSizeBytes & 0x3FFu) == 0, "Root block size must be 1KB aligned");
            CGS_ASSERT((luBlockSizeBytes & 0xFFF00000u) <= 0x4000000u, "Root block size too large");

            lrRoot.SetNumBlocks(lt, static_cast<u16>(luNumBlocks));
            lrRoot.SetBlockSize(lt, static_cast<u16>(luBlockSizeBytes >> 10));

            if (luBlockSizeBytes != 0)
            {
                u32 luFirstBlock = 0;
                u32 luLastBlock  = 0;
                if (!AllocateBlocks(static_cast<u16>(luNumBlocks), 0, static_cast<u8>(lt), false,
                                    &luFirstBlock, &luLastBlock))
                {
                    CGS_ASSERT(false, "Root bank allocation failed");
                    return;
                }
                lrRoot.SetFirstBlock(lt, luFirstBlock);
                lrRoot.SetLastBlock(lt, luLastBlock);
                lrRoot.SetData(lt, lrRegion.mpDataStart);
            }
            else
            {
                lrRoot.SetFirstBlock(lt, MemoryBlock::KU_INVALID_BLOCK);
                lrRoot.SetLastBlock(lt, MemoryBlock::KU_INVALID_BLOCK);
                lrRoot.SetData(lt, 0);
            }
        }
    }

    // @ 0x8286C038 - create a bank from a Params request: validate the ids, find a free bank slot, compute
    // the per-type block request from the params + the parent's block granularity, sub-allocate it from the
    // parent (AllocateIntoBank), then write the new bank record + id-map slot + name. (a2 is a
    // MemoryBank::Params*; field offsets + the Params-pointer shape were confirmed from the X360 asm.)
    int MemoryModule::CreateBank(const MemoryBank::Params* lpParams)
    {
        const s32 liBankId   = lpParams->mnBankId;
        const s32 liParentId = lpParams->mnParentBankId;
        if (liBankId   < 0 || liBankId   >= static_cast<s32>(muMaxBanks) ||
            liParentId < 0 || liParentId >= static_cast<s32>(muMaxBanks))
            return 1;
        if (GetBankIndexFromId(static_cast<u32>(liParentId)) == MemoryBank::KU_INVALID_BANK)
            return 3;   // parent bank does not exist
        if (GetBankIndexFromId(static_cast<u32>(liBankId)) != MemoryBank::KU_INVALID_BANK)
            return 2;   // a bank with this id already exists

        // find a free bank slot
        u8 lu8Slot = 0;
        for (; lu8Slot < muMaxBanks; ++lu8Slot)
            if (mpBanks[lu8Slot].mId == -1)
                break;
        CGS_ASSERT(lu8Slot < muMaxBanks, "No free bank slots");

        const u8    lu8ParentIndex = GetBankIndexFromId(static_cast<u32>(liParentId));
        MemoryBank& lrParent       = mpBanks[lu8ParentIndex];

        // Build the per-type sub-allocation request. [VERIFY granularity when this first runs: the X360
        // stores block sizes in 1KB units (the <<10/>>10 below) -- counts = bytes / (parentBlockSize*1024),
        // new block size = (bytes/blockCount)/1024.]
        BankAllocRequest lRequest;
        lRequest.mu8NewBankSlot     = lu8Slot;
        lRequest.mu8ParentBankIndex = lu8ParentIndex;
        lRequest.mu8Flag            = lpParams->mbIsLeaf ? 1u : 0u;   // [VERIFY: AllocateBlocks mark-used flag]
        for (s32 lt = 0; lt < MemoryBank::KI_NUM_TYPES; ++lt)
        {
            const u32 luSize       = lpParams->mauBankSize[lt];
            const u32 luBlockCount = lpParams->mauBankBlocks[lt];
            if (luSize != 0)
            {
                if (luBlockCount == 0 || luSize % luBlockCount != 0)
                    return 7;   // bank size is not a multiple of the block count
                CGS_ASSERT(lrParent.GetBlockSize(lt) != 0, "Parent has no free blocks of this type");
                const u32 luNewBlockSize  = luSize / luBlockCount;                          // bytes per new block
                const u32 luParentGranule = static_cast<u32>(lrParent.GetBlockSize(lt)) << 10;
                lRequest.mau16Counts[lt]  = static_cast<u16>(luSize / luParentGranule);     // parent blocks needed
                lRequest.mau16Sizes[lt]   = static_cast<u16>(luNewBlockSize >> 10);         // new block size (1KB units)
            }
            else
            {
                lRequest.mau16Counts[lt] = 0;
                lRequest.mau16Sizes[lt]  = 0;
            }
        }

        const int liResult = AllocateIntoBank(&lRequest);
        if (liResult != 0)
            return liResult;

        CGS_ASSERT(liBankId >= 0 && static_cast<u32>(liBankId) <= muHighestId, "Bank id out of range");

        // commit the new bank record + publish it in the id map + copy its name
        MemoryBank& lrNew = mpBanks[lu8Slot];
        lrNew.SetID(static_cast<s16>(liBankId));
        lrNew.SetParent(lu8ParentIndex);
        lrNew.SetAllowFragmentation(lpParams->mbAllowFragmentation);
        lrNew.SetIsLeaf(lpParams->mbIsLeaf);
        mpu8IdMap[liBankId] = lu8Slot;

        char* lpcName = mppcNames[liBankId];
        s32 li = 0;
        for (; li < MemoryBank::KI_NAME_SIZE - 1 && lpParams->macName[li] != 0; ++li)
            lpcName[li] = lpParams->macName[li];
        lpcName[li] = 0;
        return 0;
    }

    // @ 0x8286DF98 - per-frame entry: write-lock the response buffer, read-lock the request buffer, drain
    // every inbound request through ProcessMemoryRequest, then unlock both.
    void MemoryModule::Update(CgsModule::IOBufferStack* /*lpInStack*/, CgsModule::IOBufferStack* /*lpOutStack*/,
                              const MemoryIO::InputBuffer* lpInput, MemoryIO::OutputBuffer* lpOutput)
    {
        lpOutput->LockForWrite();
        lpInput->LockForRead();

        const MemoryIO::InputBuffer::MemoryRequestQueue* lpQueue = lpInput->GetMemoryRequestQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            ProcessMemoryRequest(static_cast<const MemoryIO::MemoryRequest*>(lpEvent), lpOutput);
            const CgsModule::Event* lpNext = 0;
            liId = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }

        lpInput->UnlockForRead();
        lpOutput->UnlockForWrite();
    }

    // @ 0x8286DE90 - route one inbound memory request to its handler by event type. Each handler does the
    // work and appends a response onto lpOutput's response queue.
    void MemoryModule::ProcessMemoryRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput)
    {
        switch (lpRequest->GetEventType())
        {
        case MemoryIO::E_EVENT_TYPE_CREATE_BANK:                 ProcessCreateBankRequest(lpRequest, lpOutput);             break;
        case MemoryIO::E_EVENT_TYPE_DESTROY_BANK:                ProcessDestroyBankRequest(lpRequest, lpOutput);            break;
        case MemoryIO::E_EVENT_TYPE_CREATE_RW_LINEAR_ALLOCATOR:  ProcessCreateLinearAllocatorRequest(lpRequest, lpOutput);  break;
        case MemoryIO::E_EVENT_TYPE_CREATE_RW_GENERAL_ALLOCATOR: ProcessCreateGeneralAllocatorRequest(lpRequest, lpOutput); break;
        case MemoryIO::E_EVENT_TYPE_CREATE_ALLOCATOR:            ProcessCreateAllocatorRequest(lpRequest, lpOutput);        break;
        case MemoryIO::E_EVENT_TYPE_DESTROY_ALLOCATOR:           ProcessDestroyAllocatorRequest(lpRequest, lpOutput);       break;
        case MemoryIO::E_EVENT_TYPE_CREATE_RESOURCE:             ProcessCreateResourceRequest(lpRequest, lpOutput);         break;
        default:                                                 CGS_ASSERT(false, "Unhandled memory request type");       break;
        }
    }

    // [STUB - next MemoryModule pass] the per-type request handlers. Each builds its specific request from
    // lpRequest, performs the bank/allocator/resource operation, and appends a response to lpOutput's
    // response queue. Reconstructed next (they need the remaining request/response payload types); inert
    // stubs close the link (the dispatch is not driven yet -- nothing calls Update).
    // @ 0x8286CD20 - create the requested bank and reply with the result code on the response queue.
    void MemoryModule::ProcessCreateBankRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput)
    {
        const MemoryIO::CreateBankRequest* lpReq = static_cast<const MemoryIO::CreateBankRequest*>(lpRequest);
        MemoryIO::CreateBankResponse lResponse;
        lResponse.Construct(lpRequest->GetUser(), lpRequest->GetEventId());
        lResponse.SetResult(static_cast<MemoryIO::ResultCodes>(CreateBank(lpReq->GetParams())));
        lpOutput->GetMemoryResponseQueue()->AddEvent(&lResponse, lResponse.GetEventType(),
                                                     static_cast<s32>(sizeof(lResponse)));
    }

    // @ 0x8286D970 - destroy the requested bank and reply with the result code.
    void MemoryModule::ProcessDestroyBankRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput)
    {
        const MemoryIO::DestroyBankRequest* lpReq = static_cast<const MemoryIO::DestroyBankRequest*>(lpRequest);
        MemoryIO::DestroyBankResponse lResponse;
        lResponse.Construct(lpRequest->GetUser(), lpRequest->GetEventId());
        // X360 passes the non-leaf/validate mode (a3 = 0) here.
        lResponse.SetResult(static_cast<MemoryIO::ResultCodes>(DestroyBank(lpReq->GetBankId(), false)));
        lpOutput->GetMemoryResponseQueue()->AddEvent(&lResponse, lResponse.GetEventType(),
                                                     static_cast<s32>(sizeof(lResponse)));
    }
    // [DEFERRED with the rwcore allocator middleware] the rw-allocator-CREATING handlers (Linear/General/
    // Resource). Shape decoded: build a CreateBankRequest from the request's rw::ResourceDescriptor ->
    // CreateBank -> create the rw allocator over the bank's data via rw::LinearResourceAllocator::
    // GetResourceDescriptor/Initialize or rw::core::GeneralResourceAllocator::Initialize -> return the
    // allocator in the response. Blocked on the rwcore rw::allocator layer: rwcore.lib is NOT linked into
    // build_game_exe.bat and those factory methods aren't reconstructed (same situation as the EA PPMalloc
    // HeapMalloc body). Kept as inert stubs (the dispatch isn't driven until the GameDataModule runs).
    void MemoryModule::ProcessCreateLinearAllocatorRequest(const MemoryIO::MemoryRequest*, MemoryIO::OutputBuffer*) {}
    void MemoryModule::ProcessCreateGeneralAllocatorRequest(const MemoryIO::MemoryRequest*, MemoryIO::OutputBuffer*) {}
    // @ 0x8286D9F0 - create a bank sized for the caller's allocator, then construct that (caller-supplied)
    // heap or linear allocator over the new bank's memory. Does NOT touch the AllocatorList (the caller owns
    // the allocator object). Field accessors + the round-up-to-parent-granule sizing are from the X360.
    void MemoryModule::ProcessCreateAllocatorRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput)
    {
        const MemoryIO::CreateAllocatorRequest* lpReq = static_cast<const MemoryIO::CreateAllocatorRequest*>(lpRequest);
        MemoryIO::CreateAllocatorResponse lResponse;
        lResponse.Construct(lpRequest->GetUser(), lpRequest->GetEventId());

        const s32 liBankId   = lpReq->GetBankId();
        const s32 liParentId = lpReq->GetParentBankId();
        const s32 liType     = lpReq->GetResourceType();
        const s32 liSize     = lpReq->GetSize();

        MemoryIO::ResultCodes leResult;
        if (liParentId < 0 || liParentId >= static_cast<s32>(muMaxBanks))
        {
            CGS_ASSERT(false, "Parent bank id out of range");
            leResult = MemoryIO::E_RESULT_BANK_OUT_OF_RANGE;
        }
        else if (GetBankIndexFromId(static_cast<u32>(liParentId)) == MemoryBank::KU_INVALID_BANK)
        {
            CGS_ASSERT(false, "Parent bank does not exist");
            leResult = MemoryIO::E_RESULT_BANK_ID_NOT_FOUND;
        }
        else
        {
            const MemoryBank& lrParent = mpBanks[GetBankIndexFromId(static_cast<u32>(liParentId))];
            // round the requested size up to the parent's block granularity (1KB units -> <<10). [VERIFY]
            const u32 luGranule  = static_cast<u32>(lrParent.GetBlockSize(liType)) << 10;
            const u32 luBankSize = luGranule != 0 ? (((static_cast<u32>(liSize) - 1) / luGranule + 1) * luGranule)
                                                  : static_cast<u32>(liSize);

            // build a one-type leaf bank for the allocator and create it
            MemoryBank::Params lParams;
            for (s32 lt = 0; lt < 6; ++lt) { lParams.mauBankSize[lt] = 0; lParams.mauBankBlocks[lt] = 0; }
            const char* lpcName = lpReq->GetBankName();
            s32 li = 0;
            for (; li < MemoryBank::KI_NAME_SIZE - 1 && lpcName[li] != 0; ++li) lParams.macName[li] = lpcName[li];
            lParams.macName[li]            = 0;
            lParams.mnBankId               = liBankId;
            lParams.mnParentBankId         = liParentId;
            lParams.mauBankSize[liType]    = luBankSize;
            lParams.mauBankBlocks[liType]  = 1;
            lParams.mbIsLeaf               = true;
            lParams.mbAllowFragmentation   = false;

            leResult = static_cast<MemoryIO::ResultCodes>(CreateBank(&lParams));
            if (leResult == MemoryIO::E_RESULT_OK)
            {
                void* lpData = mpBanks[GetBankIndexFromId(static_cast<u32>(liBankId))].GetData(liType);
                if (lpReq->GetHeapAllocator() != 0)
                {
                    lpReq->GetHeapAllocator()->Construct(lpData, liSize);
                }
                else if (lpReq->GetLinearAllocator() != 0)
                {
                    lpReq->GetLinearAllocator()->Construct();
                    lpReq->GetLinearAllocator()->Create(lpData, static_cast<size_t>(liSize));
                }
                else
                {
                    CGS_ASSERT(false, "Allocator not set");
                    leResult = MemoryIO::E_RESULT_ALLOCATOR_NOT_SET;
                }
            }
        }

        lResponse.SetResult(leResult);
        lpOutput->GetMemoryResponseQueue()->AddEvent(&lResponse, lResponse.GetEventType(),
                                                     static_cast<s32>(sizeof(lResponse)));
    }
    // @ 0x8286DD40 - tear down an allocator: destroy its (leaf) bank, then shut down the heap or linear
    // allocator that backed it, and reply with the result.
    void MemoryModule::ProcessDestroyAllocatorRequest(const MemoryIO::MemoryRequest* lpRequest, MemoryIO::OutputBuffer* lpOutput)
    {
        const MemoryIO::DestroyAllocatorRequest* lpReq = static_cast<const MemoryIO::DestroyAllocatorRequest*>(lpRequest);
        MemoryIO::DestroyAllocatorResponse lResponse;
        lResponse.Construct(lpRequest->GetUser(), lpRequest->GetEventId());

        MemoryIO::ResultCodes leResult;
        if (lpReq->GetHeapAllocator() != 0 || lpReq->GetLinearAllocator() != 0)
        {
            leResult = static_cast<MemoryIO::ResultCodes>(DestroyBank(lpReq->GetBankId(), true));   // leaf mode
            if (leResult == MemoryIO::E_RESULT_OK)
            {
                if (lpReq->GetHeapAllocator() != 0)
                    lpReq->GetHeapAllocator()->Destruct();        // X360: GeneralAllocator::Shutdown(heap's allocator)
                else
                    lpReq->GetLinearAllocator()->Destruct();
            }
        }
        else
        {
            CGS_ASSERT(false, "Allocator not set");
            leResult = MemoryIO::E_RESULT_ALLOCATOR_NOT_SET;
        }

        lResponse.SetResult(leResult);
        lpOutput->GetMemoryResponseQueue()->AddEvent(&lResponse, lResponse.GetEventType(),
                                                     static_cast<s32>(sizeof(lResponse)));
    }
    // @ 0x8286CDB8 - [DEFERRED with the rwcore allocator middleware] see the note above
    // ProcessCreateLinearAllocatorRequest. Real body builds a CreateBankRequest from the request's
    // rw::ResourceDescriptor (rw::LinearResourceAllocator::GetResourceDescriptor), CreateBank's it, then
    // Initialize's the rw allocator over the new bank and returns it. Inert stub until rwcore.lib + that
    // rw::allocator factory layer are in the build.
    void MemoryModule::ProcessCreateResourceRequest(const MemoryIO::MemoryRequest*, MemoryIO::OutputBuffer*) {}

    // @ 0x8286ACA8 - one-time construction: carve a single backing block from the resource allocator,
    // lay a bump-allocator (mScratchSpace) over it, and sub-allocate the bank / block / id-map / name
    // tables. The actual bank/block records are zeroed later by InitBanks/InitBlocks (in Prepare).
    // Zero-init so unused root resource sets read muNumBlocks=0 (skipped by CreateRootBank, not the
    // 0xFFFF "not initialised" marker). The caller fills the active counts + regions.
    MemoryModule::InitOptions::InitOptions()
        : muMaxBanks(0), muHighestId(0), muMaxBlocks(0)
    {
        for (s32 li = 0; li < 6; ++li)
        {
            maResourceSets[li].mpDataStart = 0;
            maResourceSets[li].muDataSize  = 0;
            maResourceSets[li].muNumBlocks = 0;
        }
    }

    void MemoryModule::Construct(InitOptions* lpInitOptions, rw::IResourceAllocator* lpAllocator)
    {
        ModuleSingleBuffered::Construct();
        meReleaseStage = E_RELEASE_DONE;
        mePrepareStage = E_PREPARE_START;

        CGS_ASSERT(lpInitOptions->muMaxBanks <= 0xFFu,
                   "For memory foot print reasons you can't have more than 255 simultaneous banks");
        CGS_ASSERT(lpInitOptions->muHighestId <= 0x800u,
                   "You have more than 2048 potential bank ids - this is possible but INSANE!");

        const u32 luMaxBanks  = lpInitOptions->muMaxBanks;
        const u32 luMaxBlocks = lpInitOptions->muMaxBlocks;
        const u32 luHighestId = lpInitOptions->muHighestId;

        // Total the bump-allocator must hold: the bank + block + id-map + name-pointer arrays, plus a
        // 32-byte name buffer per bank. (X360 packs the same sum with its 32-bit struct sizes; here the
        // x64 sizeofs are used so the tables fit our layout.)
        const u32 luTotalSize = static_cast<u32>(sizeof(MemoryBank))  * luMaxBanks
                              + static_cast<u32>(sizeof(MemoryBlock)) * luMaxBlocks
                              + luHighestId
                              + static_cast<u32>(sizeof(char*))       * luMaxBanks
                              + 32u * luMaxBanks;

        // Carve the backing block from the resource allocator via its virtual DoAllocate (the engine
        // holds the abstract rw::IResourceAllocator*; the game supplies a concrete one). Descriptor =
        // {m_size = total, m_alignment = 16} on pool 0; the returned Resource's main-memory pointer is
        // the backing block.
        rw::ResourceDescriptor lDescriptor;
        for (u32 li = 0; li < 4; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luTotalSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;

        rw::Resource lBacking = lpAllocator->DoAllocate(lDescriptor, 0);

        mScratchSpace.Create(lBacking.m_baseResources[0], luTotalSize);
        mpBanks   = static_cast<MemoryBank*>(mScratchSpace.Malloc(sizeof(MemoryBank)  * luMaxBanks));
        mpBlocks  = static_cast<MemoryBlock*>(mScratchSpace.Malloc(sizeof(MemoryBlock) * luMaxBlocks));
        mpu8IdMap = static_cast<u8*>(mScratchSpace.Malloc(luHighestId));
        mppcNames = static_cast<char**>(mScratchSpace.Malloc(sizeof(char*) * luMaxBanks));
        for (u32 lb = 0; lb < luMaxBanks; ++lb)
            mppcNames[lb] = static_cast<char*>(mScratchSpace.Malloc(32));

        muMaxBanks  = luMaxBanks;
        muMaxBlocks = luMaxBlocks;
        muHighestId = luHighestId;

        // Copy the caller's root memory regions (X360 copies KI_NUM_TYPES of them; the table holds 6).
        for (u32 ls = 0; ls < 6; ++ls)
            maRootSets[ls] = lpInitOptions->maResourceSets[ls];
    }

    // @ 0x8286B3B8 - sub-allocate a new bank's block ranges out of its parent. Two passes over the 5
    // resource types: (1) for each requested type, compute how many new-bank blocks the request needs
    // (parentBlockSize * count / newBlockSize) and find a contiguous run of `count` free parent blocks;
    // (2) once everything fits, mark each parent run as owned by the child and allocate the new bank's
    // blocks, filling the child's resource set (block size / count / first+last block / data pointer).
    // Search start, counts, and the allowFrag flag were pinned from the X360 asm (the pseudocode elides
    // the FindContiguousFreeBlocks out-pointers). Returns 0 on success or an error code.
    int MemoryModule::AllocateIntoBank(BankAllocRequest* lpRequest)
    {
        CGS_ASSERT(lpRequest != 0, "Invalid bank allocation request");

        const u8 lu8Parent  = lpRequest->mu8ParentBankIndex;
        CGS_ASSERT(lu8Parent < muMaxBanks && mpBanks[lu8Parent].mId != -1, "Parent bank must be valid");
        const u8 lu8NewSlot = lpRequest->mu8NewBankSlot;
        CGS_ASSERT(lu8NewSlot < muMaxBanks && mpBanks[lu8NewSlot].mId == -1, "New bank slot must be free");

        MemoryBank& lrParent  = mpBanks[lu8Parent];
        const bool  lbAllowFrag = (lrParent.mu8Flags & MemoryBank::KU_FLAG_ALLOW_FRAGMENTATION) != 0;

        // Per-type results carried from pass 1 to pass 2.
        u32 lauRunStartIndex[MemoryBank::KI_NUM_TYPES];
        u32 lauRunStartBlock[MemoryBank::KI_NUM_TYPES];
        u32 lauScanIndex[MemoryBank::KI_NUM_TYPES];
        u32 lauRunEndBlock[MemoryBank::KI_NUM_TYPES];
        u32 lauNumBlocksNeeded[MemoryBank::KI_NUM_TYPES];
        u32 luTotalBlocksNeeded = 0;

        // Pass 1: size each type + locate a free run in the parent.
        for (s32 lt = 0; lt < MemoryBank::KI_NUM_TYPES; ++lt)
        {
            lauRunStartBlock[lt]   = MemoryBlock::KU_INVALID_BLOCK;
            lauRunStartIndex[lt]   = 0;
            lauScanIndex[lt]       = 0;
            lauRunEndBlock[lt]     = MemoryBlock::KU_INVALID_BLOCK;
            lauNumBlocksNeeded[lt] = 0;

            const u16 lu16Count = lpRequest->mau16Counts[lt];
            if (lu16Count == 0)
                continue;

            const u16 lu16NewBlockSize = lpRequest->mau16Sizes[lt];
            if (lu16NewBlockSize == 0)
                return 7;   // a requested type with zero block size

            const u32 luParentBlockSize = lrParent.GetBlockSize(lt);
            const u32 luBytes           = luParentBlockSize * lu16Count;
            if (luBytes % lu16NewBlockSize != 0)
                return 5;   // the parent allocation does not divide evenly into new-bank blocks

            lauNumBlocksNeeded[lt] = luBytes / lu16NewBlockSize;
            luTotalBlocksNeeded   += lauNumBlocksNeeded[lt];

            // Search the parent's block range (from its first block of this type) for `count` contiguous
            // free parent blocks. (asm: numWanted = count, start = parent.GetFirstBlock, frag = parent flag&1)
            if (!FindContiguousFreeBlocks(lu16Count, lrParent.GetFirstBlock(lt), lbAllowFrag,
                                          &lauRunStartIndex[lt], &lauRunStartBlock[lt],
                                          &lauScanIndex[lt], &lauRunEndBlock[lt]))
                return 4;   // no contiguous run available
        }

        if (luTotalBlocksNeeded > muNumFreeBlocks)
            return 6;

        // Pass 2: commit each type -- reserve the parent run for the child, allocate the child's blocks,
        // and populate the child bank's resource set.
        MemoryBank& lrNew = mpBanks[lu8NewSlot];
        for (s32 lt = 0; lt < MemoryBank::KI_NUM_TYPES; ++lt)
        {
            const u16 lu16Count = lpRequest->mau16Counts[lt];
            if (lu16Count == 0)
                continue;

            MarkRangeAsUsed(lu8NewSlot, lauRunStartBlock[lt], lu16Count);

            u32 luFirstBlock = 0;
            u32 luLastBlock  = 0;
            const u32 luNumBlocks = lauNumBlocksNeeded[lt];
            if (!AllocateBlocks(static_cast<u16>(luNumBlocks), lu8NewSlot, static_cast<u8>(lt),
                                lpRequest->mu8Flag != 0, &luFirstBlock, &luLastBlock))
                return 6;

            lrNew.SetBlockSize(lt, lpRequest->mau16Sizes[lt]);
            lrNew.SetNumBlocks(lt, static_cast<u16>(luNumBlocks));
            lrNew.SetFirstBlock(lt, luFirstBlock);
            lrNew.SetLastBlock(lt, luLastBlock);
            // Child data = the parent's data offset by where the run starts. [VERIFY vs asm when this first
            // runs: the X360 computes (runStartIndex * parentBlockSize) << 10 + parent data; the <<10 is the
            // parent's block-size granularity and the exact run-output used is inferred.]
            void* lpParentData = lrParent.GetData(lt);
            const u32 luOffset = (lauRunStartIndex[lt] * lrParent.GetBlockSize(lt)) << 10;
            lrNew.SetData(lt, static_cast<char*>(lpParentData) + luOffset);
        }

        return 0;
    }

    // @ 0x82869880 - allocate luCount blocks off the FRONT of the global free list: tag each block with
    // its parent bank + type, optionally mark it used, then unlink the run and return its first/last
    // index. Fails (false, outputs = invalid) if there aren't enough free blocks.
    bool MemoryModule::AllocateBlocks(u16 lu16Count, u8 lu8BankId, u8 lu8Type, bool lbMarkUsed,
                                      u32* lpuOutFirstBlock, u32* lpuOutLastBlock)
    {
        CGS_ASSERT(lpuOutFirstBlock != 0 && lpuOutLastBlock != 0, "Invalid First/Last block output pointers");
        CGS_ASSERT(lu8BankId < 0xFFu, "Invalid bank index");

        if (lu16Count > muNumFreeBlocks)
        {
            *lpuOutFirstBlock = MemoryBlock::KU_INVALID_BLOCK;
            *lpuOutLastBlock  = MemoryBlock::KU_INVALID_BLOCK;
            return false;
        }

        u32 luBlock = muFirstFreeBlock;
        u32 luFirst = luBlock;
        u32 luLast  = luBlock;
        for (u16 lu = 0; lu < lu16Count; ++lu)
        {
            CGS_ASSERT(luBlock != MemoryBlock::KU_INVALID_BLOCK,
                       "Block layout has become corrupt - free list does not match free count");
            MemoryBlock& lrBlock  = mpBlocks[luBlock];
            lrBlock.mu8ChildBank  = static_cast<u8>(MemoryBank::KU_INVALID_BANK);
            lrBlock.mu8ParentBank = lu8BankId;
            lrBlock.mu8Type       = lu8Type;
            lrBlock.mxStatus.Clear();
            if (lbMarkUsed)
                lrBlock.mxStatus.SetBit(1);
            luLast  = luBlock;
            luBlock = lrBlock.muNextBlock;
        }

        // luBlock is now the next free block after the run; detach [luFirst..luLast] from the free list.
        muFirstFreeBlock              = luBlock;
        mpBlocks[luFirst].muPrevBlock = MemoryBlock::KU_INVALID_BLOCK;
        mpBlocks[luLast].muNextBlock  = MemoryBlock::KU_INVALID_BLOCK;
        if (luBlock != MemoryBlock::KU_INVALID_BLOCK)   // (X360 reads 255 here -- decompiler artifact for -1)
            mpBlocks[luBlock].muPrevBlock = MemoryBlock::KU_INVALID_BLOCK;

        muNumFreeBlocks -= lu16Count;
        *lpuOutFirstBlock = luFirst;
        *lpuOutLastBlock  = luLast;
        return true;
    }

    // @ 0x82869428 - destroy a bank. lbLeafMode asserts the bank is a leaf; otherwise it must be a
    // non-leaf and is first verified to hold no live allocations. Then (both modes) each type's block run
    // is returned to the free list and the parent's child-markings for those blocks are restored (a freed
    // block becomes fully free when the whole parent run is now free, else fragmentation-locked). Finally
    // the bank record is cleared. Field offsets + the all-clear/frag rule were verified from the X360 asm.
    int MemoryModule::DestroyBank(s32 liBankId, bool lbLeafMode)
    {
        if (liBankId < 0 || liBankId >= static_cast<s32>(muMaxBanks))
        {
            CGS_ASSERT(false, "Bank id out of range");
            return 1;
        }
        const u8    lu8Index  = GetBankIndexFromId(static_cast<u32>(liBankId));
        MemoryBank& lrBank    = mpBanks[lu8Index];
        const u8    lu8Parent = lrBank.GetParent();
        if (lu8Parent == static_cast<u8>(MemoryBank::KU_INVALID_BANK))
        {
            CGS_ASSERT(false, "Cannot destroy a bank with no parent (the root)");
            return 8;
        }

        if (lbLeafMode)
        {
            if (!lrBank.GetIsLeaf()) { CGS_ASSERT(false, "Bank is not a leaf bank"); return 12; }
        }
        else
        {
            if (lrBank.GetIsLeaf()) { CGS_ASSERT(false, "Bank is a leaf bank"); return 11; }

            // non-leaf: it must hold no live allocations (its children must already be destroyed)
            for (s32 lt = 0; lt < MemoryBank::KI_NUM_TYPES; ++lt)
            {
                if (lrBank.GetNumBlocks(lt) == 0)
                    continue;
                u32 luBlock = lrBank.GetFirstBlock(lt);
                while (luBlock != MemoryBlock::KU_INVALID_BLOCK)
                {
                    if (mpBlocks[luBlock].mxStatus.IsBitSet(1))
                    {
                        CGS_ASSERT(false, "Can not destroy non-empty bank");
                        return 9;
                    }
                    luBlock = mpBlocks[luBlock].muNextBlock;
                }
            }
        }

        MemoryBank& lrParent = mpBanks[lu8Parent];
        for (s32 lt = 0; lt < MemoryBank::KI_NUM_TYPES; ++lt)
        {
            if (lrBank.GetNumBlocks(lt) == 0)
                continue;

            // Restore the parent's blocks for this type: walk them (from the parent's last block, backward
            // via muPrevBlock) clearing this bank's child ownership. A block freed by us becomes fully free
            // only while the whole run so far is free (lbAllClear); once another child's block is seen the
            // remaining freed blocks are fragmentation-locked (status bit 2).
            bool lbAllClear = true;
            u32  luBlock    = lrParent.GetLastBlock(lt);
            while (luBlock != MemoryBlock::KU_INVALID_BLOCK)
            {
                MemoryBlock& lrBlk = mpBlocks[luBlock];
                if (lrBlk.mu8ChildBank == lu8Index)
                {
                    lrBlk.mu8ChildBank = static_cast<u8>(MemoryBank::KU_INVALID_BANK);
                    lrBlk.mxStatus.Clear();
                    if (!lbAllClear)
                        lrBlk.mxStatus.SetBit(2);   // fragmentation-locked
                }
                else if (lrBlk.mu8ChildBank == static_cast<u8>(MemoryBank::KU_INVALID_BANK))
                {
                    if (lbAllClear)
                        lrBlk.mxStatus.Clear();
                }
                else
                {
                    lbAllClear = false;   // owned by another child -> the run is fragmented
                }
                luBlock = lrBlk.muPrevBlock;
            }

            // Splice this bank's [first..last] run onto the front of the free list.
            const u32 luFirst = lrBank.GetFirstBlock(lt);
            const u32 luLast  = lrBank.GetLastBlock(lt);
            mpBlocks[luLast].muNextBlock = muFirstFreeBlock;
            if (muFirstFreeBlock != MemoryBlock::KU_INVALID_BLOCK)   // (X360 writes unconditionally; guarded here)
                mpBlocks[muFirstFreeBlock].muPrevBlock = luLast;
            muFirstFreeBlock = luFirst;
            muNumFreeBlocks += lrBank.GetNumBlocks(lt);
        }

        mpu8IdMap[liBankId] = static_cast<u8>(MemoryBank::KU_INVALID_BANK);
        lrBank.Clear();
        return 0;
    }

    // @ 0x82869B48 - walk the free list from luStartBlock looking for a run of luNumWanted contiguous
    // blocks that are free (status bit 1 = allocated, bit 2 = fragmentation-locked; a frag-locked block
    // counts as available only when lbAllowFragmented). A used block resets the run, restarting it at the
    // following block. Returns true with the run's start/end (and the scan counters the caller carries on
    // with) once a long enough run is found.
    bool MemoryModule::FindContiguousFreeBlocks(u16 lu16NumWanted, u32 luStartBlock, bool lbAllowFragmented,
                                                u32* lpuOutRunStartIndex, u32* lpuOutRunStartBlock,
                                                u32* lpuOutScanIndex, u32* lpuOutRunEndBlock)
    {
        if (luStartBlock == MemoryBlock::KU_INVALID_BLOCK)
            return false;

        u32 luBlock        = luStartBlock;   // block currently examined
        u32 luRunStartIndex = 0;             // scan index where the current run begins
        u32 luRunStartBlock = luStartBlock;  // block index where the current run begins
        u32 luContiguous    = 0;             // length of the current free run
        u32 luScanIndex     = 0;             // total blocks scanned
        do
        {
            const MemoryBlock& lrBlock = mpBlocks[luBlock];
            const bool lbUsed = lrBlock.mxStatus.IsBitSet(1) ||
                                (lrBlock.mxStatus.IsBitSet(2) && !lbAllowFragmented);
            if (lbUsed)
            {
                // run broken -- restart it at the next block
                luBlock         = lrBlock.muNextBlock;
                ++luScanIndex;
                luContiguous    = 0;
                luRunStartIndex = luScanIndex;
                luRunStartBlock = luBlock;
            }
            else
            {
                if (++luContiguous == lu16NumWanted)
                {
                    *lpuOutRunStartIndex = luRunStartIndex;
                    *lpuOutRunStartBlock = luRunStartBlock;
                    *lpuOutScanIndex     = luScanIndex;
                    *lpuOutRunEndBlock   = luBlock;
                    return true;
                }
                luBlock = lrBlock.muNextBlock;
                ++luScanIndex;
            }
        } while (luBlock != MemoryBlock::KU_INVALID_BLOCK);

        return false;
    }

    // @ 0x82869C00 - mark luCount already-linked blocks (from luStartBlock, following muNextBlock) as
    // allocated to a child bank: stamp each block's child-bank id and set its allocated status bit.
    void MemoryModule::MarkRangeAsUsed(u8 lu8ChildBankId, u32 luStartBlock, u32 luCount)
    {
        CGS_ASSERT(lu8ChildBankId < 0xFFu, "Invalid bank index");
        u32 luBlock = luStartBlock;
        while (luCount != 0)
        {
            CGS_ASSERT(luBlock != MemoryBlock::KU_INVALID_BLOCK, "Can't mark invalid block as allocated");
            CGS_ASSERT(!mpBlocks[luBlock].mxStatus.IsBitSet(1), "Block is already allocated");
            --luCount;
            mpBlocks[luBlock].mu8ChildBank = lu8ChildBankId;
            mpBlocks[luBlock].mxStatus.Clear();
            mpBlocks[luBlock].mxStatus.SetBit(1);
            luBlock = mpBlocks[luBlock].muNextBlock;
        }
    }

    // @ 0x8286BF18 - bring the memory manager up, stage by stage: run the base module Prepare, then
    // initialise the bank + block records and create the root bank. Chained in one call; if the base
    // Prepare is not ready it returns false with the stage parked so the next call resumes there.
    bool MemoryModule::Prepare()
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_MANAGER:
            mePrepareStage = E_PREPARE_MANAGER;
            if (!ModuleSingleBuffered::Prepare())
                return false;
            // fall through -- base module is prepared, initialise memory
        case E_PREPARE_INIT_MEMORY:
            mePrepareStage = E_PREPARE_INIT_MEMORY;
            // [PC incremental] Construct allocates the bank/block arrays (mpBanks) from the resource
            // allocator; until that bring-up is wired, skip bank init so the (still-stubbed-Construct)
            // module's Prepare runs harmlessly instead of dereferencing a null mpBanks. Becomes a no-op
            // guard automatically once Construct sets mpBanks.
            if (mpBanks != 0)
            {
                InitBanks();
                InitBlocks();
                CreateRootBank();
            }
            // fall through -- done
        case E_PREPARE_DONE:
            meReleaseStage = E_RELEASE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x82866FF0 - tear the memory manager down: run the base module Release, then reset the stages.
    // Idempotent once at DONE. Resumable like Prepare.
    bool MemoryModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
        case E_RELEASE_MANAGER:
            meReleaseStage = E_RELEASE_MANAGER;
            if (!ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_RELEASE_DONE:
            mePrepareStage = E_PREPARE_START;
            meReleaseStage = E_RELEASE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // No standalone X360 export (inlined/defaulted): tear down the base module. The bank/block
    // teardown runs through Release; nothing extra to release here for the (scratch-backed) module.
    void MemoryModule::Destruct()
    {
        ModuleSingleBuffered::Destruct();
    }
}
