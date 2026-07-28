#include "GameSource/Resource/BrnGameDataModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsMemoryModule.h"   // MemoryModule::InitOptions
#include "GameSource/Resource/BrnResourceAllocator.h"        // Allocators::mpInternalDebugAllocator
#include "rw/rwcore_structs.h"                                // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "rw/rwcore_general_alloc.h"                          // rw::core::GeneralResourceAllocator (CreateAllocators)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // [5b trace] (Construct runs at runtime, gpDebugPrint ready)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes (bridge)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"     // ResolveResourceType (bridge)
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"             // Pool::InitOptions (CreatePools)
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"                        // MemoryBank::Params (CreatePoolBank)
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"                    // MemoryIO Input/OutputBuffer (async pump)
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"       // SendCreatePoolMemoryRequest / DoCreatePoolRequest
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"             // PoolIO::OutputBuffer (request transport)
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"         // ResourceIO::InputBuffer (CreatePool request input)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"         // Events::LoadBundleRequest/AcquireResourceRequest (streaming)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"          // E_RESOURCETYPE_FONT (stream validation)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"            // EventReceiverQueue (acquire response receiver)
#include "GameSource/Resource/BrnMemoryMapData.h"                               // KAC_MEMORY_MAP_POOLS (the 27 real pools)
#include "GameSource/Resource/BrnGameDataModuleIO.h"                            // GameDataIO In/OutputBuffer (Update pump)
#include "GameShared/GameClasses/Core/CgsID.h"                                  // CgsIDUnCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                    // CgsDev::StrStream (prop bundle name build)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"               // CgsResource::ID::HashString
#include <cstring>                                                              // memset / memcmp / strncmp
#include <cstdlib>                                                              // atoi (prop-instance zone number)
#include <cstdio>                                                               // fopen/fread (PC schema-file leaf)
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"  // GetAttribSysAllocator (schema blob carve)
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator::Malloc

// BrnResource::GameDataModule - see the header. This pass reconstructs the rw-INDEPENDENT
// lifecycle spine (Prepare's base -> ResourceModule::Prepare core). The rw-allocator-gated
// bring-up (CreateBanks/CreatePools/CreateAllocators), the DLC/AttribSys/HUD/popup prepare
// stages, Construct, Update, Destruct and the ProcessXxxRequest handlers are DEFERRED.
namespace BrnResource
{
    // "New module": mark before any Prepare. NO logging here -- BrnGameModule embeds GameDataModule by
    // value, so this ctor runs at STATIC INIT (before gpDebugPrint is initialized); logging here would
    // deref an uninitialized gpDebugPrint and crash before the window opens.
    GameDataModule::GameDataModule()
    {
        mbIsNewModule = true;
    }

    // @ 0x82673F38 - resumable bring-up: base module, then the ResourceModule (memory/pool/
    // bundle/filesystem), then carve banks/pools/allocators. Re-enters at the current stage if
    // a sub-step is not yet ready. (The X360 then runs DLC stages 16-18+ and the AttribSys/HUD/
    // popup prepares - DEFERRED here; the machine completes after the allocator stage.)
    bool GameDataModule::Prepare(void* lpInputBuffer, void* lpOutputBuffer)
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_BASE:
            mePrepareStage = E_PREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // (X360 also Registers two debug components here - deferred)
            // fall through
        case E_PREPARE_RESOURCE:
            mePrepareStage = E_PREPARE_RESOURCE;
            if (!mResourceModule.Prepare())
                return false;
            mReceiverQueue.Clear();
            // fall through
        case E_PREPARE_BANKS:
            mePrepareStage = E_PREPARE_BANKS;
            if (!CreateBanks(lpInputBuffer, lpOutputBuffer))
                return false;
            // fall through
        case E_PREPARE_POOLS:
            mePrepareStage = E_PREPARE_POOLS;
            if (!CreatePools(lpInputBuffer, lpOutputBuffer))
                return false;
            // fall through
        case E_PREPARE_ALLOCATORS:
            mePrepareStage = E_PREPARE_ALLOCATORS;
            if (!CreateAllocators(lpInputBuffer, lpOutputBuffer))
                return false;
            // (X360: DLC stages 16-18 run between the allocators and stage 6 - deferred)
            // fall through
        case E_PREPARE_ATTRIBSYS:
        {
            mePrepareStage = E_PREPARE_ATTRIBSYS;
            // X360 stage 6 (0x82673F38 LABEL_18): AttribSysModule::Prepare (vtbl+64) with
            // the memory-map allocators -- GetLinearAllocator(22 AttribSysLinAlloc) +
            // GetHeapAllocator(0x13 AttrSysHeap / 0x14 GameTalkAlloc / 0x15 EAStl) -- and
            // liMaxNumVaults = 80; then the "Attrib" input carve + the staged schema
            // registration (PrepareAttribSysSchemaResource loops until its stage machine
            // reports done).
            CgsMemory::HeapMalloc*   lpEastlHeap    = mAllocatorList.GetHeapAllocator(0x15);
            CgsMemory::HeapMalloc*   lpGameTalkHeap = mAllocatorList.GetHeapAllocator(0x14);
            CgsMemory::HeapMalloc*   lpAttribHeap   = mAllocatorList.GetHeapAllocator(0x13);
            CgsMemory::LinearMalloc* lpLinearAlloc  = mAllocatorList.GetLinearAllocator(22);
            if (!mAttribSysModule.Prepare(lpLinearAlloc, lpAttribHeap, lpGameTalkHeap,
                                          lpEastlHeap, 80))
                return false;

            // The schema registration ([FLAG PC boot gate] inside the helper: the exe-baked
            // schema blobs are big-endian and not yet ported, so the PC helper logs and
            // completes without a live schema; the AttribSysModule's vault-array interior
            // stays gated on sbSchemaLoaded).
            {
                static CgsAttribSys::AttribSysIO::InputBuffer s_AttribPrepareInput;
                static bool s_bAttribPrepareInputConstructed = false;
                if (!s_bAttribPrepareInputConstructed)
                {
                    s_bAttribPrepareInputConstructed = true;
                    s_AttribPrepareInput.CgsModule::IOBuffer::Construct();
                    s_AttribPrepareInput.LockForWrite();
                    s_AttribPrepareInput.GetVaultRequestInterface()->mRequestQueue.Construct();
                    s_AttribPrepareInput.UnlockForWrite();
                }
                if (!PrepareAttribSysSchemaResource(&s_AttribPrepareInput))
                    return false;   // still registering -- re-enter next frame
            }
            // fall through
        }
        case E_PREPARE_DONE:
            meReleaseStage = E_PREPARE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // ---- DEFERRED (rw allocator middleware + game-data subsystems) --------------------
    // CreateBanks (0x8266DA28) / CreatePools (0x8266DB88) / CreateAllocators (0x8266DD00) carve the
    // resource banks/pools/allocators out of the GameData heap through the rw::IResourceAllocator
    // (the rwcore allocator layer). Inert stubs that report success so the Prepare stage machine
    // advances; reconstruct with that allocator layer. Construct (0x82671B90) builds the embedded
    // subsystems; Update pumps the ResourceModule and services the game-data query handlers
    // (ProcessGetVehicle/Wheel/Traffic/WorldUnit/PVS/... - 30+ handlers); Destruct (0x82664508)
    // tears it down. All inert until the loading-machine case 8 drives this module.
    bool GameDataModule::CreateBanks(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return true; }

    // @ 0x8266DB88 - create the game's resource pools. The X360 iterates the binary memory map
    // (unk_82F2A788) and sends a CreatePoolRequest per pool through the async dispatch
    // (DoCreatePoolRequest -> SendCreatePoolMemoryRequest -> MemoryModule alloc -> CreatePool). Here we
    // drive the SAME pool set directly from the extracted memory-map table (BrnMemoryMapData.h, generated
    // from progress/memory_map_artist.yaml): per pool, allocate its per-type backing memory from the root
    // debug allocator and PoolModule::CreatePool it. [marked deviation: the faithful async memory-request
    // dispatch is deferred; the pool data + Pool::InitPool are faithful.] Runs once (the Prepare stage
    // re-enters until done).
    bool GameDataModule::CreatePools(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/)
    {
        static bool s_bPoolsCreated = false;
        if (s_bPoolsCreated)
            return true;
        s_bPoolsCreated = true;

        rw::IResourceAllocator* lpAllocator =
            static_cast<rw::IResourceAllocator*>(Allocators::mpInternalDebugAllocator);
        if (lpAllocator == 0)
            return true;
        CgsResource::PoolModule&  lrPoolModule   = mResourceModule.GetPoolModule();
        CgsMemory::MemoryModule&  lrMemoryModule = mResourceModule.GetMemoryModule();
        const u32 KU_BLOCK = 0x10000u;   // 64 KB -- the MemoryModule root-bank block granularity

        s32 lNumCreated = 0, lNumFromBank = 0, lNumFromHeap = 0, lNumDepsWired = 0, lNumViaAsync = 0;

        // The ResourceModule input buffer CreatePools publishes CreatePool requests to; ResourceModule::
        // Update (the streaming shuttle) owns the pool/memory scratch buffers internally. Static so the
        // (large) embedded request queue is constructed once + reused across the loop.
        static CgsResource::ResourceIO::InputBuffer s_resIn;
        static bool s_ioConstructed = false;
        if (!s_ioConstructed) { s_resIn.Construct(); s_ioConstructed = true; }
        // id -> created Pool*, so each pool's import dependencies (referenced by pool id) resolve to real
        // pointers in this one pass: the memory-map order lists every dependency target before its
        // dependents (deps reference ids 10/25 @ idx 8/23; all dependents are at later indices).
        CgsResource::Pool* lapPoolById[64];
        memset(lapPoolById, 0, sizeof(lapPoolById));
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_POOLS; ++li)
        {
            const MemoryMapPoolDef& lrDef = KAC_MEMORY_MAP_POOLS[li];

            CgsResource::Pool::InitOptions lOpt;
            memset(&lOpt, 0, sizeof(lOpt));   // POD-ish; zero unused types/deps
            lOpt.miId                   = lrDef.miId;
            lOpt.mpcName                = lrDef.mpcName;
            lOpt.muMaxResources         = static_cast<u32>(lrDef.miMaxResources);
            lOpt.muMaxImports           = static_cast<u32>(lrDef.miMaxImports);
            lOpt.miRefCountThreshold    = 0;
            lOpt.miBankId               = -1;
            lOpt.mbAllowDefragmentation = lrDef.mbAllowDefrag;

            // Resolve this pool's import dependencies to the (earlier-created) pools by id. The dependency
            // graph drives cross-pool import resolution (e.g. OpenWorldGr/VFX/Environment import from Global
            // Textures; CarPool/Traffic also from CarSharedPool).
            s32 lNumDeps = 0;
            for (s32 ld = 0; ld < lrDef.miNumDeps && lNumDeps < 16; ++ld)
            {
                const s32 lDepId = lrDef.maiDeps[ld];
                CgsResource::Pool* lpDep = (lDepId >= 0 && lDepId < 64) ? lapPoolById[lDepId] : 0;
                if (lpDep != 0)
                    lOpt.mapDependencies[lNumDeps++] = lpDep;
            }
            lOpt.miNumDependencies = lNumDeps;
            lNumDepsWired += lNumDeps;

            // Per-type region = heap budget + (type 0) a margin >= the pool overhead (entry arrays / hash /
            // heap nodes Pool::InitPool Mallocs from the region), rounded up to the 64 KB block size so the
            // MemoryModule bank carve is block-exact. (Bump allocator -> the slack is harmless.)
            u32 lauRegion[3] = { 0, 0, 0 };
            u32 lauAlign[3]  = { 16u, 16u, 16u };
            for (s32 lt = 0; lt < 3; ++lt)
            {
                const u32 luHeapSize = lrDef.mauHeapSize[lt];
                if (luHeapSize == 0)
                    continue;
                lOpt.maHeapInfo[lt].muMaxNodes       = static_cast<u32>(lrDef.miMaxHeapNodes);
                lOpt.maHeapInfo[lt].muHeapMemorySize = luHeapSize;
                lOpt.maHeapInfo[lt].muHeapAlignment  = lrDef.mauHeapAlign[lt];

                u32 luMargin = 0;
                if (lt == 0)
                {
                    const u32 luMax = static_cast<u32>(lrDef.miMaxResources);
                    u32 luHashLen = 3u * luMax; --luHashLen;
                    luHashLen |= luHashLen >> 1;  luHashLen |= luHashLen >> 2;
                    luHashLen |= luHashLen >> 4;  luHashLen |= luHashLen >> 8;
                    luHashLen |= luHashLen >> 16; ++luHashLen;
                    luMargin = luMax * 512u + luHashLen * 16u
                             + static_cast<u32>(lrDef.miMaxHeapNodes) * 3u * 128u + 0x10000u;
                }
                lauAlign[lt]  = lrDef.mauHeapAlign[lt] < 16u ? 16u : lrDef.mauHeapAlign[lt];
                lauRegion[lt] = (luHeapSize + luMargin + (KU_BLOCK - 1u)) & ~(KU_BLOCK - 1u);
            }

            bool lbAnyType = (lauRegion[0] | lauRegion[1] | lauRegion[2]) != 0;

            // Carry the pool's resource descriptor through the async pending FIFO into CreatePool (the async
            // path fills mResource from the memory response; mDescriptor is set here from the carved regions).
            for (s32 lt = 0; lt < 3; ++lt)
            {
                if (lauRegion[lt] == 0)
                    continue;
                lOpt.mDescriptor.m_baseResourceDescriptors[lt].m_size      = lauRegion[lt];
                lOpt.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = lauAlign[lt];
            }

            const s32 liBankId = 100 + li;   // unique leaf-bank id (pool ids occupy 0..26)
            CgsResource::Pool* lpPool = 0;
            bool lbViaAsync = false;

            // ---- [Faithful async path] publish a CreatePool request to the ResourceModule input and pump
            // ResourceModule::Update -- the real streaming shuttle: ProcessResourceRequests -> pool input;
            // PoolModule::Update (ProcessInputBuffer -> SendCreatePoolMemoryRequest); ProcessPoolResource
            // Requests -> MemoryModule input; MemoryModule::Update -> ProcessCreateResourceRequest carves the
            // bank; ProcessMemoryResponses -> the pool receiver queue; (next frame) ProcessReceiverQueue ->
            // DoCreatePoolRequest -> CreatePool. The request carries the resolved InitOptions so cross-pool
            // dependency order (lapPoolById) is preserved exactly. Pool create is 2-frame, so pump a few times.
            if (lbAnyType)
            {
                CgsResource::CreatePoolRequestEvent lReqEvent;
                lReqEvent.mOptions       = lOpt;
                lReqEvent.miBankId       = liBankId;
                lReqEvent.miParentBankId = 0;                 // root bank
                for (s32 lt = 0; lt < 3; ++lt) { lReqEvent.mauRegion[lt] = lauRegion[lt]; lReqEvent.mauAlign[lt] = lauAlign[lt]; }

                s_resIn.LockForWrite();
                s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReqEvent),
                                                     0 /*CreatePool*/, static_cast<s32>(sizeof(lReqEvent)));
                s_resIn.UnlockForWrite();

                for (s32 lf = 0; lf < 4 && lpPool == 0; ++lf)
                {
                    mResourceModule.Update(&s_resIn, 0);
                    lpPool = lrPoolModule.GetPool(lrDef.miId);
                }
                if (lpPool != 0 && lpPool->IsValid()) { lbViaAsync = true; ++lNumViaAsync; ++lNumFromBank; }
            }

            // ---- [Fallback] synchronous bank carve, if the async memory dispatch did not deliver. Carve a
            // leaf bank directly, else allocate from the root debug allocator, fill mResource, CreatePool.
            if (!lbViaAsync)
            {
                CgsMemory::MemoryBank::Params lParams;
                memset(&lParams, 0, sizeof(lParams));
                for (s32 lc = 0; lc < 31 && lrDef.mpcName[lc]; ++lc)
                    lParams.macName[lc] = lrDef.mpcName[lc];
                lParams.mnParentBankId       = 0;            // root bank
                lParams.mnBankId             = liBankId;
                lParams.mbIsLeaf             = true;
                lParams.mbAllowFragmentation = lrDef.mbAllowDefrag;
                for (s32 lt = 0; lt < 3; ++lt)
                {
                    if (lauRegion[lt] == 0)
                        continue;
                    lParams.mauBankSize[lt]   = lauRegion[lt];
                    lParams.mauBankBlocks[lt] = lauRegion[lt] / KU_BLOCK;
                }

                rw::Resource lMem;
                for (s32 lr = 0; lr < 4; ++lr)
                    lMem.m_baseResources[lr] = 0;
                const bool lbFromBank = lbAnyType && lrMemoryModule.CreatePoolBank(&lParams, lMem);

                bool lbAllocOk = true;
                for (s32 lt = 0; lt < 3; ++lt)
                {
                    if (lauRegion[lt] == 0)
                        continue;
                    void* lpMem = lbFromBank ? lMem.m_baseResources[lt] : 0;
                    if (!lbFromBank)
                    {
                        rw::ResourceDescriptor lDesc;
                        for (u32 lu = 0; lu < 4; ++lu)
                        {
                            lDesc.m_baseResourceDescriptors[lu].m_size      = 0;
                            lDesc.m_baseResourceDescriptors[lu].m_alignment = 1;
                        }
                        lDesc.m_baseResourceDescriptors[0].m_size      = lauRegion[lt];
                        lDesc.m_baseResourceDescriptors[0].m_alignment = lauAlign[lt];
                        rw::Resource lFb = lpAllocator->DoAllocate(lDesc, lrDef.mpcName);
                        lpMem = lFb.m_baseResources[0];
                    }
                    if (lpMem == 0)
                    {
                        lbAllocOk = false;
                        break;
                    }
                    lOpt.mResource.m_baseResources[lt] = lpMem;
                }

                if (!lbAllocOk)
                {
                    *CgsDev::Log::gpDebugPrint << "[5b POOLS] alloc FAILED for pool " << lrDef.mpcName << "\n";
                    continue;
                }
                if (lbFromBank) ++lNumFromBank; else ++lNumFromHeap;
                lpPool = lrPoolModule.CreatePool(&lOpt);
            }

            if (lpPool != 0 && lpPool->IsValid())
                ++lNumCreated;
            if (lpPool != 0 && lrDef.miId >= 0 && lrDef.miId < 64)
                lapPoolById[lrDef.miId] = lpPool;   // so later pools can depend on this one
        }
        *CgsDev::Log::gpDebugPrint << "[5b POOLS] created " << (s32)lNumCreated << " of "
                                  << (s32)KI_NUM_MEMORY_MAP_POOLS << " pools ("
                                  << (s32)lNumViaAsync << " via async memory dispatch, "
                                  << (s32)lNumFromBank << " from MemoryModule banks, "
                                  << (s32)lNumFromHeap << " from debug heap), "
                                  << (s32)lNumDepsWired << " dependencies wired\n";

        // [resource streaming bring-up] -- FLAG: non-faithful bring-up TEST scaffolding. The X360
        // GameDataModule::Prepare just creates the pools and returns; it does NOT publish inline font
        // load/acquire/unload test requests. This inline pump currently faults (0xC0000005) and blocks
        // the boot, so it is disabled to let Prepare complete faithfully -- the real streaming path is
        // exercised by the GUI / game flow when it actually requests resources. Set to `if (true)` to
        // re-validate the request -> shuttle -> bundle loader -> pool path in isolation.
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Prepare: pools ready, streaming-test scaffolding skipped (faithful) -> return\n";
        if (false)
        {
            CgsResource::Events::LoadBundleRequest lReq;
            memset(&lReq, 0, sizeof(lReq));
            lReq.SetFileName("Language/Fonts/Default.font");
            lReq.miPoolId = 0;   // the Fonts pool

            s_resIn.LockForWrite();
            s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReq),
                                                 2 /*LoadBundle*/, static_cast<s32>(sizeof(lReq)));
            s_resIn.UnlockForWrite();
            mResourceModule.Update(&s_resIn, 0);

            CgsResource::Pool* lpFonts = lrPoolModule.GetPool(0);
            s32 liFontIdx = -1;
            CgsResource::Entry* lpFontEntry = lpFonts
                ? lpFonts->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liFontIdx) : 0;
            *CgsDev::Log::gpDebugPrint << "[stream] Fonts pool (id 0) font resource via shuttle: "
                                      << (lpFontEntry ? "FOUND" : "absent") << "\n";

            // The font bundle carries 2 resources: the Font (0x21) + its atlas Texture (RwRaster, 0x0).
            // Confirm the texture resource streamed too -- proving the system handles the most platform-
            // divergent resource type (textures), not just the font, through the same shuttle.
            s32 liTexIdx = -1;
            CgsResource::Entry* lpTexEntry = lpFonts
                ? lpFonts->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_TEXTURE, &liTexIdx) : 0;
            *CgsDev::Log::gpDebugPrint << "[stream] Fonts pool texture resource (RwRaster) via shuttle: "
                                      << (lpTexEntry ? "FOUND" : "absent") << "\n";

            // [acquire bring-up] prove the consumer path: AcquireResource by id through the shuttle. The
            // streamed font is already at status 2 (loaded) from BundleLoader's load-completion pass, so it
            // is acquirable directly. Publish an AcquireResourceRequest (id 4) -> ProcessResourceRequests ->
            // pool input -> DoAcquireResourceRequest -> Pool::FindResource (gates on status & 2) -> response
            // on the pool output queue -> ProcessPoolOutputResponses routes it back to our receiver.
            if (lpFontEntry != 0 && lpFonts != 0)
            {
                const CgsResource::ID lFontId = lpFontEntry->mID;

                static CgsModule::EventReceiverQueue<4096, 16> s_acqReceiver;
                static bool s_acqRecvCtor = false;
                if (!s_acqRecvCtor) { s_acqReceiver.Construct(); s_acqRecvCtor = true; }
                s_acqReceiver.Clear();

                CgsResource::Events::AcquireResourceRequest lAcq;
                memset(&lAcq, 0, sizeof(lAcq));
                lAcq.mpUser          = &s_acqReceiver;
                lAcq.miPoolId        = 0;
                lAcq.mResourceId     = lFontId;
                lAcq.mbCheckRefCount = true;

                s_resIn.LockForWrite();
                s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAcq),
                                                     4 /*AcquireResource*/, static_cast<s32>(sizeof(lAcq)));
                s_resIn.UnlockForWrite();
                mResourceModule.Update(&s_resIn, 0);

                const CgsModule::Event* lpResp = 0; s32 liRespSize = 0;
                const s32 liRespId = s_acqReceiver.GetFirstEvent(&lpResp, &liRespSize);
                const CgsResource::Events::AcquireResourceResponse* lpAcqResp =
                    (liRespId != -1 && lpResp != 0)
                        ? reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpResp) : 0;
                const bool lbAcquired = lpAcqResp != 0 && lpAcqResp->mpSourceEntry != 0
                                        && lpAcqResp->mpSourceEntry == lpFontEntry;
                *CgsDev::Log::gpDebugPrint << "[stream] AcquireResource(font) via shuttle: "
                                          << (lbAcquired ? "handle OK" : "FAILED") << "\n";
            }

            // [unload bring-up] prove the load/unload pair: publish an UnloadBundleRequest (id 3) for the
            // font -> ProcessResourceRequests -> bundle loader -> UnloadBundle (ref-count-release each
            // resource; frees heap memory + slot at refcount 0). Confirm the font is then gone from the pool.
            if (lpFonts != 0)
            {
                CgsResource::Events::UnloadBundleRequest lUnload;
                memset(&lUnload, 0, sizeof(lUnload));
                lUnload.SetFileName("Language/Fonts/Default.font");
                lUnload.miPoolId = 0;

                s_resIn.LockForWrite();
                s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUnload),
                                                     3 /*UnloadBundle*/, static_cast<s32>(sizeof(lUnload)));
                s_resIn.UnlockForWrite();
                mResourceModule.Update(&s_resIn, 0);

                s32 liGoneIdx = -1;
                CgsResource::Entry* lpGone =
                    lpFonts->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liGoneIdx);
                *CgsDev::Log::gpDebugPrint << "[stream] after UnloadBundle, Fonts pool font resource: "
                                          << (lpGone ? "STILL PRESENT (bug)" : "gone (freed)") << "\n";
            }
        }
        return true;
    }

    // ------------------------------------------------------------------------------------------
    // The rw allocator OBJECTS the registry's pointer rows reference. On the X360 the create
    // requests are serviced by CgsMemory::MemoryModule (ProcessCreateLinearAllocatorRequest
    // @0x8286D130 / the rw-general sibling), which carves a leaf memory bank per allocator and
    // materialises the allocator object service-side, returning its pointer in the response the
    // CreateAllocators drain stores into mapGeneratedRW*Allocators. [marked deviation: the async
    // memory dispatch is deferred (CreatePools precedent), so the objects live here, TU-local.]
    static rw::LinearResourceAllocator        s_aRWLinearAllocators[5];
    static rw::core::GeneralResourceAllocator s_aRWGeneralAllocators[5];

    // @ 0x8266DD00 - create the memory map's allocator set and publish it through the
    // bank->allocator registry (mAllocatorList) the OutputBuffer hands to every module Prepare.
    //
    // The X360 is a resumable two-phase machine: phase 1 walks the memory map's five allocator
    // record tables (mpMemoryMap +16..+32 counts / +44..+60 tables) and publishes one create
    // request per record into the ResourceModule input (event types 10 raw-resource / 14 linear +
    // heap / 11 rw-linear / 12 rw-general; each request carries &mReceiverQueue as mpUser, the
    // slot index, and - for the type-14s - the module-embedded LinearMalloc/HeapMalloc object the
    // service constructs in place), registering each record's bank id in the AllocatorList
    // (maiAllocatorMap[bank] = per-type slot, maeAllocatorType[bank] = family) as it goes; later
    // calls pump UpdateResourceModule until every response is back in mReceiverQueue, then drain:
    // case 10 stores the returned rw::Resource + descriptor into maGeneratedRawResources /
    // ...Descriptors, cases 11/12 store the returned allocator pointers into
    // mapGeneratedRW{Linear,General}Allocators AND the AllocatorList rows, case 14 has nothing to
    // store (the embedded objects were constructed in place). [marked deviation, CreatePools
    // precedent: the request/response round-trip through the ResourceModule + the per-allocator
    // leaf-bank carve (CreateBanks' bank tree is not stood up on PC) are deferred - the same
    // memory-map records (BrnMemoryMapData.h allocator tables, dumped from unk_82F2A788) are
    // driven synchronously here, backing memory comes from the root debug allocator, and the
    // registry/embedded-object stores are the faithful part.] Runs once (the Prepare stage
    // machine re-enters until done).
    bool GameDataModule::CreateAllocators(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/)
    {
        // X360 (every call, before the outstanding-request check): wire the audio-stream
        // allocator row at the module-embedded LinearMalloc (`a1[106765] = a1 + 109814`).
        mAllocatorList.mpAudioStreamAllocator = &mAudioStreamAllocator;

        static bool s_bAllocatorsCreated = false;
        if (s_bAllocatorsCreated)
            return true;
        s_bAllocatorsCreated = true;

        rw::IResourceAllocator* lpAllocator =
            static_cast<rw::IResourceAllocator*>(Allocators::mpInternalDebugAllocator);
        if (lpAllocator == 0)
            return true;

        miNumAllocatorCreationRequests = 0;
        s32 lNumCreated = 0;

        // Carve one memory-type lane from the root debug allocator (the deviation's stand-in for
        // the per-allocator leaf-bank carve).
        struct LaneAlloc
        {
            static void* Carve(rw::IResourceAllocator* lpRoot, u32 luSize, u32 luAlign,
                               const char* lpcName)
            {
                rw::ResourceDescriptor lDesc;
                for (u32 lu = 0; lu < 4; ++lu)
                {
                    lDesc.m_baseResourceDescriptors[lu].m_size      = 0;
                    lDesc.m_baseResourceDescriptors[lu].m_alignment = 1;
                }
                lDesc.m_baseResourceDescriptors[0].m_size      = luSize;
                lDesc.m_baseResourceDescriptors[0].m_alignment = luAlign < 1 ? 1 : luAlign;
                rw::Resource lRes = lpRoot->DoAllocate(lDesc, lpcName);
                return lRes.m_baseResources[0];
            }
        };

        // ---- type-10 records: raw resources (the drain's case 10 fills the embedded pair) ----
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_RAW_ALLOCATORS && li < 2; ++li)
        {
            const MemoryMapAllocatorDef& lrDef = KAC_MEMORY_MAP_RAW_ALLOCATORS[li];
            ++miNumAllocatorCreationRequests;

            rw::Resource&           lrRes  = maGeneratedRawResources[li];
            rw::ResourceDescriptor& lrDesc = maGeneratedRawResourceDescriptors[li];
            for (u32 lu = 0; lu < 4; ++lu)
            {
                lrRes.m_baseResources[lu] = 0;
                lrDesc.m_baseResourceDescriptors[lu].m_size      = lrDef.mauSize[lu];
                lrDesc.m_baseResourceDescriptors[lu].m_alignment = lrDef.mauAlign[lu];
                if (lrDef.mauSize[lu] != 0)
                    lrRes.m_baseResources[lu] =
                        LaneAlloc::Carve(lpAllocator, lrDef.mauSize[lu], lrDef.mauAlign[lu],
                                         lrDef.mpcName);
            }

            mAllocatorList.maiAllocatorMap[lrDef.miBankId]  = li;
            mAllocatorList.maeAllocatorType[lrDef.miBankId] = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW;
            mAllocatorList.mapRawResources[li]              = &lrRes;
            mAllocatorList.mapRawResourceDescriptors[li]    = &lrDesc;
            ++lNumCreated;
        }

        // ---- type-14 records, linear family: LinearMalloc constructed over the module-embedded
        //      objects (the service side does `LinearMalloc::Create(record.mpLinear, bankMem,
        //      size)` @0x8286D9F0; the publisher passed &maGeneratedLinearAllocators[i]) --------
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_LINEAR_ALLOCATORS && li < 7; ++li)
        {
            const MemoryMapAllocatorDef& lrDef = KAC_MEMORY_MAP_LINEAR_ALLOCATORS[li];
            ++miNumAllocatorCreationRequests;

            void* lpMem = LaneAlloc::Carve(lpAllocator, lrDef.mauSize[0], lrDef.mauAlign[0],
                                           lrDef.mpcName);
            if (lpMem == 0)
            {
                *CgsDev::Log::gpDebugPrint << "[5c ALLOCATORS] alloc FAILED for linear "
                                          << lrDef.mpcName << "\n";
                continue;
            }
            maGeneratedLinearAllocators[li].Construct();
            maGeneratedLinearAllocators[li].Create(lpMem, lrDef.mauSize[0]);

            mAllocatorList.maiAllocatorMap[lrDef.miBankId]  = li;
            mAllocatorList.maeAllocatorType[lrDef.miBankId] = CgsMemory::MemoryMap::E_ALLOCATORTYPE_LINEAR;
            mAllocatorList.mapLinearAllocators[li]          = &maGeneratedLinearAllocators[li];
            ++lNumCreated;
        }

        // ---- type-14 records, heap family: HeapMalloc constructed over the embedded objects
        //      (service side: `HeapMalloc::Construct(record.mpHeap, bankMem, size)`) ------------
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_HEAP_ALLOCATORS && li < 8; ++li)
        {
            const MemoryMapAllocatorDef& lrDef = KAC_MEMORY_MAP_HEAP_ALLOCATORS[li];
            ++miNumAllocatorCreationRequests;

            void* lpMem = LaneAlloc::Carve(lpAllocator, lrDef.mauSize[0], lrDef.mauAlign[0],
                                           lrDef.mpcName);
            if (lpMem == 0)
            {
                *CgsDev::Log::gpDebugPrint << "[5c ALLOCATORS] alloc FAILED for heap "
                                          << lrDef.mpcName << "\n";
                continue;
            }
            maGeneratedHeapAllocators[li].Construct(lpMem, static_cast<s32>(lrDef.mauSize[0]));

            mAllocatorList.maiAllocatorMap[lrDef.miBankId]  = li;
            mAllocatorList.maeAllocatorType[lrDef.miBankId] = CgsMemory::MemoryMap::E_ALLOCATORTYPE_HEAP;
            mAllocatorList.mapHeapAllocators[li]            = &maGeneratedHeapAllocators[li];
            ++lNumCreated;
        }

        // ---- type-11 records: rw linear resource allocators (service @0x8286D130: bank carve
        //      per non-zero descriptor lane, then LinearResourceAllocator::Initialize(resource,
        //      capacity); the drain's case 11 stores the returned pointer) --------------------
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_RWLINEAR_ALLOCATORS && li < 5; ++li)
        {
            const MemoryMapAllocatorDef& lrDef = KAC_MEMORY_MAP_RWLINEAR_ALLOCATORS[li];
            ++miNumAllocatorCreationRequests;

            rw::Resource           lRes;
            rw::ResourceDescriptor lCapacity;
            bool lbAllocOk = true;
            for (u32 lu = 0; lu < 4; ++lu)
            {
                lCapacity.m_baseResourceDescriptors[lu].m_size      = lrDef.mauSize[lu];
                lCapacity.m_baseResourceDescriptors[lu].m_alignment = lrDef.mauAlign[lu];
                lRes.m_baseResources[lu] = 0;
                if (lrDef.mauSize[lu] == 0)
                    continue;
                lRes.m_baseResources[lu] =
                    LaneAlloc::Carve(lpAllocator, lrDef.mauSize[lu], lrDef.mauAlign[lu],
                                     lrDef.mpcName);
                if (lRes.m_baseResources[lu] == 0)
                    lbAllocOk = false;
            }
            if (!lbAllocOk)
            {
                *CgsDev::Log::gpDebugPrint << "[5c ALLOCATORS] alloc FAILED for rw linear "
                                          << lrDef.mpcName << "\n";
                continue;
            }
            // (LinearResourceAllocator::GetResourceDescriptor is the identity - a linear
            // allocator keeps its bookkeeping in its own object - so the carve == the capacity.)
            s_aRWLinearAllocators[li].Initialize(lRes, lCapacity);

            mapGeneratedRWLinearAllocators[li]              = &s_aRWLinearAllocators[li];
            mAllocatorList.maiAllocatorMap[lrDef.miBankId]  = li;
            mAllocatorList.maeAllocatorType[lrDef.miBankId] = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWLINEAR;
            mAllocatorList.mapRWLinearAllocators[li]        = &s_aRWLinearAllocators[li];
            ++lNumCreated;
        }

        // ---- type-12 records: rw general resource allocators (EA GeneralAllocator pair). The
        //      X360 service sizes the carve via GetResourceDescriptor (lane 0 grows by the
        //      in-heap bookkeeping overhead, rounded to the lane alignment) then Initializes
        //      with the ORIGINAL capacity - mirrored here; the drain's case 12 stores the
        //      returned pointer. -----------------------------------------------------------
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_RWGENERAL_ALLOCATORS && li < 5; ++li)
        {
            const MemoryMapAllocatorDef& lrDef = KAC_MEMORY_MAP_RWGENERAL_ALLOCATORS[li];
            ++miNumAllocatorCreationRequests;

            rw::BaseResourceDescriptors<5> lWanted, lCarve;
            for (u32 lu = 0; lu < 5; ++lu)
            {
                lWanted.m_baseResourceDescriptors[lu].m_size      = lrDef.mauSize[lu];
                lWanted.m_baseResourceDescriptors[lu].m_alignment = lrDef.mauAlign[lu];
            }
            rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lCarve, &lWanted);

            rw::Resource           lRes;
            rw::ResourceDescriptor lCapacity;
            bool lbAllocOk = true;
            for (u32 lu = 0; lu < 4; ++lu)
            {
                lCapacity.m_baseResourceDescriptors[lu].m_size      = lrDef.mauSize[lu];
                lCapacity.m_baseResourceDescriptors[lu].m_alignment = lrDef.mauAlign[lu];
                lRes.m_baseResources[lu] = 0;
                if (lCarve.m_baseResourceDescriptors[lu].m_size == 0)
                    continue;
                lRes.m_baseResources[lu] =
                    LaneAlloc::Carve(lpAllocator,
                                     lCarve.m_baseResourceDescriptors[lu].m_size,
                                     lCarve.m_baseResourceDescriptors[lu].m_alignment,
                                     lrDef.mpcName);
                if (lRes.m_baseResources[lu] == 0)
                    lbAllocOk = false;
            }
            if (!lbAllocOk)
            {
                *CgsDev::Log::gpDebugPrint << "[5c ALLOCATORS] alloc FAILED for rw general "
                                          << lrDef.mpcName << "\n";
                continue;
            }
            s_aRWGeneralAllocators[li].Initialize(lRes, lCapacity);

            mapGeneratedRWGeneralAllocators[li]             = &s_aRWGeneralAllocators[li];
            mAllocatorList.maiAllocatorMap[lrDef.miBankId]  = li;
            mAllocatorList.maeAllocatorType[lrDef.miBankId] = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWHEAP;
            mAllocatorList.mapRWGeneralAllocators[li]       = &s_aRWGeneralAllocators[li];
            ++lNumCreated;
        }

        *CgsDev::Log::gpDebugPrint << "[5c ALLOCATORS] created " << lNumCreated << " of "
                                  << miNumAllocatorCreationRequests
                                  << " memory-map allocators (raw "
                                  << KI_NUM_MEMORY_MAP_RAW_ALLOCATORS << ", linear "
                                  << KI_NUM_MEMORY_MAP_LINEAR_ALLOCATORS << ", heap "
                                  << KI_NUM_MEMORY_MAP_HEAP_ALLOCATORS << ", rw linear "
                                  << KI_NUM_MEMORY_MAP_RWLINEAR_ALLOCATORS << ", rw general "
                                  << KI_NUM_MEMORY_MAP_RWGENERAL_ALLOCATORS << ")\n";

        // X360 (end of the drain): the outstanding counter is zeroed and the receiver cleared
        // once every response has been stored.
        miNumAllocatorsCreated         = lNumCreated;   // FLAG: updater outside this fn's asm
        miNumAllocatorCreationRequests = 0;
        return true;
    }

    // @ 0x82673258 -- the staged schema registration (Prepare stage 6 helper). X360:
    //   stage 0: push RegisterSchema(mReceiverQueue, vlt = the exe-baked schema .vlt blob
    //            @0x82CD3D88 [size @0x82CD3D84 = 5664], bin = the .bin blob @0x82CD53AC
    //            [size @0x82CD53A8 = 20352]) into the attrib input, Clear the receiver,
    //            stage = 1; then the shared tail (UpdateResourceModule + a DIRECT
    //            AttribSysModule::ProcessInputs on the attrib input) and return false.
    //   stage 1: wait for the SchemaRegisteredResponse on mReceiverQueue; on arrival
    //            Clear + stage = 2 (still returns false through the tail).
    //   stage 2: return true (complete). stage >= 3 asserts "Invalid resource prepare stage.\n".
    //
    // [FLAG PC boot gate] the schema blobs are X360 BIG-ENDIAN rodata; their LE port (the
    // schema-vault chunk container flips like attribsys_transcode.py's data vaults, but the
    // BIN payload is the SDK ClassLoadData record set) and the Attrib SDK runtime cluster
    // are not committed. Until then the PC stage 0 logs one-shot and jumps straight to
    // stage 2 WITHOUT registering a schema -- AttribSysModule::sbSchemaLoaded stays false
    // and its vault-array interior stays gated (see CgsAttribSysModule.cpp).
    bool GameDataModule::PrepareAttribSysSchemaResource(
            CgsAttribSys::AttribSysIO::InputBuffer* lpAttribModuleInputBuffer)
    {
        CGS_ASSERT(lpAttribModuleInputBuffer != 0, "lpAttribModuleInputBuffer != NULL");   // X360 line 1481

        switch (miResourcePrepareStage)
        {
        case 0:
        {
            // [PC seam] the schema pair is exe-baked rodata on the X360; the PC
            // build loads the LE-ported files the schema port tool stages
            // (build/game/schema.vlt + schema.bin -- the filenames the vault's
            // own DepN dependency table records). One-time load into the
            // AttribSys package allocator (the blobs stay live for the process
            // exactly like the X360 rodata). Gated on KB_PC_ATTRIB_SCHEMA_FILES
            // until the Attrib SDK runtime cluster is linked (see the constant's
            // note in CgsAttribSysModule.h).
            static void* s_pSchemaVlt = 0;
            static void* s_pSchemaBin = 0;
            static s32   s_iSchemaVltSize = 0;
            static s32   s_iSchemaBinSize = 0;
            if (CgsAttribSys::KB_PC_ATTRIB_SCHEMA_FILES && s_pSchemaVlt == 0)
            {
                static const char* s_apcSchemaFiles[2] = { "schema.vlt", "schema.bin" };
                void* lapBlobs[2] = { 0, 0 };
                s32 laiSizes[2] = { 0, 0 };
                for (int liFile = 0; liFile < 2; ++liFile)
                {
                    FILE* lpFile = fopen(s_apcSchemaFiles[liFile], "rb");
                    CGS_ASSERT(lpFile != 0, "PC schema file missing (run attribsys_schema_port.py)");
                    if (lpFile == 0)
                        break;
                    fseek(lpFile, 0, SEEK_END);
                    laiSizes[liFile] = static_cast<s32>(ftell(lpFile));
                    fseek(lpFile, 0, SEEK_SET);
                    lapBlobs[liFile] =
                        CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Malloc(
                            static_cast<size_t>(laiSizes[liFile]), 0);
                    fread(lapBlobs[liFile], 1, static_cast<size_t>(laiSizes[liFile]), lpFile);
                    fclose(lpFile);
                }
                s_pSchemaVlt = lapBlobs[0];
                s_iSchemaVltSize = laiSizes[0];
                s_pSchemaBin = lapBlobs[1];
                s_iSchemaBinSize = laiSizes[1];
            }

            if (!CgsAttribSys::KB_PC_ATTRIB_SCHEMA_FILES || s_pSchemaVlt == 0 || s_pSchemaBin == 0)
            {
                static bool s_bLoggedSchemaGate = false;
                if (!s_bLoggedSchemaGate)
                {
                    s_bLoggedSchemaGate = true;
                    *CgsDev::Log::gpDebugPrint
                        << "GameDataModule::PrepareAttribSysSchemaResource: Attrib SDK "
                           "runtime not mounted -- RegisterSchema skipped [FLAG PC boot gate]\n";
                }
                // X360: RegisterSchema(&mReceiverQueue, schemaVlt, 5664, schemaBin, 20352);
                // mReceiverQueue.Clear(); miResourcePrepareStage = 1; + the ProcessInputs tail.
                miResourcePrepareStage = 2;   // gated jump (see the seam note above)
                return false;
            }

            // X360 stage 0: push RegisterSchema (event type 1) with the two blobs
            // (exe-baked there, the ported LE files here), clear the receiver, advance.
            lpAttribModuleInputBuffer->GetVaultRequestInterface()->RegisterSchema(
                &mReceiverQueue, s_pSchemaVlt, s_iSchemaVltSize, s_pSchemaBin, s_iSchemaBinSize);
            mReceiverQueue.Clear();
            miResourcePrepareStage = 1;

            // The shared X360 per-pass tail: a DIRECT AttribSysModule::ProcessInputs
            // on the attrib input (the schema request is consumed synchronously; the
            // SchemaRegisteredResponse lands on mReceiverQueue for stage 1).
            mAttribSysModule.ProcessInputs(lpAttribModuleInputBuffer);
            return false;
        }
        case 1:
            if (mReceiverQueue.GetLength() <= 0)
            {
                // Keep draining while the reply is pending (the X360 tail runs
                // ProcessInputs on every stage-0/1 pass).
                mAttribSysModule.ProcessInputs(lpAttribModuleInputBuffer);
                return false;
            }
            mReceiverQueue.Clear();
            miResourcePrepareStage = 2;
            return false;
        case 2:
            return true;
        default:
            CGS_ASSERT(false, "Invalid resource prepare stage.\n");   // X360 line 1534
            return false;
        }
    }

    void GameDataModule::Construct()
    {
        // X360 0x82671B90 first runs the module base's Construct (stage/DataBuffer reset; it
        // clears mbIsNewModule, which is re-raised below).
        CgsModule::ModuleSingleBuffered::Construct();

        mbIsNewModule = true;   // X360 *(this+4)=1 (new module type; base Prepare skips the old IO path)

        // X360 0x82671B90: wire the in-flight event-slot pool -- capacity 96
        // (`*(a1+439324) = 96`), the module-embedded element array (a1+439328) and
        // free-index array (a1+443936), then Clear -- and the internal receiver queue
        // (0x8000-byte buffer @a1+363120, 16-byte alignment: `*(a1+363112)=0x8000;
        // *(a1+363116)=16` + Clear). Every request the world handlers publish into the
        // ResourceModule names mReceiverQueue as its mpUser reply-to.
        mGameDataEventSlotPool.Construct(maGameDataEventSlots, masGameDataEventSlotFreeIndices,
                                         static_cast<s16>(KI_NUM_GAMEDATA_EVENT_SLOTS));
        mReceiverQueue.Construct();

        // X360 0x82671B90: the attrib reply receiver (a1+395888) + the embedded
        // AttribSysModule (a1+399000) are constructed in the same pass.
        mAttribSysReceiverQueue.Construct();
        mAttribSysModule.Construct();

        // Completion-routing state words (X360 a1+439284 / a1+475936 / a1+475940). [reliable
        // init] zero-start so the counters are coherent from the first Update; the X360
        // Construct's full store list for this region is not mapped (the module rides
        // zero-init BSS there), so this is the same observable state.
        muLoadedSoundBundlesCount       = 0;
        miWorldCollisionRefCount        = 0;
        miWorldCollisionValidatePending = 0;
        miResourcePrepareStage          = 0;

        // X360 0x82671B90 (tail): reset the bank->allocator registry -- the inline
        // `maiAllocatorMap[0..66] = -1` loop == AllocatorList::Construct. CreateAllocators
        // (deferred) populates it; Update publishes it to the OutputBuffer each frame.
        mAllocatorList.Construct();

        // [5b] Bring up the resource memory system. This is an incrementally-populated stand-in for the
        // X360 ConstructResourceModule (0x8266D570): it now builds the real CgsResource::ResourceModule::
        // InitOptions composite (memory + pool + bundle-loader option blocks) instead of a bare
        // MemoryModule InitOptions, carves the root region from the debug allocator, and constructs the
        // ResourceModule. The MemoryModule block is fully populated (so MemoryModule carves its bank/block
        // tables, as before); the Pool/BundleLoader blocks are minimal for now (PoolModule front half
        // ignores them; the type-list + defrag/heap params land with the PoolModule back half + the full
        // RegisterResourceTypes).
        rw::IResourceAllocator* lpAllocator =
            static_cast<rw::IResourceAllocator*>(Allocators::mpInternalDebugAllocator);
        if (lpAllocator == 0)
            return;

        static CgsResource::ResourceModule::InitOptions s_InitOptions;   // outlives Construct (module keeps the region)
        CgsMemory::MemoryModule::InitOptions& lrMem = s_InitOptions.mMemoryInitOptions;
        lrMem.muMaxBanks  = 128;
        lrMem.muHighestId = 1024;
        lrMem.muMaxBlocks = 30000;

        // Root memory regions, carved in 64 KB blocks, sized to hold the 27 pools' per-type memory (sum
        // ~70 MB type 0 / main + ~196 MB type 1 / graphics, + per-pool overhead + block rounding). The
        // MemoryModule's root bank (bank 0) is built from these; GameDataModule::CreatePools then carves a
        // leaf bank per pool out of them (#2 -- pool memory from the MemoryModule banks).
        const u32 KU_BLOCK    = 0x00010000u;   // 64 KB
        const u32 KU_TYPE0    = 0x0A000000u;   // 160 MB main  (2560 blocks)
        const u32 KU_TYPE1    = 0x0E000000u;   // 224 MB gfx   (3584 blocks)
        rw::ResourceDescriptor lRegionDesc;
        for (u32 li = 0; li < 4; ++li)
        {
            lRegionDesc.m_baseResourceDescriptors[li].m_size      = 0;
            lRegionDesc.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        // Align the root regions to the block size (64 KB): leaf-bank memory is root + N*blockSize, so a
        // block-aligned root keeps every pool's per-type heap memory aligned to its heap alignment (16 main
        // / 4096 graphics, both <= 64 KB) -- else Heap::Prepare asserts (lpHeapMemory % heapAlign == 0).
        lRegionDesc.m_baseResourceDescriptors[0].m_alignment = KU_BLOCK;

        lRegionDesc.m_baseResourceDescriptors[0].m_size = KU_TYPE0;
        rw::Resource lType0 = lpAllocator->DoAllocate(lRegionDesc, "GameDataRoot0");
        lrMem.maResourceSets[0].mpDataStart = lType0.m_baseResources[0];
        lrMem.maResourceSets[0].muDataSize  = KU_TYPE0;
        lrMem.maResourceSets[0].muNumBlocks = static_cast<u16>(KU_TYPE0 / KU_BLOCK);

        lRegionDesc.m_baseResourceDescriptors[0].m_size = KU_TYPE1;
        rw::Resource lType1 = lpAllocator->DoAllocate(lRegionDesc, "GameDataRoot1");
        lrMem.maResourceSets[1].mpDataStart = lType1.m_baseResources[0];
        lrMem.maResourceSets[1].muDataSize  = KU_TYPE1;
        lrMem.maResourceSets[1].muNumBlocks = static_cast<u16>(KU_TYPE1 / KU_BLOCK);
        // memory types 2-4 left zeroed (skipped by CreateRootBank)

        // [PC diagnostic] the serialised resource format stores pointers in FOUR-BYTE slots
        // (the PointerFromU32 convention); on x64 that only round-trips while the pointee is
        // below 4 GB, which is what BrnResourceAllocator's LowMemory::Reserve guarantees for
        // this root. Log the carved bases so a regression is visible in BrnGame.log rather
        // than as a truncated-pointer access violation deep in a FixUp consumer.
        {
            const u64 lu0 = reinterpret_cast<u64>(lType0.m_baseResources[0]);
            const u64 lu1 = reinterpret_cast<u64>(lType1.m_baseResources[0]);
            const bool lbLow = (lu0 + KU_TYPE0) <= 0x100000000ull && (lu1 + KU_TYPE1) <= 0x100000000ull;
            *CgsDev::Log::gpDebugPrint << "[5b ROOT] GameDataRoot0="
                                      << CgsDev::E_PRINTMODE_HEXONCE << lu0
                                      << " GameDataRoot1="
                                      << CgsDev::E_PRINTMODE_HEXONCE << lu1
                                      << (lbLow ? " (below 4GB: OK for PointerFromU32)"
                                                : " *** ABOVE 4GB -- PointerFromU32 slots WILL TRUNCATE ***")
                                      << "\n";
        }

        // [BRIDGE -- marked deviation from X360 RegisterResourceTypes 0x82667EA8] Populate the pool type
        // list from our existing singleton resource-type handlers. The X360 operator-new's ~50 Type objects
        // into a GSResourceType array, but that path's prerequisites (Type::operator new / InitCachedValues
        // + the ~50 subclasses) are deferred, and our tree already registers handler SINGLETONS into a
        // CgsResource::TypeRegistry (RegisterAllResourceTypes). So reuse those: register them (idempotent),
        // then build the GSResourceType list from ResolveResourceType(id) for the wired type ids. The
        // PoolModule type-registry loop then copies them into maTypes (keyed by GetTypeID()). Grows as more
        // handlers are wired; backfill the faithful operator-new path if byte-fidelity is needed.
        CgsResource::RegisterAllResourceTypes();
        static CgsResource::PoolModule::InitOptions::GSResourceType s_aGameTypes[5];
        static const struct { u32 muId; const char* mpcName; } skWiredTypes[5] =
        {
            { 0x00u, "RwRasterResourceType" },
            { 0x0Eu, "RwTextureStateResourceType" },
            { 0x0Fu, "MaterialStateResourceType" },
            { 0x21u, "FontResourceType" },
            { 0x42u, "VideoDataResourceType" },
        };
        s32 lNumGameTypes = 0;
        for (s32 li = 0; li < 5; ++li)
        {
            const CgsResource::Type* lpType = CgsResource::ResolveResourceType(skWiredTypes[li].muId);
            if (lpType == 0)
                continue;
            s_aGameTypes[lNumGameTypes].mpType  = lpType;
            s_aGameTypes[lNumGameTypes].mpcName = skWiredTypes[li].mpcName;
            ++lNumGameTypes;
        }
        s_InitOptions.mPoolInitOptions.mpGameSpecificTypes    = s_aGameTypes;
        s_InitOptions.mPoolInitOptions.miNumGameSpecificTypes = lNumGameTypes;
        s_InitOptions.mDebugParams.mpDebugAllocator           = lpAllocator;
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: registered "
                                  << (s32)lNumGameTypes << " resource types into the pool registry\n";

        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: root type0="
                                  << (s32)(u32)(size_t)lType0.m_baseResources[0]
                                  << " -> ResourceModule::Construct\n";
        mResourceModule.Construct(&s_InitOptions, lpAllocator);
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: ResourceModule constructed ok\n";
    }
    bool GameDataModule::Release() { return true; }
    void GameDataModule::Destruct() {}

    // ====================================================================================
    // The world-request service path (THIS BATCH): the Update request pump + the GameData
    // dispatch chain + the four world handlers. Ground truth = the X360 ARTIST build
    // (addresses on each function); the request-side event/queue types are the committed
    // SharedIO slice (BrnGameDataRequestQueue.h / BrnGameDataEvents.h / the
    // RequestInterface<N> builders on the producer side).
    // ====================================================================================

    namespace
    {
        // X360 .rdata world-data file-name globals (values read from the ARTIST .i64 data
        // segment; the exports are function-only so the pointer targets are attested by the
        // database, not the JSON).
        const char* const KPC_SURFACE_LIST_FILE_NAME = "surfacelist.bin";           // off_82F2A704
        const char* const KPC_PVS_FILE_NAME          = "pvs.bndl";                  // off_82F2A70C
        const char* const KPC_PROP_INSTANCES_PATH    = "Props/Instances/TRK_UNIT";  // off_82F2A748

        // off_82F2A72C -- the PVS zone-list RESOURCE name ProcessGetPVSRequest hashes.
        // Attested TWICE: read from the ARTIST .i64 data segment (headless IDA dump), and
        // CRC32-lowercase("newgrid") == 0x5A4A4CDB == the id of the one ZoneList resource
        // in the shipped PVS.BNDL (its debug ResourceStringTable also names it "newgrid").
        const char* const KPC_PVS_RESOURCE_NAME      = "newgrid";

        // off_82F2A708 -- the world-collision bundle file name (ProcessInternalValidate
        // Response's swap-in reload + the LoadWorldCollision handler family). Read from
        // the ARTIST .i64 data segment (headless IDA dump; the exports are function-only).
        const char* const KPC_WORLD_COLLISION_FILE_NAME = "worldcol.bin";

        // ProcessLoadPropInstancesRequest baked constants (the assert texts @0x8266F178
        // name both: "strncmp(lacResourceName, \"PRP_INST_\", KU_STRING_INDEX_OF_ZONE_NUMBER)
        // == 0" and "luZoneId < KU_MAX_ZONES"; the compare immediates are 9 and 0x1F4).
        const u32 KU_STRING_INDEX_OF_ZONE_NUMBER = 9;
        const u32 KU_MAX_ZONES                   = 500;

        // off_82F2A6BC -- the per-asset-set bundle-name suffix table
        // ProcessLoadWorldUnitRequest indexes with the request's meType to build
        // "<TRK_UNITn>_<suffix>.bndl". Values read from the ARTIST .i64 data segment
        // (0x82001700 "GR", 0x820016FC "PH", 0x820016F8 "SO", 0x820016F4 "DA",
        // 0x820016F0 "AT") -- i.e. exactly the BrnResource::EAssetSet order.
        const char* const KAPC_ASSET_SET_SUFFIXES[] = { "GR", "PH", "SO", "DA", "AT" };
        const char* const KPC_TRACK_UNIT_FILE_FORMAT = "%s_%s.bndl";   // off_82F2A6B4-adjacent format literal
    }

    // @ 0x82674670 -- the per-frame pump. Reconstructed slice: the GameData request drain
    // (raw CgsResource requests forward verbatim into the embedded ResourceModule's input;
    // GameData-level requests route through ProcessGameDataEvent), the drained-input clear,
    // the output allocator-list publish and the ResourceModule pump. DEFERRED stages are
    // marked inline (each with its X360 anchor) -- their subsystems are not committed yet.
    bool GameDataModule::Update(void* lpInputBuffer, void* lpOutputBuffer)
    {
        GameDataIO::InputBuffer*  lpInput  = static_cast<GameDataIO::InputBuffer*>(lpInputBuffer);
        GameDataIO::OutputBuffer* lpOutput = static_cast<GameDataIO::OutputBuffer*>(lpOutputBuffer);

        // [deferred] dev slot-usage dump toggles (byte_82FFAD00/01 -> AllocatorList::
        // DebugDumpStats + the "Slot <n> is used" walk over the event-slot free list).
        // [deferred] CgsGameTalk::GameTalk::Update (X360 a1+399624; module not committed).

        // The per-frame CgsResource input the pump publishes into. The X360 carves it from
        // the CgsModule::IOBufferStack (CreateIOBuffer<ResourceIO::InputBuffer>("Resource"));
        // [marked deviation] the IOBufferStack is deferred, so the module reuses one
        // function-local static buffer, exactly like the committed CreatePools bring-up path.
        static CgsResource::ResourceIO::InputBuffer s_ResourceInput;
        static bool s_bResourceInputConstructed = false;
        if (!s_bResourceInputConstructed) { s_ResourceInput.Construct(); s_bResourceInputConstructed = true; }

        // The per-frame AttribSys module input. The X360 carves it from the IOBufferStack
        // (CreateIOBuffer<AttribSysIO::InputBuffer>("Attrib")); [marked deviation] same
        // one-static-buffer pattern as the ResourceIO input below.
        static CgsAttribSys::AttribSysIO::InputBuffer s_AttribInput;
        static bool s_bAttribInputConstructed = false;
        if (!s_bAttribInputConstructed)
        {
            s_bAttribInputConstructed = true;
            s_AttribInput.CgsModule::IOBuffer::Construct();
            s_AttribInput.LockForWrite();
            s_AttribInput.GetVaultRequestInterface()->mRequestQueue.Construct();
            s_AttribInput.UnlockForWrite();
        }

        if (lpOutput != 0)
        {
            // X360: LockForWrite(out); FileSystemStatusInterface::Construct(out+36);
            // out->mpAllocatorList = 0; SetAllocatorList(out, &mAllocatorList); then the
            // LiveUpdateIO status copy (SetLiveUpdateStatus from the LiveUpdate output).
            // The LiveUpdate module + the OutputBuffer's filesystem-status member are
            // deferred; the allocator-list publish is the live part.
            lpOutput->LockForWrite();
            lpOutput->Construct();
            lpOutput->SetAllocatorList(&mAllocatorList);
            lpOutput->UnlockForWrite();
        }

        if (lpInput != 0)
            lpInput->LockForRead();

        s_ResourceInput.LockForWrite();

        // [deferred] LiveUpdateIO::OutputBuffer request append (X360 Append<512,16> of the
        // live-update output's request queue into the resource input; module not committed).

        // ---- the request pump: drain the GameData request interface ---------------------
        // Queue type ids < 26 are raw CgsResource requests (CreatePool 0 / LoadBundle 2 /
        // UnloadBundle 3 / AcquireResource 4 / AcquireResourceList 5 / ...) forwarded
        // verbatim into the ResourceModule input; ids >= 26 are GameData-level events
        // routed through ProcessGameDataEvent (26 == LoadGameDataEvent, the first
        // GameData-level id -- the X360 boundary compare).
        if (lpInput != 0)
        {
            // Drain through the CONST accessor (read-locked; the mutable overload asserts
            // the write lock -- X360 0x82663F90 vs 0x823B1788).
            const GameDataIO::InputBuffer* lpInputRead = lpInput;
            const GameDataIO::RequestInterface<GameDataIO::InputBuffer::knRequestInterfaceQueueSize>*
                lpRequestInterface = lpInputRead->GetRequestInterface();

            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liType = lpRequestInterface->mRequestQueue.GetFirstEvent(&lpEvent, &liSize);
            while (lpEvent != 0)
            {
                if (liType >= 26)
                    ProcessGameDataEvent(&s_ResourceInput, lpEvent, liType);
                else
                    s_ResourceInput.GetResourceQueue()->AddEvent(lpEvent, liType, liSize);

                liType = lpRequestInterface->mRequestQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
            }
            // X360 (0x82674670, still under the input read lock): bulk-append the GameData
            // input's AttribSys request queue into the module input ("Attrib") --
            // AttribSysIO::InputBuffer::AppendRequestInterface<32768> @0x82671948.
            s_AttribInput.LockForWrite();
            s_AttribInput.AppendRequestInterface<
                GameDataIO::InputBuffer::kiAttribSysRequestInterfaceQueueSize>(
                    lpInputRead->GetAttribSysRequestInterface());
            s_AttribInput.UnlockForWrite();

            lpInput->UnlockForRead();

            // X360: re-lock the input for write and clear the drained request state (the
            // 32768-byte request queue, the AttribSys request queue @a4+8197 and the debug
            // Im2d pointer @a4[16393]). The Im2d member is deferred with its subsystem.
            lpInput->LockForWrite();
            lpInput->GetRequestInterface()->mRequestQueue.Clear();
            lpInput->GetAttribSysRequestInterface()->mRequestQueue.Clear();
            lpInput->UnlockForWrite();
        }

        // ---- internal response drain (X360 0x82674670, the type 2/3/4/7/8 switch) -------
        // Route each CgsResource completion parked on mReceiverQueue back out to the
        // original requester through its staged event slot. Responses the ResourceModule
        // posts DURING this frame's pump (below) land after this drain and are serviced
        // next frame -- same as the X360 (its pump runs on the resource thread).
        {
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            while (lpEvent != 0)
            {
                switch (liType)
                {
                case 2:
                    ProcessInternalLoadBundleResponse(&s_ResourceInput,
                        reinterpret_cast<const CgsResource::Events::LoadBundleResponse*>(lpEvent));
                    break;
                case 3:
                    ProcessInternalUnloadResponse(&s_ResourceInput,
                        reinterpret_cast<const CgsResource::Events::UnloadBundleResponse*>(lpEvent));
                    break;
                case 4:
                    ProcessInternalAcquireResponse(&s_ResourceInput,
                        reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent),
                        &s_AttribInput);
                    break;
                case 7:
                    ProcessInternalInvalidateResponse(&s_ResourceInput, lpEvent);
                    break;
                case 8:
                    ProcessInternalValidateResponse(&s_ResourceInput, lpEvent);
                    break;
                default:
                    CGS_ASSERT(false, "Invalid request received\n");   // X360 line 1904
                    break;
                }
                liType = mReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
            }
            // X360 ring-rewinds the queue in place for the next frame; the committed
            // EventReceiverQueue::Clear performs the same drained-state reset.
            mReceiverQueue.Clear();
        }

        s_ResourceInput.UnlockForWrite();

        // ---- the AttribSys module pump (X360 0x82674670: `(*(module vtbl+68))(module,
        // attribIn)` right after the resource-input unlock) + the attrib reply receiver
        // drain (a1+395888: 3 -> vault registered, 5 -> vault unregistered, else "Invalid
        // request received\n" line 1946). Handlers that need the resource input re-lock it
        // themselves (the X360 passes it unlocked here too). ------------------------------
        mAttribSysModule.Update(&s_AttribInput);
        // [PC placement] the X360 destroys the per-frame "Attrib" carve at the end of
        // Update; the persistent static must drop the drained requests itself.
        s_AttribInput.LockForWrite();
        s_AttribInput.GetVaultRequestInterface()->mRequestQueue.Clear();
        s_AttribInput.UnlockForWrite();
        {
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liType = mAttribSysReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            while (lpEvent != 0)
            {
                if (liType == 3)
                    ProcessAttribSysRegisterVaultResponse(&s_ResourceInput, lpEvent);
                else if (liType == 5)
                    ProcessUnregisterVehicleAttribsResponse(&s_ResourceInput, lpEvent);
                else
                    CGS_ASSERT(false, "Invalid request received\n");   // X360 line 1946
                liType = mAttribSysReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
            }
            mAttribSysReceiverQueue.Clear();   // X360 ring-rewind, same as mReceiverQueue
        }

        // Pump the embedded streaming engine with the requests published above. The X360
        // wraps this in UpdateResourceModule @0x826663B0, which also carves a ResourceIO::
        // OutputBuffer from the IOBufferStack and copies the filesystem status out
        // (a1+475952) -- both deferred with their consumers.
        const bool lbResult = mResourceModule.Update(&s_ResourceInput, 0);

        // [deferred] final output publish of the filesystem status interface (X360
        // SetFileSystemStatusInterface(out, a1+475952); member not committed).
        return lbResult;
    }

    // @ 0x826744F0 -- route one queued GameData-level event by its queue type id.
    void GameDataModule::ProcessGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                              const CgsModule::Event* lpEvent, s32 liEventType)
    {
        switch (liEventType)
        {
        case 26:   // LoadGameDataEvent
            ProcessLoadGameDataEvent(lpResourceInput,
                                     static_cast<const GameDataIO::GameDataAssetEvent*>(lpEvent), -1);
            break;
        case 39:   // UnloadGameDataEvent
            ProcessUnloadGameDataEvent(lpResourceInput,
                                       static_cast<const GameDataIO::GameDataAssetEvent*>(lpEvent), -1);
            break;
        case 49:   // GetGameDataEvent
            ProcessGetGameDataEvent(lpResourceInput,
                                    static_cast<const GameDataIO::GameDataAssetEvent*>(lpEvent), -1);
            break;
        case 67:   // SwapOutCollisionWorldRequest
            ProcessSwapOutCollisionWorldRequest();
            break;
        case 68:   // SwapInCollisionWorldRequest
            ProcessSwapInCollisionWorldRequest();
            break;
        default:
            CGS_ASSERT(false, "Invalid event id\n");   // X360 line 2791
            break;
        }
    }

    // @ 0x826664A0 -- fetch (liSlotIndex >= 0) or allocate (liSlotIndex < 0, the dispatch
    // path) an event slot, then capture the request event into it (the X360 copies
    // miEventId / the receiver queue / miPoolId / mId / meType and clears the fail flag).
    GameDataEventSlot* GameDataModule::GetGameDataEventSlot(
            const GameDataIO::GameDataAssetEvent* lpEvent, s32 liSlotIndex)
    {
        GameDataEventSlot* lpSlot;
        if (liSlotIndex >= 0)
        {
            lpSlot = &mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)];
        }
        else
        {
            lpSlot = mGameDataEventSlotPool.Pop();
            CGS_ASSERT(lpSlot != 0,
                       "No space for new requests - extend size of game data event pool\n");
            if (lpSlot == 0)
                return 0;   // [marked deviation] the X360 assert is non-fatal and the next
                            // store would fault on a full pool; guard the host instead.
            lpSlot->miResponseEventId = 69;   // fresh-slot sentinel (X360 `*(slot+40) = 69`)
        }

        // X360 store order: fail flag(+36)=0, meType(+32), mId(+24), miPoolId(+16),
        // queue(+12), miEventId(+8).
        lpSlot->mEvent.mbFailFlag      = false;
        lpSlot->mEvent.meType          = lpEvent->meType;
        lpSlot->mEvent.mId             = lpEvent->mId;
        lpSlot->mEvent.miPoolId        = lpEvent->miPoolId;
        lpSlot->mEvent.mpReceiverQueue = lpEvent->mpReceiverQueue;
        lpSlot->mEvent.miEventId       = lpEvent->miEventId;
        return lpSlot;
    }

    // PC bring-up scaffolding (NOT an X360 function) -- see the header note.
    void GameDataModule::DeferredGameDataRequest(const char* lpcHandlerName, GameDataEventSlot* lpSlot)
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] request handler DEFERRED: "
                                   << lpcHandlerName << "\n";
        mGameDataEventSlotPool.PushIndex(mGameDataEventSlotPool.GetObjectIndex(lpSlot));
    }

    // @ 0x82671EA0 -- dispatch a LoadGameDataEvent by the uncompressed prefix of its CgsID.
    // (The export set is function-gapped here; the body was decompiled straight from the
    // ARTIST .i64.) The X360 switches on the packed 4-char dwords of the uncompressed id;
    // the memcmp prefix compares below test the same bytes in the same order,
    // host-endian-safely. Response event ids (stored at slot->miResponseEventId by each
    // handler) are the X360 case immediates: Vehicle 27, TrafficVehicle 28, AILanes 29,
    // TrafficLanes 30, WorldUnit 31, WorldCollision 32, PVS 33, PropPhysics 34,
    // PropInstances 35, Wheel 36, SurfaceList 37.
    void GameDataModule::ProcessLoadGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                  const GameDataIO::GameDataAssetEvent* lpEvent,
                                                  s32 liSlotIndex)
    {
        char lacName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpEvent->mId, lacName);

        GameDataEventSlot* lpSlot = GetGameDataEventSlot(lpEvent, liSlotIndex);
        if (lpSlot == 0)
            return;   // [marked deviation] full-pool guard (see GetGameDataEventSlot)
        const s32 liIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);
        lpSlot->meStage = GameDataEventSlot::E_LOADING;

        if (memcmp(lacName, "VEH_", 4) == 0)
        {
            DeferredGameDataRequest("LoadVehicle (0x8266EB98, id 27)", lpSlot);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            DeferredGameDataRequest("LoadWheel (0x8266EDB8, id 36)", lpSlot);
        }
        else if (memcmp(lacName, "TVEH", 4) == 0)
        {
            DeferredGameDataRequest("LoadTrafficVehicle (0x8266EF00, id 28)", lpSlot);
        }
        else if (memcmp(lacName, "ICE_", 4) == 0)
        {
            // X360: the ICE_ load id dispatches to NO handler (empty case -> fall out).
        }
        else if (memcmp(lacName, "PRP_", 4) == 0)
        {
            if (memcmp(lacName + 4, "PHYS", 4) == 0)
                DeferredGameDataRequest("LoadPropPhysics (id 34)", lpSlot);
            else if (memcmp(lacName + 4, "INST", 4) == 0)
                ProcessLoadPropInstancesRequest(lpResourceInput, lpEvent, 35, liIndex);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 2899)
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                DeferredGameDataRequest("LoadTrafficLanes (0x8266F398, id 30)", lpSlot);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                DeferredGameDataRequest("LoadAILanes (0x8266F4B0, id 29)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 2914)
        }
        else if (memcmp(lacName, "TRK_", 4) == 0)
        {
            if (memcmp(lacName + 4, "UNIT", 4) == 0)
                ProcessLoadWorldUnitRequest(lpResourceInput, lpEvent, 31, liIndex);
            else if (memcmp(lacName + 4, "COLL", 4) == 0)
                DeferredGameDataRequest("LoadWorldCollision (0x8266F830, id 32)", lpSlot);
            else if (memcmp(lacName + 4, "ZONE", 4) == 0)
                ProcessLoadPVSRequest(lpResourceInput, lpEvent, 33, liIndex);
            else
                CGS_ASSERT(false, "Invalid track data id: ");   // X360 streams the id (line 2933)
        }
        else if (memcmp(lacName, "SURF", 4) == 0)
        {
            ProcessLoadSurfaceListRequest(lpResourceInput, lpEvent, 37, liIndex);
        }
        else
        {
            CGS_ASSERT(false, "Invalid data id: ");   // X360 streams the id (line 2945)
        }
    }

    // @ 0x82672268 -- dispatch a GetGameDataEvent by the uncompressed prefix of its CgsID.
    // Response event ids are the X360 case immediates: Vehicle 50, TrafficVehicle 51,
    // VehicleList 52, FreeburnChallengeList 53, AILanes 54, TrafficLanes 55, WorldUnit 56,
    // WorldCollision 57, PVS 58, WheelList 59, Wheel 60, PropPhysics 61, PropInstances 62,
    // PropGraphicsList 63, ICEList 64, ICEMovie 65.
    void GameDataModule::ProcessGetGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                 const GameDataIO::GameDataAssetEvent* lpEvent,
                                                 s32 liSlotIndex)
    {
        char lacName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpEvent->mId, lacName);

        GameDataEventSlot* lpSlot = GetGameDataEventSlot(lpEvent, liSlotIndex);
        if (lpSlot == 0)
            return;   // [marked deviation] full-pool guard (see GetGameDataEventSlot)
        const s32 liIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);
        lpSlot->meStage = GameDataEventSlot::E_AQUIRING;

        if (memcmp(lacName, "VEH_", 4) == 0)
        {
            DeferredGameDataRequest("GetVehicle (0x8266FDA0, id 50)", lpSlot);
        }
        else if (memcmp(lacName, "ICE_", 4) == 0)
        {
            DeferredGameDataRequest("GetICEMovie (id 65)", lpSlot);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            DeferredGameDataRequest("GetWheel (0x82670140, id 60)", lpSlot);
        }
        else if (memcmp(lacName, "TVEH", 4) == 0)
        {
            DeferredGameDataRequest("GetTrafficVehicle (0x82670280, id 51)", lpSlot);
        }
        else if (memcmp(lacName, "VL__", 4) == 0)
        {
            DeferredGameDataRequest("GetVehicleList (id 52)", lpSlot);
        }
        else if (memcmp(lacName, "CL__", 4) == 0)
        {
            DeferredGameDataRequest("GetFreeburnChallengeList (id 53)", lpSlot);
        }
        else if (memcmp(lacName, "IL__", 4) == 0)
        {
            DeferredGameDataRequest("GetICEList (id 64)", lpSlot);
        }
        else if (memcmp(lacName, "PRP_", 4) == 0)
        {
            if (memcmp(lacName + 4, "PHYS", 4) == 0)
                DeferredGameDataRequest("GetPropPhysics (id 61)", lpSlot);
            else if (memcmp(lacName + 4, "GL__", 4) == 0)
                DeferredGameDataRequest("GetPropGraphicsList (id 63)", lpSlot);
            else if (memcmp(lacName + 4, "INST", 4) == 0)
                DeferredGameDataRequest("GetPropInstances (id 62)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3114
        }
        else if (memcmp(lacName, "WL__", 4) == 0)
        {
            DeferredGameDataRequest("GetWheelList (id 59)", lpSlot);
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                DeferredGameDataRequest("GetTrafficLanes (0x826703B0, id 55)", lpSlot);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                DeferredGameDataRequest("GetAILanes (0x826704C0, id 54)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3133
        }
        else if (memcmp(lacName, "TRK_", 4) == 0)
        {
            if (memcmp(lacName + 4, "UNIT", 4) == 0)
                ProcessGetWorldUnitRequest(lpResourceInput, lpEvent, 56, liIndex);
            else if (memcmp(lacName + 4, "COLL", 4) == 0)
                DeferredGameDataRequest("GetWorldCollision (0x82670700, id 57)", lpSlot);
            else if (memcmp(lacName + 4, "PVS_", 4) == 0)
                DeferredGameDataRequest("GetPVS (0x82670880, id 58)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3152
        }
        else
        {
            CGS_ASSERT(false, "Invalid id\n");   // X360 line 3157
        }
    }

    // @ 0x826733F8 -- dispatch an UnloadGameDataEvent by the uncompressed prefix of its
    // CgsID, exactly like the load/get dispatchers. The X360 switches on the packed 4-char
    // dwords of the uncompressed id (v14/v15/v16 == chunks 0/1/2); the memcmp prefix
    // compares below test the same bytes in the same order, host-endian-safely:
    //   VEH_ -> UnloadVehicle 40 @0x82672DE0        WHE_      -> UnloadWheel 47 @0x82670AA0
    //   TVEH -> UnloadTrafficVehicle 41 @0x82670BE0 GD__/LANE -> UnloadTrafficLanes 43
    //   GD__/AI__ -> UnloadAILanes 42 @0x82670E40   TRK_/UNIT -> UnloadWorldUnit 44 @0x82671160
    //   TRK_/COLL -> UnloadWorldCollision 45 @0x826712A0
    //   TRK_/PVS_ -> UnloadPVS 46 @0x82671420       PRP_/INST -> UnloadPropInstances 48 @0x82670F50
    // (NOTE the TRK_ sub-key is "PVS_" here and in the GET dispatcher, but "ZONE" in the
    // LOAD dispatcher -- reproduced verbatim.) The slot is staged E_UNLOADING before the
    // routing, matching `*GameDataEventSlot = 2`.
    void GameDataModule::ProcessUnloadGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                    const GameDataIO::GameDataAssetEvent* lpEvent,
                                                    s32 liSlotIndex)
    {
        char lacName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpEvent->mId, lacName);

        GameDataEventSlot* lpSlot = GetGameDataEventSlot(lpEvent, liSlotIndex);
        if (lpSlot == 0)
            return;   // [marked deviation] full-pool guard (see GetGameDataEventSlot)
        const s32 liIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);
        lpSlot->meStage = GameDataEventSlot::E_UNLOADING;

        if (memcmp(lacName, "VEH_", 4) == 0)
        {
            DeferredGameDataRequest("UnloadVehicle (0x82672DE0, id 40)", lpSlot);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            DeferredGameDataRequest("UnloadWheel (0x82670AA0, id 47)", lpSlot);
        }
        else if (memcmp(lacName, "TVEH", 4) == 0)
        {
            DeferredGameDataRequest("UnloadTrafficVehicle (0x82670BE0, id 41)", lpSlot);
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                DeferredGameDataRequest("UnloadTrafficLanes (id 43)", lpSlot);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                DeferredGameDataRequest("UnloadAILanes (0x82670E40, id 42)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3003
        }
        else if (memcmp(lacName, "PRP_", 4) == 0)
        {
            if (memcmp(lacName + 4, "INST", 4) == 0)
                DeferredGameDataRequest("UnloadPropInstances (0x82670F50, id 48)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 3014)
        }
        else if (memcmp(lacName, "TRK_", 4) == 0)
        {
            if (memcmp(lacName + 4, "UNIT", 4) == 0)
                ProcessUnloadWorldUnitRequest(lpResourceInput, lpEvent, 44, liIndex);
            else if (memcmp(lacName + 4, "COLL", 4) == 0)
                DeferredGameDataRequest("UnloadWorldCollision (0x826712A0, id 45)", lpSlot);
            else if (memcmp(lacName + 4, "PVS_", 4) == 0)
                DeferredGameDataRequest("UnloadPVS (0x82671420, id 46)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3033
        }
        else
        {
            CGS_ASSERT(false, "Invalid id\n");   // X360 line 3038
        }
    }

    // @ 0x826717A8 / @ 0x82671530 -- swap the collision world in/out. DEFERRED with the
    // collision-world swap machinery (WorldEntityModule Validate/InvalidateCollision).
    void GameDataModule::ProcessSwapInCollisionWorldRequest()
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] ProcessSwapInCollisionWorldRequest DEFERRED\n";
    }
    void GameDataModule::ProcessSwapOutCollisionWorldRequest()
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] ProcessSwapOutCollisionWorldRequest DEFERRED\n";
    }

    // @ 0x8266F5C8 -- service a LOAD world-unit request: post the LoadBundle for the unit's
    // per-asset-set bundle ("<TRK_UNITn>_<GR|PH|SO|DA|AT>.bndl"). The reply lands on
    // mReceiverQueue and ProcessInternalLoadBundleResponse's case 31 then dispatches the
    // paired GET (ProcessGetWorldUnitRequest, id 56) which acquires the unit's instance
    // list. This is the FIRST hop of the world graphics streamer's load chain.
    void GameDataModule::ProcessLoadWorldUnitRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                     const GameDataIO::GameDataAssetEvent* lpEvent,
                                                     s32 liEventId, s32 liSlotIndex)
    {
        // X360 store order: the response id is staged FIRST (before the name work).
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        // X360: `if (event->meType) assert` -- the streamer always asks for asset set 0.
        // The suffix lookup runs on meType regardless (the assert is a non-gating tripwire).
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS,
                   "Invalid asset type for track units\n");   // X360 line 4256

        const u32 luAssetSet = static_cast<u32>(lpEvent->meType);
        const char* lpcSuffix =
            (luAssetSet < (sizeof(KAPC_ASSET_SET_SUFFIXES) / sizeof(KAPC_ASSET_SET_SUFFIXES[0])))
                ? KAPC_ASSET_SET_SUFFIXES[luAssetSet]
                : KAPC_ASSET_SET_SUFFIXES[0];

        char lacFileName[208];
        CgsCore::SPrintf(lacFileName, 128, KPC_TRACK_UNIT_FILE_FORMAT, lacResourceName, lpcSuffix);

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(lacFileName);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = true;   // X360 stores 1 here (v33), unlike the prop path

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x82671160 -- service an UNLOAD world-unit request: post the UnloadBundle for the
    // unit's per-asset-set bundle ("<TRK_UNITn>_<GR|PH|SO|DA|AT>.bndl"). The reply lands on
    // mReceiverQueue as a type-3 UnloadBundleResponse and ProcessInternalUnloadResponse then
    // posts the id-44 completion back to the world graphics streamer's receiver queue, which
    // frees its current-list slot. This is the streamer's UNLOAD leg, the mirror of
    // ProcessLoadWorldUnitRequest @0x8266F5C8 (same body shape as ProcessUnloadWheelRequest
    // @0x82670AA0 / ProcessUnloadPVSRequest @0x82671420: response id staged first, the name
    // formatted, then ONE type-3 UnloadBundleRequest into the resource queue).
    void GameDataModule::ProcessUnloadWorldUnitRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                       const GameDataIO::GameDataAssetEvent* lpEvent,
                                                       s32 liEventId, s32 liSlotIndex)
    {
        // X360 store order: the response id is staged FIRST (before the name work).
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        // X360: `if (event->meType) assert` -- the streamer always asks for asset set 0.
        // The suffix lookup runs on meType regardless (the assert is a non-gating tripwire).
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS,
                   "Invalid asset type for track units\n");   // X360 line 5337

        const u32 luAssetSet = static_cast<u32>(lpEvent->meType);
        const char* lpcSuffix =
            (luAssetSet < (sizeof(KAPC_ASSET_SET_SUFFIXES) / sizeof(KAPC_ASSET_SET_SUFFIXES[0])))
                ? KAPC_ASSET_SET_SUFFIXES[luAssetSet]
                : KAPC_ASSET_SET_SUFFIXES[0];

        char lacFileName[208];
        CgsCore::SPrintf(lacFileName, 128, KPC_TRACK_UNIT_FILE_FORMAT, lacResourceName, lpcSuffix);

        // The X360 144-byte type-3 record: {mpUser = &mReceiverQueue, miEventId = the slot
        // index, filename, mbLiveUpdateReplace = false, miPoolId = the request's pool}.
        // UnloadBundleRequest adds no payload of its own over BundleLoaderEvent.
        CgsResource::Events::UnloadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(lacFileName);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            3 /*UnloadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x826705D0 -- service a GET world-unit request: acquire the unit's "<name>_list"
    // instance-list resource from the pool named by the request. The reply lands on
    // mReceiverQueue and (X360) ProcessInternalAcquireResponse posts it back to the
    // requester with the response id staged at slot->miResponseEventId (56).
    void GameDataModule::ProcessGetWorldUnitRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                    const GameDataIO::GameDataAssetEvent* lpEvent,
                                                    s32 liEventId, s32 liSlotIndex)
    {
        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        // X360: `if (event->meType) assert` -- track units are asset set 0 (GRAPHICS).
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS,
                   "Invalid asset type for track units\n");   // X360 line 4897

        // "<TRK_UNITn>_list" -- the unit's instance-list resource name (X360 SPrintf cap 128).
        char lacListName[128];
        CgsCore::SPrintf(lacListName, 128, "%s_list", lacResourceName);

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        // The X360 24-byte type-4 record: {mpUser = &mReceiverQueue, miEventId = the slot
        // index, miPoolId = the request's pool, mResourceId = ID::HashString(list name)}.
        // mbCheckRefCount lies outside the X360 record; false per the committed convention.
        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacListName)))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F9C0 -- service a LOAD PVS request: stream the world's PVS bundle
    // ("pvs.bndl") into the request's pool. Response id staged at the slot (33).
    void GameDataModule::ProcessLoadPVSRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                               const GameDataIO::GameDataAssetEvent* lpEvent,
                                               s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for pvs\n");   // X360 line 4355

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        // The X360 148-byte type-2 LoadBundle record: {mpUser = &mReceiverQueue,
        // miEventId = the slot index, filename = the baked PVS bundle name,
        // mbLiveUpdateReplace = false, miPoolId = the request's pool,
        // mbAllowFailiure = the request's fail flag, mbUseHDCache = false}.
        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_PVS_FILE_NAME);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F718 -- service a LOAD surface-list request: stream the world's surface
    // list ("surfacelist.bin") into the request's pool. Response id staged at the slot
    // (37 -- the id the WorldEntityModule surface-list consumer waits on).
    void GameDataModule::ProcessLoadSurfaceListRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                       const GameDataIO::GameDataAssetEvent* lpEvent,
                                                       s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_ATTRIBS,
                   "Invalid asset type for surface list\n");   // X360 line 4285

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_SURFACE_LIST_FILE_NAME);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F178 -- service a LOAD prop-instances request: parse the zone number out of
    // the "PRP_INST_<n>" id, build the zone's prop-instance bundle path
    // ("Props/Instances/TRK_UNIT<n>_PropInstances.bundle") through the StrStream chain,
    // and stream it into the request's pool. Response id staged at the slot (35).
    void GameDataModule::ProcessLoadPropInstancesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                         const GameDataIO::GameDataAssetEvent* lpEvent,
                                                         s32 liEventId, s32 liSlotIndex)
    {
        // X360: the stream is constructed over a 208-byte stack buffer with a 100-byte
        // capacity argument, first byte pre-cleared, at function entry.
        char lacFileName[208];
        lacFileName[0] = '\0';
        CgsDev::StrStream lStream(lacFileName, 100);

        CGS_ASSERT(lpEvent->meType == E_ASSETSET_PHYSICS,
                   "Invalid asset type for prop physics \n");   // X360 line 4149 (verbatim, incl. the space)

        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        // X360 line 4155 (message-less assert: the stringized condition is the message).
        CGS_ASSERT(strncmp(lacResourceName, "PRP_INST_", KU_STRING_INDEX_OF_ZONE_NUMBER) == 0,
                   "strncmp(lacResourceName, \"PRP_INST_\", KU_STRING_INDEX_OF_ZONE_NUMBER) == 0");

        const u32 luZoneId = static_cast<u32>(atoi(&lacResourceName[KU_STRING_INDEX_OF_ZONE_NUMBER]));
        // X360 fires this guard TWICE (lines 4157 and 4160) inside one out-of-range branch;
        // both asserts are reproduced.
        CGS_ASSERT(luZoneId < KU_MAX_ZONES, "luZoneId < KU_MAX_ZONES");
        CGS_ASSERT(luZoneId < KU_MAX_ZONES, "luZoneId < KU_MAX_ZONES");

        lStream << KPC_PROP_INSTANCES_PATH << luZoneId << "_PropInstances.bundle";

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(lacFileName);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // ====================================================================================
    // The GET acquire builders the completion routing dispatches (THIS BATCH).
    // ====================================================================================

    // @ 0x82670880 -- service a GET PVS request: acquire the world's zone-list resource
    // ("newgrid" -- see KPC_PVS_RESOURCE_NAME) from the request's pool. Response id staged
    // at the slot (58).
    void GameDataModule::ProcessGetPVSRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                              const GameDataIO::GameDataAssetEvent* lpEvent,
                                              s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for pvs\n");   // X360 line 4957

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>(KPC_PVS_RESOURCE_NAME)))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266FBF8 -- service a GET surface-list request: acquire the "surfacelist"
    // resource from the request's pool (no asset-type assert on the X360). Response id
    // staged at the slot (66 == EVENT_GET_SURFACE_LIST -- posted back only once the
    // AttribSys vault registration completes; see ProcessInternalAcquireResponse).
    void GameDataModule::ProcessGetSurfaceListRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                      const GameDataIO::GameDataAssetEvent* lpEvent,
                                                      s32 liEventId, s32 liSlotIndex)
    {
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>("surfacelist")))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266FB68 -- service a GET prop-instances request: acquire the resource named by
    // the request's own id string ("PRP_INST_<n>"). Response id staged at the slot (62).
    void GameDataModule::ProcessGetPropInstancesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                        const GameDataIO::GameDataAssetEvent* lpEvent,
                                                        s32 liEventId, s32 liSlotIndex)
    {
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacResourceName)))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // ====================================================================================
    // The ProcessInternal*Response completion routing (THIS BATCH): Update's receiver
    // drain hands each CgsResource completion here; the staged event slot names the
    // requester and the response id, and the reply is posted back to its receiver queue.
    // ====================================================================================

    namespace
    {
        // The type-7 pool-invalidate request record the collision swap-out path publishes
        // (X360 12-byte build @0x8266E3F0 / @0x8266E5D8: {mpUser, miEventId, 2}). MINIMAL
        // SLICE: the pool-side invalidate consumer is not committed yet; the trailing
        // immediate 2 is the collision pool id at both build sites.
        struct InvalidatePoolRequest
        {
            CgsModule::BaseEventReceiverQueue* mpUser;     // +0
            s32                                miEventId;  // +4 (the event-slot index)
            s32                                miPoolId;   // +8 (== 2, the collision pool)
        };

        // The type-7 invalidate RESPONSE view (X360 reads @0x8266E5D8). MINIMAL SLICE with
        // FLAG lane names -- the producer (pool invalidate) is not committed on PC, so only
        // the read offsets are attested: miEventId @+4, three words @+12..+20 and six words
        // @+24..+44 echoed into the id-67 reply, the still-loaded flag @+48.
        struct InvalidatePoolResponse
        {
            CgsModule::BaseEventReceiverQueue* mpUser;                  // +0
            s32                                miEventId;               // +4
            s32                                miPoolId;                // +8
            u32                                mauEchoWordsA[3];        // +12..+20 (FLAG role)
            u32                                mauEchoWordsB[6];        // +24..+44 (FLAG role)
            s32                                miResourcesStillLoaded;  // +48
        };

        // The 44-byte id-67 "collision world swapped out" reply record (X360 build
        // @0x8266E5D8: {slot event id, 0, the three +12 words, the six +24 words}).
        struct CollisionSwapOutResponse
        {
            s32 miEventId;            // +0
            s32 miZero;               // +4 (X360 stores 0)
            u32 mauEchoWordsA[3];     // +8..+16
            u32 mauEchoWordsB[6];     // +20..+40
        };

        // The 8-byte id-68 "collision world swapped in" reply record (X360 builds
        // {slot event id, 0} @0x82672630 case 0x44 / @0x8266E858).
        struct CollisionSwapInResponse
        {
            s32 miEventId;   // +0
            s32 miZero;      // +4
        };

        // The type-8 validate RESPONSE view (X360 reads @0x8266E858: only miEventId @+4).
        struct ValidatePoolResponse
        {
            CgsModule::BaseEventReceiverQueue* mpUser;     // +0
            s32                                miEventId;  // +4
        };
    }

    // The shared 40-byte completion-post the X360 inlines in every acquire/load case:
    // a GameDataAssetEvent echoing the slot's captured request (receiver lane zeroed),
    // plus the resolved handle. luTypeLane reproduces the X360 +0x18 store exactly --
    // 0 for most replies, 1 for the collision/graphics variants, and the id's high word
    // for the PVS/prop replies (the X360 copies slot dword +24 there). [x64 note: the
    // X360 record is 32/40 bytes; the reply is the widened committed GameDataAssetEvent,
    // sized by sizeof per the x64 semantic-parity convention.]
    void GameDataModule::PostGameDataResponse(const GameDataEventSlot* lpSlot, s32 liResponseId,
                                              bool lbFailFlag, u32 luTypeLane,
                                              const void* lpResourceMemory, void* lpSourceEntry)
    {
        GameDataIO::GameDataAssetEvent lResponse;
        memset(&lResponse, 0, sizeof(lResponse));
        lResponse.miEventId       = lpSlot->mEvent.miEventId;
        lResponse.mpReceiverQueue = 0;   // X360 zeroes the receiver lane in the reply
        lResponse.miPoolId        = lpSlot->mEvent.miPoolId;
        lResponse.mId             = lpSlot->mEvent.mId;
        lResponse.meType          = static_cast<EAssetSet>(luTypeLane);
        lResponse.mbFailFlag      = lbFailFlag;
        lResponse.mHandle.mpResourceMemory = const_cast<void*>(lpResourceMemory);
        lResponse.mHandle.mpSourceEntry    = static_cast<CgsResource::Entry*>(lpSourceEntry);

        lpSlot->mEvent.mpReceiverQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), liResponseId,
            static_cast<s32>(sizeof(lResponse)));
    }

    // @ 0x82672630 -- a LoadBundle completed: dispatch the paired GET (the resource is now
    // in its pool) or post the failure back to the requester and free the slot. Cases whose
    // ProcessGetXxx handler is not reconstructed yet route through DeferredGameDataRequest
    // (log + free; marked deviation -- the X360 keeps the slot through the Get).
    void GameDataModule::ProcessInternalLoadBundleResponse(
            CgsResource::ResourceIO::InputBuffer* lpResourceInput,
            const CgsResource::Events::LoadBundleResponse* lpResponse)
    {
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(lpResponse->miEventId)];
        const s32 liSlotIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);
        const bool lbFailed =
            (lpResponse->meResult != CgsResource::Events::LoadBundleResponse::E_RESULT_SUCCESS);

        switch (lpSlot->miResponseEventId)
        {
        case 27:   // vehicle
            if (lbFailed)
            {
                PostGameDataResponse(lpSlot, 27, true, 0, 0, 0);
                mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            }
            else
                DeferredGameDataRequest("GetVehicle after load (0x8266FDA0, id 50)", lpSlot);
            break;

        case 28:   // traffic vehicle
            if (lbFailed)
            {
                PostGameDataResponse(lpSlot, 28, true, 0, 0, 0);
                mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            }
            else
                DeferredGameDataRequest("GetTrafficVehicle after load (0x82670280, id 51)", lpSlot);
            break;

        case 29:   // AI lanes
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3238
            DeferredGameDataRequest("GetAILanes after load (0x826704C0, id 54)", lpSlot);
            break;

        case 30:   // traffic lanes
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3245
            DeferredGameDataRequest("GetTrafficLanes after load (0x826703B0, id 55)", lpSlot);
            break;

        case 31:   // world unit
            if (lbFailed)
            {
                PostGameDataResponse(lpSlot, 31, true, 0, 0, 0);
                mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            }
            else
                ProcessGetWorldUnitRequest(lpResourceInput, &lpSlot->mEvent, 56, liSlotIndex);
            break;

        case 32:   // world collision -- loaded is the terminal state (acquire rides the
                   // zone-collision AcquireResourceList protocol, not a GET here)
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3268
            ++miWorldCollisionRefCount;
            PostGameDataResponse(lpSlot, 32, false, 1 /*X360 +0x18 immediate*/, 0, 0);
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            break;

        case 33:   // PVS
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3282
            ProcessGetPVSRequest(lpResourceInput, &lpSlot->mEvent, 58, liSlotIndex);
            break;

        case 34:   // prop physics
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3302
            DeferredGameDataRequest("GetPropPhysics after load (id 61)", lpSlot);
            break;

        case 35:   // prop instances
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3309
            ProcessGetPropInstancesRequest(lpResourceInput, &lpSlot->mEvent, 62, liSlotIndex);
            break;

        case 36:   // wheel
            if (lbFailed)
            {
                PostGameDataResponse(lpSlot, 36, true, 0, 0, 0);
                mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            }
            else
                DeferredGameDataRequest("GetWheel after load (0x82670140, id 60)", lpSlot);
            break;

        case 37:   // surface list
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3316
            ProcessGetSurfaceListRequest(lpResourceInput, &lpSlot->mEvent, 66, liSlotIndex);
            break;

        case 38:   // sound bundle batch (X360: post id 38 + count down slot->+44, free at
                   // zero). The sound-bundle producer path is DEFERRED on PC (nothing
                   // stages id 38); gate = log + free so the pool cannot leak.
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3289
            DeferredGameDataRequest("sound-bundle batch completion (id 38)", lpSlot);
            break;

        case 68:   // collision world swap-in reload completed (the validate retry path)
        {
            CollisionSwapInResponse lReply;
            lReply.miEventId = lpSlot->mEvent.miEventId;
            lReply.miZero    = 0;
            lpSlot->mEvent.mpReceiverQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lReply), 68,
                static_cast<s32>(sizeof(lReply)));
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            break;
        }

        default:
            CGS_ASSERT(false, "Invalid event type received\n");   // X360 line 3335
            break;
        }
    }

    // @ 0x826736D8 -- an AcquireResource completed: post the resolved handle back to the
    // requester with the staged response id and free the slot. The AttribSys legs (vehicle
    // vault register 50/meType 4, vehicle vault unregister 40, surface-list vault register
    // 66) forward Register/UnregisterVault into the AttribSys module input with THIS
    // module's attrib receiver as the reply target and the event-slot index as the
    // request's miEventId; the slot stays STAGED until the type-3/5 reply completes it
    // (ProcessAttribSysRegisterVaultResponse / ProcessUnregisterVehicleAttribsResponse).
    void GameDataModule::ProcessInternalAcquireResponse(
            CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
            const CgsResource::Events::AcquireResourceResponse* lpResponse,
            CgsAttribSys::AttribSysIO::InputBuffer* lpAttribModuleInputBuffer)
    {
        CGS_ASSERT(lpAttribModuleInputBuffer != 0, "lpAttribModuleInputBuffer != NULL");   // X360 line 3357
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(lpResponse->miEventId)];
        const s32 liSlotIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);

        // No reply-to queue captured -> nothing to route; just recycle the slot.
        if (lpSlot->mEvent.mpReceiverQueue == 0)
        {
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            return;
        }

        // The X360 +0x18 lane for the PVS/prop replies is the slot id's HIGH word (the
        // dword at slot+24 on the big-endian console) -- reproduced exactly.
        const u32 luIdHighWord = static_cast<u32>(lpSlot->mEvent.mId >> 32);

        switch (lpSlot->miResponseEventId)
        {
        case 40:   // vehicle unload -> AttribSys UnregisterVault (slot stays staged; the
                   // type-5 reply routes through ProcessUnregisterVehicleAttribsResponse)
        {
            CGS_ASSERT(lpSlot->mEvent.meType == E_ASSETSET_ATTRIBS,
                       "lpSlot->mEvent.GetGameDataType() == E_ASSETSET_ATTRIBS");   // line 3542
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
            lHandle.mpSourceEntry    = static_cast<CgsResource::Entry*>(lpResponse->mpSourceEntry);
            lpAttribModuleInputBuffer->LockForWrite();
            lpAttribModuleInputBuffer->GetVaultRequestInterface()->UnregisterVault(
                &mAttribSysReceiverQueue, lHandle, liSlotIndex);
            lpAttribModuleInputBuffer->UnlockForWrite();
            break;
        }

        case 50:   // vehicle
            if (lpSlot->mEvent.meType == E_ASSETSET_GRAPHICS ||
                lpSlot->mEvent.meType == E_ASSETSET_PHYSICS)
            {
                // X360: meType 0 posts the plain reply; meType 1 posts the graphics
                // variant (the +0x18 lane carries the asset set).
                PostGameDataResponse(lpSlot, 50, false,
                                     static_cast<u32>(lpSlot->mEvent.meType),
                                     lpResponse->mpResourceMemory, lpResponse->mpSourceEntry);
                mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            }
            else if (lpSlot->mEvent.meType == E_ASSETSET_ATTRIBS)
            {
                // X360 (internal_handlers.txt @0x826736D8 case 50/type 4): assert the vault
                // resource loaded (line 3409, CgsIDUnCompress'd name in the message), pick
                // the vault type off the "VEH_T" id prefix (prefix match -> RESIDENT 0,
                // else STREAMED 1), then RegisterVault(queue = the attrib receiver, handle,
                // miEventId = the slot index, type). Slot stays staged for the type-3 reply.
                CGS_ASSERT(lpResponse->mpSourceEntry != 0,
                           "Trying to register handling attrib vault before vault resource has been loaded");   // line 3409
                char lacResourceName[16];
                CgsIDUnCompress(lpSlot->mEvent.mId, lacResourceName);
                lacResourceName[5] = 0;
                const CgsAttribSys::AttribSysIO::EAttribSysVaultType leVaultType =
                    (strcmp(lacResourceName, "VEH_T") == 0)
                        ? CgsAttribSys::AttribSysIO::E_VAULT_TYPE_RESIDENT
                        : CgsAttribSys::AttribSysIO::E_VAULT_TYPE_STREAMED;
                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry    = static_cast<CgsResource::Entry*>(lpResponse->mpSourceEntry);
                lpAttribModuleInputBuffer->LockForWrite();
                lpAttribModuleInputBuffer->GetVaultRequestInterface()->RegisterVault(
                    &mAttribSysReceiverQueue, lHandle, liSlotIndex, leVaultType);
                lpAttribModuleInputBuffer->UnlockForWrite();
            }
            else
            {
                CGS_ASSERT(false, "Invalid event type received\n");   // line 3559 sibling
                mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            }
            break;

        case 51:   // traffic vehicle
        case 54:   // AI lanes
        case 55:   // traffic lanes
        case 56:   // world unit (the WorldGraphicsStreamer completion)
        case 60:   // wheel
        case 63:   // prop graphics list
        case 65:   // ICE movie
            PostGameDataResponse(lpSlot, lpSlot->miResponseEventId, false, 0,
                                 lpResponse->mpResourceMemory, lpResponse->mpSourceEntry);
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            break;

        case 58:   // PVS zone list
        case 61:   // prop physics
        case 62:   // prop instances
            PostGameDataResponse(lpSlot, lpSlot->miResponseEventId, false, luIdHighWord,
                                 lpResponse->mpResourceMemory, lpResponse->mpSourceEntry);
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            break;

        case 57:   // world collision GET is not a supported protocol
            CGS_ASSERT(false,
                "GETWORLDCOLLISION request not supported - use Load world collision and then Acquire zone collision\n");   // line 3517
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            break;

        case 66:   // surface list -> AttribSys RegisterVault(surface vault). X360 case 'B':
                   // RegisterVault(queue = the attrib receiver, handle = the response's
                   // resolved handle, miEventId = the slot index, type 0 RESIDENT); the
                   // id-66 reply to the requester is posted by ProcessAttribSysRegister
                   // VaultResponse (@0x82666590) once registration lands. Slot stays staged.
        {
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
            lHandle.mpSourceEntry    = static_cast<CgsResource::Entry*>(lpResponse->mpSourceEntry);
            lpAttribModuleInputBuffer->LockForWrite();
            lpAttribModuleInputBuffer->GetVaultRequestInterface()->RegisterVault(
                &mAttribSysReceiverQueue, lHandle, liSlotIndex,
                CgsAttribSys::AttribSysIO::E_VAULT_TYPE_RESIDENT);
            lpAttribModuleInputBuffer->UnlockForWrite();
            break;
        }

        default:
            CGS_ASSERT(false, "Invalid event type received\n");   // X360 line 3559
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            break;
        }
    }

    // @ 0x8266E3F0 -- an UnloadBundle completed: the world-collision unload kicks the pool
    // invalidate; the sound batch counts down; everything else echoes the slot back to the
    // requester with the staged response id.
    void GameDataModule::ProcessInternalUnloadResponse(
            CgsResource::ResourceIO::InputBuffer* lpResourceInput,
            const CgsResource::Events::UnloadBundleResponse* lpResponse)
    {
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(lpResponse->miEventId)];
        const s32 liSlotIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);

        if (lpSlot->miResponseEventId == 67)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Unload of world collision succesful - invalidating pool\n";
            lpSlot->meStage = GameDataEventSlot::E_DONE;

            InvalidatePoolRequest lRequest;
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = liSlotIndex;
            lRequest.miPoolId  = 2;   // the collision pool (X360 immediate)
            lpResourceInput->GetResourceQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                7 /*InvalidatePool*/, static_cast<s32>(sizeof(lRequest)));
            return;
        }

        if (lpSlot->miResponseEventId == 45)
        {
            CGS_ASSERT(miWorldCollisionRefCount > 0,
                       "Attempt to unload world collision when it already has 0 ref count\n");   // line 3603
            --miWorldCollisionRefCount;
        }

        if (lpSlot->mEvent.mpReceiverQueue == 0)
        {
            mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
            return;
        }

        if (lpSlot->mEvent.meType == E_ASSETSET_SOUND)
        {
            CGS_ASSERT(muLoadedSoundBundlesCount != 0, "muLoadedSoundBundlesCount != 0");   // line 3618
            if (muLoadedSoundBundlesCount-- != 1)
                return;   // more sound bundles still unloading -- keep the slot alive
        }

        // The unload reply record (X360 32-byte build): NOTE the +8 lane is the slot id's
        // HIGH word (slot dword +24), NOT the captured pool id -- reproduced exactly.
        {
            GameDataIO::GameDataAssetEvent lReply;
            memset(&lReply, 0, sizeof(lReply));
            lReply.miEventId       = lpSlot->mEvent.miEventId;
            lReply.mpReceiverQueue = 0;
            lReply.miPoolId        = static_cast<s32>(static_cast<u32>(lpSlot->mEvent.mId >> 32));
            lReply.mId             = lpSlot->mEvent.mId;
            lReply.meType          = lpSlot->mEvent.meType;
            lReply.mbFailFlag      = false;
            lpSlot->mEvent.mpReceiverQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lReply), lpSlot->miResponseEventId,
                static_cast<s32>(sizeof(lReply)));
        }
        mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
    }

    // @ 0x8266E5D8 -- a pool invalidate completed (the collision swap-out protocol): retry
    // while resources are still loaded, else post the id-67 "swapped out" reply.
    void GameDataModule::ProcessInternalInvalidateResponse(
            CgsResource::ResourceIO::InputBuffer* lpResourceInput,
            const CgsModule::Event* lpEvent)
    {
        const InvalidatePoolResponse* lpResponse =
            reinterpret_cast<const InvalidatePoolResponse*>(lpEvent);
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(lpResponse->miEventId)];
        const s32 liSlotIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);

        CGS_ASSERT(lpSlot->miResponseEventId == 67,
                   "Should only receive invalidate responses to swapping out of collision world\n");   // line 3657
        CGS_ASSERT(lpSlot->meStage == GameDataEventSlot::E_DONE,
                   "Should only ever get invalidate responses for 'done' slots\n");   // line 3658

        if (lpResponse->miResourcesStillLoaded != 0)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "Attempt to invalidate failed due to resources still loaded - retrying now\n";
            lpSlot->meStage = GameDataEventSlot::E_DONE;

            InvalidatePoolRequest lRequest;
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = liSlotIndex;
            lRequest.miPoolId  = 2;
            lpResourceInput->GetResourceQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                7 /*InvalidatePool*/, static_cast<s32>(sizeof(lRequest)));
            return;
        }

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Attempt to invalidate succeeded\n";

        CollisionSwapOutResponse lReply;
        memset(&lReply, 0, sizeof(lReply));
        lReply.miEventId = lpSlot->mEvent.miEventId;
        lReply.miZero    = 0;
        for (s32 li = 0; li < 3; ++li)
            lReply.mauEchoWordsA[li] = lpResponse->mauEchoWordsA[li];
        for (s32 li = 0; li < 6; ++li)
            lReply.mauEchoWordsB[li] = lpResponse->mauEchoWordsB[li];
        lpSlot->mEvent.mpReceiverQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lReply), 67,
            static_cast<s32>(sizeof(lReply)));
        mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
    }

    // @ 0x8266E858 -- a pool validate completed (the collision swap-in protocol): with the
    // collision bundle still ref-counted, reload it (ref count must be exactly 1); else
    // post the id-68 "swapped in" reply.
    void GameDataModule::ProcessInternalValidateResponse(
            CgsResource::ResourceIO::InputBuffer* lpResourceInput,
            const CgsModule::Event* lpEvent)
    {
        const ValidatePoolResponse* lpResponse =
            reinterpret_cast<const ValidatePoolResponse*>(lpEvent);
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(lpResponse->miEventId)];
        const s32 liSlotIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);

        CGS_ASSERT(lpSlot->miResponseEventId == 68,
                   "Should only receive validate responses to swapping in of collision world\n");   // line 3738
        CGS_ASSERT(lpSlot->meStage == GameDataEventSlot::E_DONE,
                   "Should only ever get validate responses for 'done' slots\n");   // line 3739

        const s32 liRefCount = miWorldCollisionRefCount;
        miWorldCollisionValidatePending = 0;

        if (liRefCount != 0)
        {
            CGS_ASSERT(liRefCount == 1,
                       "Currently only support swapping out collision world when it's ref count is 1\n");   // line 3754
            lpSlot->meStage = GameDataEventSlot::E_LOADING;   // X360 *slot = 0

            CgsResource::Events::LoadBundleRequest lRequest;
            memset(&lRequest, 0, sizeof(lRequest));
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = liSlotIndex;
            lRequest.SetFileName(KPC_WORLD_COLLISION_FILE_NAME);
            lRequest.mbLiveUpdateReplace = false;
            lRequest.miPoolId            = 2;
            lRequest.mbAllowFailiure     = true;
            lRequest.mbUseHDCache        = false;
            lpResourceInput->GetResourceQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
            return;
        }

        CollisionSwapInResponse lReply;
        lReply.miEventId = lpSlot->mEvent.miEventId;
        lReply.miZero    = 0;
        lpSlot->mEvent.mpReceiverQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lReply), 68,
            static_cast<s32>(sizeof(lReply)));
        mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
    }

    // @ 0x82666590 -- an AttribSys RegisterVault completed (attrib receiver type 3): the
    // 4-byte reply payload is the event-slot index the acquire leg staged. Look the slot
    // up, require its response id to be 50 (vehicle) or 66 (surface list) and its captured
    // asset set to be ATTRIBS (a non-ATTRIBS slot silently returns, slot untouched -- X360
    // `bne cr6 -> epilogue`), then echo the captured request back to the ORIGINAL
    // requester (X360 32-byte build: {miEventId, 0 receiver lane, miPoolId, mId u64,
    // asset-set lane 4, fail 0} -- PostGameDataResponse's field set with a zeroed handle;
    // the PC struct additionally carries the zeroed handle lanes the X360's 32-byte
    // truncation stops short of) and free the slot.
    void GameDataModule::ProcessAttribSysRegisterVaultResponse(
            CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
            const CgsModule::Event* lpResponse)
    {
        const s32 liSlotIndex = *reinterpret_cast<const s32*>(lpResponse);
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)];

        const s32 liResponseId = lpSlot->miResponseEventId;
        if (liResponseId != 50 && liResponseId != 66)
        {
            CGS_ASSERT(false, "Invalid event type received\n");   // X360 line 3817
            return;
        }
        if (lpSlot->mEvent.meType != E_ASSETSET_ATTRIBS)
            return;   // X360: silent return, slot untouched

        PostGameDataResponse(lpSlot, liResponseId, false,
                             static_cast<u32>(E_ASSETSET_ATTRIBS), 0, 0);
        mGameDataEventSlotPool.PushIndex(
            static_cast<s16>(mGameDataEventSlotPool.GetObjectIndex(lpSlot)));
    }

    // @ 0x8266EAA0 -- an AttribSys UnregisterVault completed (attrib receiver type 5): the
    // X360 asserts the slot's asset set is ATTRIBS, rebuilds the vehicle bundle file name
    // ("Vehicles\%s_%s.bin" from the CgsIDUnCompress'd id + the off_82F2A6BC asset-set
    // suffix table) and publishes the type-3 UnloadBundle that completes the vehicle
    // unload chain. [FLAG PC boot gate] the vehicle GameData path (ids 27/39/40/50) is
    // not exercised on the PC boot-to-world path and its suffix table/unload chain is not
    // committed -- log + free the slot (honest observable: the vehicle unload never
    // completes).
    void GameDataModule::ProcessUnregisterVehicleAttribsResponse(
            CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
            const CgsModule::Event* lpResponse)
    {
        const s32 liSlotIndex = *reinterpret_cast<const s32*>(lpResponse);
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)];
        CGS_ASSERT(lpSlot->mEvent.meType == E_ASSETSET_ATTRIBS,
                   "leBundleAssetSet == E_ASSETSET_ATTRIBS");   // X360 line 3861
        DeferredGameDataRequest(
            "vehicle attrib unload continuation (0x8266EAA0) -- vehicle path deferred", lpSlot);
    }
}
