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
#include "GameShared/GameClasses/Core/CgsID.h"                                  // CgsIDUnCompress / CgsIDConvertToString / CgsIDCompress
#include "SharedClasses/DataLists/VehicleListEntry.h"                           // VehicleListEntry (the vehicle-table probe)
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
        case E_PREPARE_VEHICLE_LIST:
            // X360 stage 9 (LABEL_26): PrepareVehicleList. (Stage 7 GameTalk and stage 8's
            // module prepare stay deferred between ATTRIBSYS and here.)
            mePrepareStage = E_PREPARE_VEHICLE_LIST;
            if (!PrepareVehicleList())
                return false;
            // fall through
            // (X360 stage 10 PrepareFreeburnChallengeList sits here -- still deferred.)
        case E_PREPARE_ICE_LIST:
            // X360 stage 11 (LABEL_28): PrepareICEList.
            mePrepareStage = E_PREPARE_ICE_LIST;
            if (!PrepareICEList())
                return false;
            // fall through
        case E_PREPARE_WHEEL_LIST:
            // X360 stage 12 (LABEL_30): PrepareWheelList.
            mePrepareStage = E_PREPARE_WHEEL_LIST;
            if (!PrepareWheelList())
                return false;
            // fall through
        case E_PREPARE_HUD_MESSAGES:
            // [gateui r3] X360 stage 13: PrepareHudMessages @0x8266C8E0 -- "HudMessages.hm"
            // into pool 11, then acquire + HudMessageController::AddMessages. This is what
            // gives GuiCache::mpHudMessageController something to point at.
            // (X360 stage 14 PreparePopups @0x8266CBA0 -- "Popups.pup", same shape, same
            //  pool -- still deferred: nothing in this tree consumes a PopupController.)
            mePrepareStage = E_PREPARE_HUD_MESSAGES;
            if (!PrepareHudMessages())
                return false;
            // fall through
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
            // ⭐ 2026-08-16 (boot audit F-P7-13). CreatePools @0x8266DC30-50 does NOT just copy
            // the memory-map row's flag: it FORCES the request's allow-defragmentation word to
            // 1 for exactly three pool ids -- 3 (OpenWorldGr), 4 (CarPool), 15 (Traffic) --
            // and those are precisely the defragmenter's own pools. Our table has all three
            // rows false and no special case, so even once the relocating defragmenter lands
            // it would have had nothing to work on. Latent until then, wrong either way.
            lOpt.mbAllowDefragmentation =
                (lrDef.miId == 3 || lrDef.miId == 4 || lrDef.miId == 15) ? true
                                                                        : lrDef.mbAllowDefrag;

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
        // ⭐ 2026-08-16 (boot audit F-P7-22). Construct @0x82671C94-9C runs
        // CgsMemory::LinearMalloc::Construct on this member (gm-data + 0x6B3D8) before
        // publishing it. We published the pointer without ever constructing the object --
        // which only works because the module happens to live in static storage and is
        // therefore zero-initialised, and stops working the moment it is heap-hosted (which
        // is what the console does: the boot allocator carves the whole game module).
        mAudioStreamAllocator.Construct();
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
        // ⚠️ [marked deviation, 2026-08-14 deformation-mount wave] HOST HEADROOM x2 on the
        // rw-linear lane sizes. The memory map's budgets are CONSOLE truth sized for 32-bit
        // objects; the host's widened types must carve MORE from the same banks. The first
        // measured overflow: bank 23 "PhysicsAlloc" (pool-0 budget 0x3ED800) already holds the
        // sim module's widened carve, and DeformationManager::Prepare's 28-model pool (console
        // 28 x 26496 = 0xB5200; host 28 x sizeof(DeformableObject), pointer-widened) tipped it --
        // LinearResourceAllocator::DoAllocate returned a null lane and the boot died on the
        // "mpaModels != NULL" tripwire + an AV in ClearVariables. The budgets scale, the map
        // data itself stays untouched (the carve comes from the host debug allocator).
        static const u32 KU_HOST_RWLINEAR_HEADROOM = 2u;
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_RWLINEAR_ALLOCATORS && li < 5; ++li)
        {
            const MemoryMapAllocatorDef& lrDef = KAC_MEMORY_MAP_RWLINEAR_ALLOCATORS[li];
            ++miNumAllocatorCreationRequests;

            rw::Resource           lRes;
            rw::ResourceDescriptor lCapacity;
            bool lbAllocOk = true;
            for (u32 lu = 0; lu < 4; ++lu)
            {
                const u32 luHostSize = lrDef.mauSize[lu] * KU_HOST_RWLINEAR_HEADROOM;
                lCapacity.m_baseResourceDescriptors[lu].m_size      = luHostSize;
                lCapacity.m_baseResourceDescriptors[lu].m_alignment = lrDef.mauAlign[lu];
                lRes.m_baseResources[lu] = 0;
                if (lrDef.mauSize[lu] == 0)
                    continue;
                lRes.m_baseResources[lu] =
                    LaneAlloc::Carve(lpAllocator, luHostSize, lrDef.mauAlign[lu],
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
    // Forward declaration: the shared list-prepare resource input carve, defined in the
    // anonymous namespace below. The schema prepare's X360 tail pumps the resource module
    // through it too (boot audit F-P7-17), and that tail comes first in this file.
    namespace { CgsResource::ResourceIO::InputBuffer* GetListPrepareResourceInput(); }

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

            // The shared X360 per-pass tail @0x826733B0-E4, in the console's order:
            // unlock, UpdateResourceModule @0x826663B0, destroy the carve, then a DIRECT
            // AttribSysModule::ProcessInputs on the attrib input (the schema request is
            // consumed synchronously; the SchemaRegisteredResponse lands on mReceiverQueue
            // for stage 1).
            //
            // ⭐ THE RESOURCE PUMP IS RESTORED, 2026-08-17 (boot audit F-P7-17). Only the
            // ProcessInputs half was here. Latent while the schema blobs come from files --
            // nothing is queued for the resource module to service -- and wrong the moment
            // the schema arrives as a resource, which is the shape the console is written
            // for. Same pump the list-prepare tail below already calls.
            mResourceModule.Update(GetListPrepareResourceInput(), 0);
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

    // ====================================================================================
    // The data-table prepares -- Prepare stages 9 (@0x8266C410) and 12 (@0x8266D1F8).
    //
    // ⭐ These, not the VL__/WL__ GET handlers, are where the vehicle and wheel tables are
    // actually loaded. The GET handlers stream nothing (see ProcessGetVehicleListRequest).
    //
    // The X360 pair are two near-identical six-state machines. Reproduced faithfully as one
    // shared body plus two thin wrappers, because the ONLY differences are:
    //     stage word   a1[140]                        a1[142]
    //     bundle       "Vehicles/VehicleList.bundle"   "Wheels/WheelList.bundle"
    //     resource     "B5VehicleList"                 "B5WheelList"
    //     allowFail    true                            false
    //     consumer     mVehicleList.AddListResource    mWheelList.AddListResource
    //     assert lines 875 / 907 / 914 / 946           1383 / 1415 / 1422 / 1446
    // [marked deviation] the console emits two separate functions with the bodies written
    // out twice; folding them keeps every store and every guard identical.
    //
    // The state machine (X360 store-for-store):
    //   0/1 : LoadBundleRequest{mpUser=&mReceiverQueue, miEventId=0, file=<bundle>,
    //         mbLiveUpdateReplace=false, miPoolId=5, mbAllowFailiure=<flag>,
    //         mbUseHDCache=false} -> resource queue type 2; mReceiverQueue.Clear(); -> 2
    //   2   : nothing queued -> tail (pump + return false). Queued -> assert the event id is
    //         2 (LoadBundleResponse), fall into 3.
    //   3   : AcquireResourceRequest{mpUser=&mReceiverQueue, miEventId=0, miPoolId=5,
    //         mResourceId=HashString(<resource>)} -> resource queue type 4;
    //         mReceiverQueue.Clear(); -> 4  (which then sees the empty queue and tails out)
    //   4   : nothing queued -> tail. Queued -> assert the event id is 4 and the response's
    //         miEventId is 0, bind a ResourcePtr from the response handle, AddListResource.
    //   5   : terminal -> return true.
    // Every non-terminal exit runs the X360's shared tail: unlock the resource input, pump
    // the resource module (UpdateResourceModule @0x826663B0), and return false.
    // ====================================================================================
    namespace
    {
        // The X360 carves a "Resource" ResourceIO::InputBuffer off the IOBufferStack on every
        // pass and destroys it at the end. [marked deviation] the IOBufferStack is deferred,
        // so the two prepares share one function-local static, exactly like Update's own
        // s_ResourceInput and the CreatePools bring-up path.
        CgsResource::ResourceIO::InputBuffer* GetListPrepareResourceInput()
        {
            static CgsResource::ResourceIO::InputBuffer s_ListPrepareInput;
            static bool s_bConstructed = false;
            if (!s_bConstructed)
            {
                s_bConstructed = true;
                s_ListPrepareInput.Construct();
            }
            return &s_ListPrepareInput;
        }

        const s32 KI_DATA_LIST_POOL_ID = 5;   // X360 immediate (the "GameData" pool)

        // [gateui r3] PrepareHudMessages @0x8266C8E0 uses a DIFFERENT pool: `li r26, 0xB`
        // at 0x8266C948, stored as the LoadBundleRequest's miPoolId (var_84 @+140) and
        // again as the AcquireResourceRequest's miPoolId (var_128 @+8). Its sibling
        // PreparePopups @0x8266CBA0 uses the same 11. Reading 5 here would push the HUD
        // message bundle into the GameData pool and the acquire would miss.
        const s32 KI_GUI_DATA_POOL_ID  = 11;  // X360 immediate (`li r26, 0xB`)
    }

    bool GameDataModule::PrepareDataListResource(s32& lriStage, const char* lpcBundleFileName,
                                                 const char* lpcResourceName, bool lbAllowFailure,
                                                 s32 liPoolId,
                                                 CgsResource::ResourceHandle* lpOutHandle)
    {
        CgsResource::ResourceIO::InputBuffer* lpResourceInput = GetListPrepareResourceInput();
        lpResourceInput->LockForWrite();

        bool lbComplete = false;
        bool lbFallThrough = true;

        switch (lriStage)
        {
        case 0:
        case 1:
        {
            lriStage = 1;
            CgsResource::Events::LoadBundleRequest lRequest;
            memset(&lRequest, 0, sizeof(lRequest));
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = 0;
            lRequest.SetFileName(lpcBundleFileName);
            lRequest.mbLiveUpdateReplace = false;
            lRequest.miPoolId            = liPoolId;
            lRequest.mbAllowFailiure     = lbAllowFailure;
            lRequest.mbUseHDCache        = false;
            lpResourceInput->GetResourceQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
            mReceiverQueue.Clear();
        }
        // fall through
        case 2:
        {
            lriStage = 2;
            if (mReceiverQueue.GetLength() < 1)
            {
                lbFallThrough = false;
                break;
            }
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            CGS_ASSERT(liType == 2, "Invalid event id received\n");   // X360 cpp:875 / 1383
        }
        // fall through
        case 3:
        {
            lriStage = 3;
            CgsResource::Events::AcquireResourceRequest lRequest;
            memset(&lRequest, 0, sizeof(lRequest));
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = 0;
            lRequest.miPoolId  = liPoolId;
            lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
                CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpcResourceName)))));
            lRequest.mbCheckRefCount = false;
            lpResourceInput->GetResourceQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
            mReceiverQueue.Clear();
        }
        // fall through
        case 4:
        {
            lriStage = 4;
            if (mReceiverQueue.GetLength() < 1)
            {
                lbFallThrough = false;
                break;
            }
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            CGS_ASSERT(liType == 4, "Invalid event id received\n");   // X360 cpp:907 / 1415

            const CgsResource::Events::AcquireResourceResponse* lpResponse =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
            CGS_ASSERT(lpResponse != 0 && lpResponse->miEventId == 0,
                       "Invalid event id received\n");                // X360 cpp:914 / 1422

            // X360: BaseResourcePtr::CreateFromHandle(&localPtr, &response.mHandle) then
            // <List>::AddListResource(this + <offset>, &localPtr). The handle is handed back
            // to the caller so the two wrappers can bind their own typed ResourcePtr.
            if (lpResponse != 0 && lpOutHandle != 0)
            {
                lpOutHandle->mpResourceMemory = lpResponse->mpResourceMemory;
                lpOutHandle->mpSourceEntry    = lpResponse->mpSourceEntry;
            }
            lriStage = 5;
            lbComplete = true;
            break;
        }
        case 5:
            // X360 LABEL_27 / LABEL_19: terminal. (The wheel machine resets its word to 2
            // here rather than leaving it at 5 -- a console quirk with no observable effect,
            // since Prepare never re-enters a completed stage. Left at 5 for both.)
            lbFallThrough = false;
            lbComplete = false;
            lpResourceInput->UnlockForWrite();
            mReceiverQueue.Clear();
            return true;
        default:
            CGS_ASSERT(false, "Invalid Stage\n");   // X360 cpp:946 / 1446
            lbFallThrough = false;
            break;
        }
        (void)lbFallThrough;

        // The shared X360 tail: unlock the input, then UpdateResourceModule @0x826663B0
        // (which on the PC is the embedded streaming engine's own pump), then destroy the
        // carve. Runs on EVERY pass, completing or not -- that pump is what actually services
        // the LoadBundle / AcquireResource this pass just published.
        lpResourceInput->UnlockForWrite();
        mResourceModule.Update(lpResourceInput, 0);

        if (lbComplete)
        {
            mReceiverQueue.Clear();
            return true;
        }
        return false;
    }

    // @ 0x8266C410 -- Prepare stage 9.
    bool GameDataModule::PrepareVehicleList()
    {
        CgsResource::ResourceHandle lHandle;
        lHandle.Clear();
        if (!PrepareDataListResource(miVehicleListPrepareStage,
                                     "Vehicles/VehicleList.bundle", "B5VehicleList",
                                     true /*mbAllowFailiure -- the X360 sets 1 here*/,
                                     KI_DATA_LIST_POOL_ID,
                                     &lHandle))
            return false;

        if (lHandle.mpResourceMemory != 0)
        {
            CgsResource::ResourcePtr<VehicleListResource> lResourcePtr(lHandle);
            mVehicleList.AddListResource(lResourcePtr);
        }
        else
        {
            // [marked deviation] the console has no null path here (mbAllowFailiure only
            // covers the bundle load); report instead of binding a null list.
            *CgsDev::Log::gpDebugPrint
                << "[GameData] PrepareVehicleList: B5VehicleList did not resolve"
                   " -- vehicle table is EMPTY\n";
        }

        *CgsDev::Log::gpDebugPrint
            << "[GameData] PrepareVehicleList: " << mVehicleList.GetVehicleCount()
            << " vehicles (" << mVehicleList.GetSelectableVehicleCount()
            << " selectable, " << mVehicleList.GetSponsorVehicleCount() << " sponsor)\n";

        // [PC diagnostic] prove the table is not just RESIDENT but READABLE: resolve the
        // Junkyard starter car by id and read its display name back out of the streamed
        // payload. A silently un-FixUp'd entry array (no registered resource type, or a
        // mis-sized offset slot) shows up here as index -1 / garbage rather than as an AV
        // three subsystems later.
        {
            const s32 liCavalry = mVehicleList.GetVehicleIndex(CgsIDCompress("PUSMC01"));
            const VehicleListEntry* lpFirst = mVehicleList.GetVehicleData(0);
            *CgsDev::Log::gpDebugPrint
                << "[GameData]   PUSMC01 (Hunter Cavalry) index=" << liCavalry
                << "  entry0 name='" << (lpFirst != 0 ? lpFirst->GetName() : "<null>")
                << "' rank=" << (lpFirst != 0 ? (s32)lpFirst->GetUnlockRank() : -1)
                << " carType=" << (lpFirst != 0 ? (s32)lpFirst->GetCarType() : -1) << "\n";
        }
        return true;
    }

    // ========================================================================================
    // @ 0x8266CEB0 -- Prepare stage 11, PrepareICEList.
    //
    // The SAME six-state machine as PrepareVehicleList/PrepareWheelList, over its own stage
    // word (X360 this+0x234 == a1[141]) with:
    //     bundle    "Cameras.bundle"      (off_82F2A6F8)
    //     resource  "StandardICETakes"    (off_82F2A71C)
    //     pool      5                     (`li r20, 5`, the shared KI_DATA_LIST_POOL_ID)
    //     allowFail false                 (the X360 leaves the byte at its `li r29,0` zero)
    //     consumer  BrnResource::ICEList::AddListResource(this + 0x70000 - 0x440, &ptr)
    //     asserts   cpp:1257 / 1286 / 1291 / 1311 ("Invalid Stage\n")
    // Both string literals are the IDA asm's own string comments on those two rodata slots,
    // and both are CONFIRMED against the shipped bundle by hash:
    //     HashString("StandardICETakes") == 0x0DC0EE8F -> CAMERAS.BUNDLE resource type 65
    //     (the ICE take dictionary)
    //     HashString("CameraVault")      == 0x28FE4576 -> CAMERAS.BUNDLE resource type 28
    //     (the AttribSys vault DirectorResourceManager::Prepare acquires; SAME bundle)
    //
    // ⭐ WHY THIS FUNCTION MATTERS BEYOND THE ICE LIST. Its stage-0 LoadBundle is the ONLY
    // thing in the whole image that makes "Cameras.bundle" resident. Both of the bundle's
    // resources land in pool 5 together, so this is also what puts the CameraVault where
    // DirectorResourceManager::Prepare's AcquireResource can find it. Without it that
    // acquire never replies and the director's prepare stage machine parks for ever.
    //
    // ⭐ UN-GATED 2026-08-01 (ICE take-runtime wave). The terminal AddListResource used to be
    // held back because neither ICEList.cpp nor the type-65 handler was in the exe source
    // list -- with no registered handler the pool skips FixUp, so the dictionary's mpaIndex
    // and every entry's mpData are still resource-relative OFFSETS, and binding that would
    // have made every take lookup dereference an offset as a pointer. Both are mounted now
    // (DictionaryResourceType<ICE::ICETakeData> is registered in
    // CgsResourceTypeRegistration.cpp), so the bind is the console's own again.
    // ========================================================================================
    bool GameDataModule::PrepareICEList()
    {
        CgsResource::ResourceHandle lHandle;
        lHandle.Clear();
        if (!PrepareDataListResource(miICEListPrepareStage,
                                     "Cameras.bundle", "StandardICETakes",
                                     false /*mbAllowFailiure -- the X360 leaves it 0*/,
                                     KI_DATA_LIST_POOL_ID,
                                     &lHandle))
            return false;

        if (lHandle.mpResourceMemory != 0)
        {
            BrnResource::ICEList::ICETakeDictionaryResourcePtr lResourcePtr(lHandle);
            mICEList.AddListResource(lResourcePtr);
        }
        else
        {
            // [marked deviation] the console has no null path here (as for the vehicle/wheel
            // lists); report instead of binding a null dictionary.
            *CgsDev::Log::gpDebugPrint
                << "[GameData] PrepareICEList: StandardICETakes did not resolve"
                   " -- the ICE take table is EMPTY\n";
        }

        *CgsDev::Log::gpDebugPrint
            << "[GameData] PrepareICEList: " << mICEList.GetICEMovieCount() << " ICE takes\n";
        {
            // [PC diagnostic] the three takes ArbStateCarSelect's game-intro shot group
            // names (610132 Intro_FlyCam_Loop / 605855 DMV_IntroA / 605858 DMV_IntroB),
            // resolved by guid through the same rebased entry array -- so this doubles as
            // the regression gate on the type-65 FixUp.
            static const s32 kaiIntroTakeGuids[3] = { 610132, 605855, 605858 };
            for (s32 liIndex = 0; liIndex < 3; ++liIndex)
            {
                const ICE::ICETakeData* lpTake =
                    mICEList.GetICETakeDataFromGuid(kaiIntroTakeGuids[liIndex]);
                *CgsDev::Log::gpDebugPrint
                    << "[GameData]   take guid " << kaiIntroTakeGuids[liIndex] << " -> "
                    << (lpTake != 0 ? lpTake->GetName() : "<not found>");
                if (lpTake != 0)
                {
                    *CgsDev::Log::gpDebugPrint << " len=" << lpTake->GetLength();
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }
        }
        return true;
    }

    // @ 0x8266D1F8 -- Prepare stage 12. (This one is NOT in the .ida-exports set -- the gap
    // rule applies: it was recovered straight from the ARTIST database with headless IDA.)
    bool GameDataModule::PrepareWheelList()
    {
        CgsResource::ResourceHandle lHandle;
        lHandle.Clear();
        if (!PrepareDataListResource(miWheelListPrepareStage,
                                     "Wheels/WheelList.bundle", "B5WheelList",
                                     false /*mbAllowFailiure -- the X360 sets 0 here*/,
                                     KI_DATA_LIST_POOL_ID,
                                     &lHandle))
            return false;

        if (lHandle.mpResourceMemory != 0)
        {
            CgsResource::ResourcePtr<WheelListResource> lResourcePtr(lHandle);
            mWheelList.AddListResource(lResourcePtr);
        }
        else
        {
            *CgsDev::Log::gpDebugPrint
                << "[GameData] PrepareWheelList: B5WheelList did not resolve"
                   " -- wheel table is EMPTY\n";
        }

        *CgsDev::Log::gpDebugPrint
            << "[GameData] PrepareWheelList: " << mWheelList.GetWheelCount() << " wheels\n";
        {
            // [PC diagnostic] the Hunter Cavalry's default wheel, resolved by name through
            // the same rebased entry array (also the regression gate on the FixUp fix).
            const s32 liWheel = mWheelList.FindWheelIndexFromName("5Spoke_19_16_650");
            *CgsDev::Log::gpDebugPrint
                << "[GameData]   wheel '5Spoke_19_16_650' index=" << liWheel << "\n";
        }
        return true;
    }

    // ========================================================================================
    // [gateui r3] @ 0x8266C8E0 -- Prepare stage 13, PrepareHudMessages. THE PRODUCER OF
    // BrnResource::HudMessageController. Until this landed, GuiCache::mpHudMessageController
    // had ZERO writers anywhere in the tree, so HudMessageDirector::FilterAndSendOffMessage
    // stopped every HUD message at its `mpController` assert (cpp:222) and the gateui ladder
    // could never reach `[UI-gate] hud message sent`.
    //
    // Structurally identical to the vehicle/wheel/ICE machine (same six states, same
    // LoadBundleRequest -> wait -> AcquireResourceRequest -> wait -> bind shape, same
    // "Invalid event id received\n" asserts at cpp:1015 / cpp:1045, same "Invalid Stage\n"
    // default), with exactly two differences, both X360-measured:
    //   * POOL 11, not 5 (`li r26, 0xB` @0x8266C948 -- see KI_GUI_DATA_POOL_ID);
    //   * the terminal step hands the ACQUIRE RESPONSE to HudMessageController::AddMessages
    //     (`addis r3, r30, 6 / addi r3, r3, 0x5950 / bl AddMessages` @0x8266CB10), instead of
    //     binding a typed ResourcePtr and calling a list's AddListResource.
    //
    // [marked deviation] the shared helper hands back a ResourceHandle rather than the queued
    // event record, so the response AddMessages reads is rebuilt from that handle here. The
    // console's AddMessages @0x8267D580 reads exactly ONE thing out of the response --
    // `CreateFromHandle(this + 4, response + 24)`, i.e. the {mpResourceMemory, mpSourceEntry}
    // handle pair -- so the two forms are observationally identical.
    bool GameDataModule::PrepareHudMessages()
    {
        CgsResource::ResourceHandle lHandle;
        lHandle.Clear();
        if (!PrepareDataListResource(miHudMessagesPrepareStage,
                                     "HudMessages.hm", "HudMessages.hm",
                                     false /*mbAllowFailiure -- the X360 stores 0 @0x8266C9C8*/,
                                     KI_GUI_DATA_POOL_ID,
                                     &lHandle))
        {
            // [DIAG] NOT IN THE X360 BINARY, and NOT a new failure mode -- the stage still
            // waits exactly as the vehicle/ICE/wheel siblings do, for ever if need be. This
            // is a one-shot LOUD tripwire so that verify_r3_fix3hud's SUSPECT-1 (a boot hang
            // on the mounted path, invisible because Prepare simply never completes) is
            // diagnosable from BrnGame.log instead of looking like a freeze. Three firsts
            // stack up in this one stage and none had ever run in this tree before round 3:
            // the first `.hm` bundle requested, the first pool-11 acquire, and the first
            // acquire of a type whose handler round 4 only just registered.
            // Unconditional (not BRN_PROP_DIAG-gated) to match this file's house style --
            // every other gpDebugPrint in BrnGameDataModule.cpp is unconditional -- and it
            // prints at most once per process.
            static const s32 KI_HUD_MESSAGE_STALL_WARN_PUMPS = 600;   // ~10 s at 60 Hz
            static s32       siHudMessageWaitPumps           = 0;
            static bool      sbHudMessageStallReported       = false;

            ++siHudMessageWaitPumps;
            if (!sbHudMessageStallReported &&
                siHudMessageWaitPumps >= KI_HUD_MESSAGE_STALL_WARN_PUMPS &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                sbHudMessageStallReported = true;
                *CgsDev::Log::gpDebugPrint
                    << "[GameData] PrepareHudMessages: STILL WAITING after "
                    << siHudMessageWaitPumps
                    << " pumps at sub-stage " << miHudMessagesPrepareStage
                    << " -- the HudMessages.hm LoadBundle/acquire has not answered on pool "
                    << KI_GUI_DATA_POOL_ID
                    << ". GameData Prepare stage 13 cannot complete and the boot will not"
                       " advance past it.\n";
            }
            return false;
        }

        // ⭐ [gateui r4] THE ROUND-3 REFUSAL GATE IS DELETED. It read the registry for type
        // 44 and, finding no handler, declined the bind -- because
        // CgsResource::HudMessageResourceType existed (CgsGuiHudMessageType.cpp, GetTypeID
        // 44) but was never registered, so CgsResourceBundleLoader.cpp's
        // `mpResourceType != 0` guards (:195/:251/:258/:266) skipped every FixUp pass and the
        // acquire handed back a pointer table that was still a FILE OFFSET. Both halves are
        // now closed in the same commit: the type is registered
        // (CgsResourceTypeRegistration.cpp, "HudMessage") and its FixUp/FixDown relocate at
        // HOST pointer width (CgsGuiHudMessage.cpp / CgsGuiHudMessageType.cpp, GetLoadBase64)
        // -- which is what the shipped bundle needs, since build/game/HUDMESSAGES.HM is
        // already transcoded to platform 4 with 8-byte slots (mppHudMessageData reads 0x80,
        // the 250 table entries at an 8-byte stride, record stride 0x170 == the host
        // sizeof(CgsGui::GuiHudMessageData)). With FixUp running, the bind must proceed.
        if (lHandle.mpResourceMemory != 0)
        {
            CgsResource::Events::AcquireResourceResponse lResponse;
            lResponse.mpUser           = &mReceiverQueue;
            lResponse.miEventId        = 0;
            lResponse.miPoolId         = KI_GUI_DATA_POOL_ID;
            lResponse.mpResourceMemory = lHandle.mpResourceMemory;
            lResponse.mpSourceEntry    = lHandle.mpSourceEntry;
            mHudMessageController.AddMessages(&lResponse);

            // [DIAG] NOT IN THE X360 BINARY -- the gateui ladder's producer rung, guarded by
            // the wave's single knob (BRN_PROP_DIAG) exactly like PropEntityModule_wQ_04.cpp.
            // Fires ONCE, at the bind, and reports the loaded message count so a silently
            // empty table (unregistered type 44 / skipped FixUp) is visible here rather than
            // as an "Unable to find message" line 400 frames later.
            {
                static const bool sbUiGateDiag = (getenv("BRN_PROP_DIAG") != 0);
                if (sbUiGateDiag && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[UI-gate] hud controller bound msgs="
                        << mHudMessageController.GetMessageLoadedCount() << "\n";
                }
            }
        }
        else
        {
            // [marked deviation] the console has no null path here (mbAllowFailiure is 0, so
            // a missing bundle asserts inside the loader). Report instead of binding a null
            // resource -- AddMessages would dereference it through mMessagesPtr->
            // miHudMessageCount for its own 300-message cap assert.
            *CgsDev::Log::gpDebugPrint
                << "[GameData] PrepareHudMessages: HudMessages.hm did not resolve"
                   " -- the HUD message table is EMPTY (the director will reject every"
                   " message at GetIndexFromMessageHash)\n";
        }

        *CgsDev::Log::gpDebugPrint
            << "[GameData] PrepareHudMessages: "
            << ((lHandle.mpResourceMemory != 0) ? mHudMessageController.GetMessageLoadedCount() : 0)
            << " hud messages\n";
        return true;
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
        miVehicleListPrepareStage       = 0;
        miICEListPrepareStage           = 0;
        miWheelListPrepareStage         = 0;
        miHudMessagesPrepareStage       = 0;   // [gateui r3] X360 a1[145]

        // X360 0x82671B90 also constructs the two resident data tables here (VehicleList::
        // Construct @0x82677850 and WheelList::Construct @0x82677DB8 both list
        // GameDataModule::Construct as their only caller). They MUST run before Prepare
        // stages 9/12: AddListResource appends into slot tables that Construct seeds with -1.
        mVehicleList.Construct();
        mWheelList.Construct();

        // [gateui r3] X360 0x82671D1C: HudMessageController::Construct(this + 0x65950) --
        // through the ICF-folded one-store symbol at 0x82676600 (see the banner on
        // HudMessageController::Construct). It MUST run before Prepare stage 13:
        // AddMessages' first act is the "already loaded" tripwire on mbMessagesUsed.
        mHudMessageController.Construct();

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
        // list from our existing singleton resource-type handlers. The X360 operator-new's 76 Type objects
        // into a GSResourceType array, but that path's prerequisites (Type::operator new / InitCachedValues
        // + the 76 subclasses) are deferred, and our tree already registers handler SINGLETONS into a
        // CgsResource::TypeRegistry (RegisterAllResourceTypes). So reuse those: register them (idempotent),
        // then forward the WHOLE registered set into the GSResourceType list. The PoolModule type-registry
        // loop then copies them into maTypes (keyed by GetTypeID()).
        //
        // ⭐ CORRECTED 2026-08-14 (boot audit F-P7-18). This forwarded a HARDCODED FIVE ids
        // (raster/texture-state/material-state/font/videodata) while the engine had ~45 handlers
        // registered, so every other type reaching a pool took CgsResourcePool.cpp's "no registered
        // type handler -- allocating raw, FixUp will be skipped" path. A resource whose FixUp never
        // runs keeps its file-relative pointers, so the first consumer to walk one dereferences an
        // unrelocated offset -- which is exactly how EnvironmentSettings::Dictionary (0x10014) crashed
        // EnvironmentManager::StreamIn the moment the scripted world load was restored to its console
        // position (F3). The console registers every handler it owns; so do we now.
        CgsResource::RegisterAllResourceTypes();
        enum { KI_MAX_GAME_TYPES = CgsResource::TypeRegistry::KU_MAX_REGISTERED_RESOURCE_TYPES };
        static CgsResource::PoolModule::InitOptions::GSResourceType s_aGameTypes[KI_MAX_GAME_TYPES];
        s32 lNumGameTypes = 0;
        {
            const u32 luRegistered = CgsResource::TypeRegistry::GetCount();
            for (u32 lu = 0; lu < luRegistered && lNumGameTypes < KI_MAX_GAME_TYPES; ++lu)
            {
                const CgsResource::Type* lpType = CgsResource::TypeRegistry::GetByIndex(lu);
                if (lpType == 0)
                    continue;
                s_aGameTypes[lNumGameTypes].mpType = lpType;
                // The name column is the console's parallel string table; it is debug-only.
                s_aGameTypes[lNumGameTypes].mpcName = CgsResource::TypeRegistry::GetNameByIndex(lu);
                ++lNumGameTypes;
            }
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

        // The prop-physics bundle file name (DWARF: KPC_PROP_PHYSICS_BUNDLE_FILENAME,
        // BrnGameDataModule.cpp:82 -- declared immediately before KPC_PROP_PHYSICS_RESOURCENAME
        // on :83, i.e. the file/resource pair this TU's prop-physics LOAD+GET legs use).
        //
        // ⚠️ PROVENANCE, read before trusting: unlike every other literal in this block the
        // VALUE here is NOT read out of the ARTIST data segment. ProcessLoadPropPhysicsRequest
        // has no function of its own in the X360 image -- it is inlined into
        // ProcessLoadGameDataEvent @0x82671EA0, which is one of the function-gapped addresses
        // this export set does not carry (its xrefs_from list every OTHER Load handler and no
        // prop-physics one), and the .i64 is packed so its string pool is not readable here
        // either. The value below is therefore DERIVED, from three converging facts:
        //   1. the shipped disc file is PROPS/PROPPHYSICS.BUNDLE, and it holds exactly one
        //      resource -- 0xD75C5932 / type 0x1000F / name "PRP_PHYSICS_" -- which is the
        //      resource ProcessGetPropPhysicsRequest then acquires on hop 2;
        //   2. the sibling constants in this same TU spell mixed-case, forward-slash paths
        //      with the real extension ("Props/Instances/TRK_UNIT...PropInstances.bundle",
        //      "Wheels/%s_%s.bndl");
        //   3. name resolution is case-insensitive here -- the already-committed
        //      "surfacelist.bin"/"pvs.bndl"/"worldcol.bin" all resolve to upper-case files.
        // Only the glyph case/separator is uncertain and neither is behaviourally
        // load-bearing. Replace with the literal from the data segment if 0x82671EA0 is ever
        // exported.
        const char* const KPC_PROP_PHYSICS_BUNDLE_FILE_NAME = "Props/PropPhysics.bundle";

        // off_82F2A72C -- the PVS zone-list RESOURCE name ProcessGetPVSRequest hashes.
        // Attested TWICE: read from the ARTIST .i64 data segment (headless IDA dump), and
        // CRC32-lowercase("newgrid") == 0x5A4A4CDB == the id of the one ZoneList resource
        // in the shipped PVS.BNDL (its debug ResourceStringTable also names it "newgrid").
        const char* const KPC_PVS_RESOURCE_NAME      = "newgrid";

        // The lane-data file names + resource names, all four read straight out of the four
        // handlers' own asm (the `lwz rN, off_82F2Axxx` right before SetFileName/HashString):
        //   off_82F2A6EC "B5Traffic.bndl"  ProcessLoadTrafficLanesRequest @0x8266F398
        //   off_82F2A700 "AI.dat"          ProcessLoadAILanesRequest      @0x8266F4B0
        //   off_82F2A710 "BaseTraffic"     ProcessGetTrafficLanesRequest  @0x826703B0
        //   off_82F2A724 "WorldMapData"    ProcessGetAILanesRequest       @0x826704C0
        // Both RESOURCE names are attested a second time by the shipped data, exactly the way
        // KPC_PVS_RESOURCE_NAME is: CRC32-lowercase("BaseTraffic") == 0xC43359DA == the id of
        // the one TrafficData (type 65538) resource in B5TRAFFIC.BNDL, and
        // CRC32-lowercase("WorldMapData") == 0xA8CD78D4 == the id of the one AISections
        // (type 65537) resource in AI.DAT.
        const char* const KPC_TRAFFIC_LANES_FILE_NAME    = "B5Traffic.bndl";
        const char* const KPC_AI_LANES_FILE_NAME         = "AI.dat";
        const char* const KPC_TRAFFIC_LANES_RESOURCE_NAME = "BaseTraffic";
        const char* const KPC_AI_LANES_RESOURCE_NAME      = "WorldMapData";

        // off_82F2A708 -- the world-collision bundle file name (ProcessInternalValidate
        // Response's swap-in reload + the LoadWorldCollision handler family). Read from
        // the ARTIST .i64 data segment (headless IDA dump; the exports are function-only).
        const char* const KPC_WORLD_COLLISION_FILE_NAME = "worldcol.bin";

        // off_82F2A744 -- the PROP-PHYSICS resource name ProcessGetPropPhysicsRequest
        // @0x8266FAD8 hashes. Unlike every other prop GET, this one does NOT hash the
        // request's own id string: the asm loads the literal pointer
        // (`lis r11, off_82F2A744@ha; lwz r3, off_82F2A744@l(r11)  # "PRP_PHYSICS_"`) and
        // hands THAT to CgsResource::ID::HashString, so the id string it just converted into
        // the stack buffer is discarded. Attested TWICE, exactly like KPC_PVS_RESOURCE_NAME:
        // the IDA data-segment comment on that literal, and the shipped data --
        // PROPS/PROPPHYSICS.BUNDLE holds exactly one resource entry, id 0xD75C5932, type
        // 0x1000F (PropPhysics), and its debug ResourceStringTable names it "PRP_PHYSICS_";
        // CRC32-lowercase("PRP_PHYSICS_") == 0xD75C5932.
        // (DecFIGS DWARF spells the original identifier KPC_PROP_PHYSICS_RESOURCENAME
        // -- BrnGameDataModule.cpp:83; the `_RESOURCE_NAME` form here matches the spelling
        // the rest of this recon block already uses.)
        const char* const KPC_PROP_PHYSICS_RESOURCE_NAME = "PRP_PHYSICS_";

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

        // The vehicle handlers' four format literals (X360 aVehiclesSSBin / aEngines08xBund /
        // aSS_13 / aSS). The engine-bundle one is shared by BOTH sound legs; note its SPrintf
        // cap is 64, not 128.
        const char* const KPC_VEHICLE_FILE_FORMAT          = "Vehicles\\%s_%s.bin";
        const char* const KPC_ENGINE_BUNDLE_FILE_FORMAT    = "Engines\\%08x.bundle";
        const char* const KPC_VEHICLE_RESOURCE_FORMAT      = "%s_%s";
        const char* const KPC_VEHICLE_RESOURCE_NOSEP_FORMAT = "%s%s";

        // The wheel LOAD handler's own file literal (X360 aWheelsSSBndl @0x8266EE7C). Note
        // it is NOT the vehicle spelling: forward slash, and a ".bndl" extension, matching
        // WHEELS/WHE_<code>_GR.BNDL on disc. The wheel GET handler reuses
        // KPC_VEHICLE_RESOURCE_FORMAT ("%s_%s", the shared aSS_13 literal).
        const char* const KPC_WHEEL_FILE_FORMAT            = "Wheels/%s_%s.bndl";
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
            // X360 @0x82674818-2C: LockForWrite(out);
            // FileSystemStatusInterface::Construct(out + 9);  <- ⭐ NINE, corrected
            // 2026-08-16 (boot audit F-P7-12): the old "out+36" here was a WORD-index
            // reading of a byte offset, i.e. the same member counted in the wrong unit.
            // Then out+4 := 0 (the allocator-list slot cleared) and
            // SetAllocatorList(out, &mAllocatorList), then the LiveUpdateIO status copy
            // (SetLiveUpdateStatus from the LiveUpdate output).
            //
            // [FLAG] we Construct the WHOLE output buffer where the console constructs only
            // that one member -- wider than the console, and it stays that way until the
            // OutputBuffer's filesystem-status member is homed. The LiveUpdate module is
            // deferred (F-P7-4); the allocator-list publish is the live part.
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
            ProcessLoadVehicleRequest(lpResourceInput, lpEvent, 27, liIndex);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            ProcessLoadWheelRequest(lpResourceInput, lpEvent, 36, liIndex);
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
                ProcessLoadPropPhysicsRequest(lpResourceInput, lpEvent, 34, liIndex);
            else if (memcmp(lacName + 4, "INST", 4) == 0)
                ProcessLoadPropInstancesRequest(lpResourceInput, lpEvent, 35, liIndex);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 2899)
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                ProcessLoadTrafficLanesRequest(lpResourceInput, lpEvent, 30, liIndex);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                ProcessLoadAILanesRequest(lpResourceInput, lpEvent, 29, liIndex);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 2914)
        }
        else if (memcmp(lacName, "TRK_", 4) == 0)
        {
            if (memcmp(lacName + 4, "UNIT", 4) == 0)
                ProcessLoadWorldUnitRequest(lpResourceInput, lpEvent, 31, liIndex);
            else if (memcmp(lacName + 4, "COLL", 4) == 0)
                ProcessLoadWorldCollisionRequest(lpResourceInput, lpEvent, 32, liIndex);
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
            ProcessGetVehicleRequest(lpResourceInput, lpEvent, 50, liIndex);
        }
        else if (memcmp(lacName, "ICE_", 4) == 0)
        {
            DeferredGameDataRequest("GetICEMovie (id 65)", lpSlot);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            ProcessGetWheelRequest(lpResourceInput, lpEvent, 60, liIndex);
        }
        else if (memcmp(lacName, "TVEH", 4) == 0)
        {
            DeferredGameDataRequest("GetTrafficVehicle (0x82670280, id 51)", lpSlot);
        }
        else if (memcmp(lacName, "VL__", 4) == 0)
        {
            ProcessGetVehicleListRequest(lpResourceInput, lpEvent, 52, liIndex);
        }
        else if (memcmp(lacName, "CL__", 4) == 0)
        {
            DeferredGameDataRequest("GetFreeburnChallengeList (id 53)", lpSlot);
        }
        else if (memcmp(lacName, "IL__", 4) == 0)
        {
            ProcessGetICEListRequest(lpResourceInput, lpEvent, 64, liIndex);
        }
        else if (memcmp(lacName, "PRP_", 4) == 0)
        {
            if (memcmp(lacName + 4, "PHYS", 4) == 0)
                ProcessGetPropPhysicsRequest(lpResourceInput, lpEvent, 61, liIndex);
            else if (memcmp(lacName + 4, "GL__", 4) == 0)
                ProcessGetPropGraphicsListRequest(lpResourceInput, lpEvent, 63, liIndex);
            else if (memcmp(lacName + 4, "INST", 4) == 0)
                ProcessGetPropInstancesRequest(lpResourceInput, lpEvent, 62, liIndex);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3114
        }
        else if (memcmp(lacName, "WL__", 4) == 0)
        {
            ProcessGetWheelListRequest(lpResourceInput, lpEvent, 59, liIndex);
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                ProcessGetTrafficLanesRequest(lpResourceInput, lpEvent, 55, liIndex);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                ProcessGetAILanesRequest(lpResourceInput, lpEvent, 54, liIndex);
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
    // @ 0x8266EB98 -- service a LOAD vehicle request (dispatch id 27).
    //
    // Five asset sets share one handler. The bundle-set index is the request's meType with
    // ONE remap: E_ASSETSET_PHYSICS (1) -> E_ASSETSET_ATTRIBS (4), so physics and attribs
    // both stream "VEH_<code>_AT.bin" (that file carries the AttribSysVault AND the
    // StreamedDeformationSpec). The X360 does this remap only for vehicles -- the wheel and
    // traffic-vehicle Load handlers index the same suffix table with meType unremapped.
    //
    // SOUND (2) does not build a "Vehicles\..." name at all: it resolves the request's id
    // through the VehicleList and streams the entry's EXHAUST engine bundle
    // ("Engines\%08x.bundle" of HashString(decode(mExhaustName))), resetting
    // muLoadedSoundBundlesCount to 0 first. Its twin in ProcessGetVehicleRequest streams the
    // ENGINE one (+0xC0) -- that is not an inconsistency: they are two different named
    // fields of the entry's mAudioData block and a car legitimately has both assets.
    //
    // The file name is built from the FULL id string here (it keeps the "VEH_" prefix,
    // matching "VEHICLES\VEH_PUSMC01_GR.BIN" on disk); ProcessGetVehicleRequest builds
    // RESOURCE names from id+4. That asymmetry is real.
    //
    // No reply is posted: the completion rides ProcessInternalLoadBundleResponse's case 27,
    // which is why the response id is staged into the slot first.
    void GameDataModule::ProcessLoadVehicleRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                   const GameDataIO::GameDataAssetEvent* lpEvent,
                                                   s32 liEventId, s32 liSlotIndex)
    {
        // X360 store order: the response id is staged FIRST (`stw r6, 0x28(slot)`).
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        char lacVehicleID[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacVehicleID);

        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS
                       || lpEvent->meType == E_ASSETSET_PHYSICS
                       || lpEvent->meType == E_ASSETSET_SOUND
                       || lpEvent->meType == E_ASSETSET_ATTRIBS,
                   "Invalid asset type for vehicles\n");   // X360 line 3938

        // X360 0x8266EC44: `if (meType == 1) bundleAssetSet = 4;` -- and the SOUND branch
        // below tests the ORIGINAL meType, not the remapped one.
        const u32 luBundleAssetSet =
            (lpEvent->meType == E_ASSETSET_PHYSICS)
                ? static_cast<u32>(E_ASSETSET_ATTRIBS)
                : static_cast<u32>(lpEvent->meType);

        char lacResourceName[208];

        if (lpEvent->meType == E_ASSETSET_SOUND)
        {
            CGS_ASSERT(strstr(lacVehicleID, "VEH_") != 0, "strstr( lacVehicleID, \"VEH_\" )");   // X360 line 3951

            const VehicleListEntry* lpVehicleListEntry =
                mVehicleList.GetVehicleData(CgsIDCompress(lacVehicleID + 4));
            CGS_ASSERT(lpVehicleListEntry != 0, "lpVehicleListEntry");   // X360 line 3956
            if (lpVehicleListEntry == 0)
                return;   // [marked deviation] the X360 assert is non-fatal and the next
                          // load would fault on the null entry; guard the host instead.

            muLoadedSoundBundlesCount = 0;

            char lacEngineName[KI_CGSID_STRING_LEN];   // the X360 local name, for BOTH legs
            CgsIDConvertToString(lpVehicleListEntry->mExhaustName, lacEngineName);
            CgsCore::SPrintf(lacResourceName, 64, KPC_ENGINE_BUNDLE_FILE_FORMAT,
                             CgsResource::ID::HashString(
                                 reinterpret_cast<const u8*>(lacEngineName)));
        }
        else
        {
            const char* lpcSuffix =
                (luBundleAssetSet < (sizeof(KAPC_ASSET_SET_SUFFIXES) / sizeof(KAPC_ASSET_SET_SUFFIXES[0])))
                    ? KAPC_ASSET_SET_SUFFIXES[luBundleAssetSet]
                    : KAPC_ASSET_SET_SUFFIXES[0];
            CgsCore::SPrintf(lacResourceName, 128, KPC_VEHICLE_FILE_FORMAT,
                             lacVehicleID, lpcSuffix);
        }

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(lacResourceName);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;   // X360 stores 0 here (unlike the track-unit path)

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266EDB8 -- service a LOAD wheel request (dispatch id 36). The bodyshell's twin:
    // hop 1 of the wheel-graphics fetch the race-car streamer drives ("STRM: Need to load
    // WheelGfx" -> RequestInterface::LoadGameData("WHE_<code>")).
    //
    // Simpler than the vehicle handler in three ways the asm is explicit about:
    //   * ONE asset set. `if (event->meType) assert` -- wheels only ever ship GRAPHICS, so
    //     there is no PHYSICS->ATTRIBS remap and no SOUND leg. (The assert TEXT really does
    //     say "vehicles"; the console's copy-paste, reproduced verbatim.)
    //   * The suffix table is indexed with meType UNREMAPPED (`slwi r10, r9, 2` straight off
    //     the request), so a valid request always picks KAPC_ASSET_SET_SUFFIXES[0] == "GR".
    //   * The file name keeps the FULL id string including the "WHE_" prefix -- the disc
    //     really holds WHEELS/WHE_51916650_GR.BNDL. (ProcessGetWheelRequest below drops the
    //     prefix for the RESOURCE name; that asymmetry is the same one the vehicle pair has.)
    //
    // No reply is posted here: the completion rides ProcessInternalLoadBundleResponse's
    // case 36, which chains into ProcessGetWheelRequest -- hence the response id is staged
    // into the slot first, exactly as the vehicle LOAD does.
    void GameDataModule::ProcessLoadWheelRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                 const GameDataIO::GameDataAssetEvent* lpEvent,
                                                 s32 liEventId, s32 liSlotIndex)
    {
        // X360 store order: the response id is staged FIRST (`stw r31, 0x28(r3)` before the
        // CgsIDConvertToString call).
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        char lacWheelID[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacWheelID);

        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS,
                   "Invalid asset type for vehicles\n");   // X360 line 4010

        const u32 luAssetSet = static_cast<u32>(lpEvent->meType);
        const char* lpcSuffix =
            (luAssetSet < (sizeof(KAPC_ASSET_SET_SUFFIXES) / sizeof(KAPC_ASSET_SET_SUFFIXES[0])))
                ? KAPC_ASSET_SET_SUFFIXES[luAssetSet]
                : KAPC_ASSET_SET_SUFFIXES[0];

        char lacFileName[208];
        CgsCore::SPrintf(lacFileName, 128, KPC_WHEEL_FILE_FORMAT, lacWheelID, lpcSuffix);

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(lacFileName);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;   // X360 stores 0 here, like the vehicle path

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266FDA0 -- service a GET vehicle request (dispatch id 50).
    //
    // Four asset sets, four RESOURCE names, all built from the id string WITHOUT its
    // "VEH_" prefix:
    //   GRAPHICS(0) -> "<code>_Graphics"            (confirmed at runtime: the car's
    //                                                graphics bundle really does publish
    //                                                exactly this hash)
    //   ATTRIBS(4)  -> strncpy(<code>, 9) + "_AttribSys"
    //   SOUND(2)    -> the ref-counted engine-bundle leg below (returns early)
    //   otherwise   -> "<code>DeformationModel"     (NO separator -- the X360 format is
    //                                                "%s%s", not "%s_%s")
    // then one AcquireResourceRequest (type 4) for HashString(name) out of the request's pool.
    //
    // The SOUND leg is a two-pass ref count on muLoadedSoundBundlesCount:
    //   0 -> 1 : stream "Engines\%08x.bundle" of the entry's mEngineName (+0xC0) and re-stage
    //            the slot's response id to 27, so ProcessInternalLoadBundleResponse's vehicle
    //            case calls straight back in here with the SAVED request. No reply yet.
    //   1 -> 2 : post the id-50 reply (meType forced to SOUND) to the ORIGINAL requester's
    //            queue and free the slot. No bundle work.
    //   >= 2   : assert and bail (the counter still increments).
    void GameDataModule::ProcessGetVehicleRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                  const GameDataIO::GameDataAssetEvent* lpEvent,
                                                  s32 liEventId, s32 liSlotIndex)
    {
        GameDataEventSlot* lpSlot = &mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)];
        lpSlot->miResponseEventId = liEventId;

        char lacVehicleID[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacVehicleID);

        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS
                       || lpEvent->meType == E_ASSETSET_PHYSICS
                       || lpEvent->meType == E_ASSETSET_SOUND
                       || lpEvent->meType == E_ASSETSET_ATTRIBS,
                   "Invalid asset type for vehicles\n");   // X360 line 4649

        char lacResourceName[208];

        if (lpEvent->meType == E_ASSETSET_SOUND)
        {
            ++muLoadedSoundBundlesCount;

            if (muLoadedSoundBundlesCount == 1)
            {
                CGS_ASSERT(strstr(lacVehicleID, "VEH_") != 0,
                           "strstr( lacVehicleID, \"VEH_\" )");   // X360 line 4684

                const VehicleListEntry* lpVehicleListEntry =
                    mVehicleList.GetVehicleData(CgsIDCompress(lacVehicleID + 4));
                CGS_ASSERT(lpVehicleListEntry != 0, "lpVehicleListEntry");   // X360 line 4689
                if (lpVehicleListEntry == 0)
                    return;   // [marked deviation] see ProcessLoadVehicleRequest

                char lacEngineName[KI_CGSID_STRING_LEN];
                CgsIDConvertToString(lpVehicleListEntry->mEngineName, lacEngineName);
                CgsCore::SPrintf(lacResourceName, 64, KPC_ENGINE_BUNDLE_FILE_FORMAT,
                                 CgsResource::ID::HashString(
                                     reinterpret_cast<const u8*>(lacEngineName)));

                CgsResource::Events::LoadBundleRequest lRequest;
                memset(&lRequest, 0, sizeof(lRequest));
                lRequest.mpUser    = &mReceiverQueue;
                lRequest.miEventId = liSlotIndex;
                lRequest.SetFileName(lacResourceName);
                lRequest.mbLiveUpdateReplace = false;
                lRequest.miPoolId            = lpEvent->miPoolId;
                lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
                lRequest.mbUseHDCache        = false;

                lpResourceInput->GetResourceQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest),
                    2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));

                // X360 `li r11,0x1B; stw r11,0x28(slot)` -- re-route the completion through
                // the LOAD-vehicle case so this handler is re-entered on the second pass.
                lpSlot->miResponseEventId = 27;
                return;
            }

            if (muLoadedSoundBundlesCount == 2)
            {
                PostGameDataResponse(lpSlot, 50, lpSlot->mEvent.mbFailFlag,
                                     static_cast<u32>(E_ASSETSET_SOUND), 0, 0);
                mGameDataEventSlotPool.PushIndex(
                    static_cast<s16>(mGameDataEventSlotPool.GetObjectIndex(lpSlot)));
                return;
            }

            CGS_ASSERT(false, "Tried to load too many sound bundles");   // X360 line 4721
            return;
        }

        if (lpEvent->meType == E_ASSETSET_GRAPHICS)
        {
            CgsCore::SPrintf(lacResourceName, 128, KPC_VEHICLE_RESOURCE_FORMAT,
                             lacVehicleID + 4, "Graphics");
        }
        else if (lpEvent->meType == E_ASSETSET_ATTRIBS)
        {
            // X360: strncpy(dst, id+4, 9) then an inline StrCat of "_AttribSys" (with the
            // CgsStringUtils.h:75 length tripwire the inline expansion carries).
            strncpy(lacResourceName, lacVehicleID + 4, 9);
            lacResourceName[9] = '\0';
            CGS_ASSERT(strlen(lacResourceName) + 10 < 0x7F,
                       "strlen(lpcDest) + strlen(lpcSrc) < liMaxLength");   // CgsStringUtils.h:75
            strcat(lacResourceName, "_AttribSys");
        }
        else
        {
            CgsCore::SPrintf(lacResourceName, 128, KPC_VEHICLE_RESOURCE_NOSEP_FORMAT,
                             lacVehicleID + 4, "DeformationModel");
        }

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

    // @ 0x82670140 -- service a GET wheel request (dispatch id 60). Hop 2 of the wheel
    // fetch: the bundle is now in the pool, so acquire the wheel's GraphicsSpec by name.
    //
    // ONE resource name, and it is built from the id string WITHOUT its "WHE_" prefix --
    // the asm passes `var_D4`, i.e. the name buffer + 4, into the "%s_%s" format
    // (`lacWheelID + 4` + "Graphics" -> "51916650_Graphics"). Same prefix asymmetry the
    // vehicle pair has: the FILE keeps the prefix, the RESOURCE drops it.
    //
    // The reply rides ProcessInternalAcquireResponse with the response id staged at the
    // slot (60), which is what finally binds RaceCarStreamer::maWheelGraphicsResources.
    void GameDataModule::ProcessGetWheelRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                const GameDataIO::GameDataAssetEvent* lpEvent,
                                                s32 liEventId, s32 liSlotIndex)
    {
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        char lacWheelID[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacWheelID);

        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS,
                   "Invalid asset type for wheel\n");   // X360 line 4764

        // [marked deviation] the X360 leaves lacResourceName UNINITIALISED when the (already
        // asserted) meType is not GRAPHICS and hashes it anyway; terminate it so the host
        // cannot walk off the stack down that dead branch.
        char lacResourceName[208];
        lacResourceName[0] = '\0';

        if (lpEvent->meType == E_ASSETSET_GRAPHICS)
        {
            CgsCore::SPrintf(lacResourceName, 128, KPC_VEHICLE_RESOURCE_FORMAT,
                             lacWheelID + 4, "Graphics");
        }

        // The X360 24-byte type-4 record: {mpUser = &mReceiverQueue, miEventId = the slot
        // index, miPoolId = the request's pool, mResourceId = ID::HashString(name)}.
        // mbCheckRefCount lies outside the X360 record; false per the committed convention.
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

    // @ 0x8266F830 -- service a LOAD world-collision request: stream "worldcol.bin" into the
    // request's pool. Response id staged at the slot (32); ProcessInternalLoadBundleResponse's
    // case 32 is TERMINAL (bump the ref count, reply id 32) -- there is deliberately no paired
    // GET, and the case-57 assert spells out why: the per-zone geometry is acquired afterwards
    // through WorldEntityModule::PrepareZoneCollision's "TRK_CLIL<n>" AcquireResourceList calls.
    //
    // Structurally ProcessLoadPVSRequest with the same 148-byte type-2 LoadBundleRequest build
    // and the same `li r5,2 / li r6,0x94` AddEvent tail, plus one extra leading guard: the
    // collision world must not currently be invalidated (the swap-out/swap-in path sets that
    // flag while the bundle is being replaced).
    void GameDataModule::ProcessLoadWorldCollisionRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                          const GameDataIO::GameDataAssetEvent* lpEvent,
                                                          s32 liEventId, s32 liSlotIndex)
    {
        // X360 line 4326. The flag is a1+475940 -- committed as miWorldCollisionValidatePending
        // with a FLAGged role name; this assert text is the first real attestation of what it
        // means ("invalidated"), so the role is now confirmed even though the identifier is not.
        CGS_ASSERT(miWorldCollisionValidatePending == 0,
                   "Attempted collision world operation when it is invalidated\n");
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_PHYSICS,
                   "Invalid asset type for world collision\n");   // X360 line 4327

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_WORLD_COLLISION_FILE_NAME);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        // ⚠️ NOT the PVS/lanes tail: the X360 stores 1 here (`li r11,1; stb r11,0x91(sp)` --
        // v26 = 1 in the decompilation), i.e. worldcol.bin IS routed through the HD cache.
        // It is the only one of the five LoadBundle builders in this file that sets it.
        lRequest.mbUseHDCache        = true;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F398 -- service a LOAD traffic-lanes request: stream the traffic lane-graph
    // bundle ("B5Traffic.bndl") into the request's pool. Response id staged at the slot (30);
    // ProcessInternalLoadBundleResponse's case 30 then fires the paired GET (id 55).
    //
    // Store-for-store identical to ProcessLoadPVSRequest @0x8266F9C0 apart from the file name
    // and the assert text -- the asm is the same 148-byte type-2 LoadBundleRequest build with
    // the same field order (mpUser <- &mReceiverQueue @+0x00, miEventId <- the slot index
    // @+0x04, SetFileName, mbLiveUpdateReplace <- 0 @+0x08, miPoolId <- lwz 8(event) @+0x8C,
    // mbAllowFailiure <- lbz 0x1C(event) @+0x90, mbUseHDCache <- 0 @+0x91) and the same
    // `li r5,2 / li r6,0x94` AddEvent tail.
    void GameDataModule::ProcessLoadTrafficLanesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                        const GameDataIO::GameDataAssetEvent* lpEvent,
                                                        s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for traffic lanes\n");   // X360 line 4188 (0x105C)

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_TRAFFIC_LANES_FILE_NAME);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F4B0 -- service a LOAD AI-lanes request: stream the AI section graph
    // ("AI.dat") into the request's pool. Response id staged at the slot (29);
    // ProcessInternalLoadBundleResponse's case 29 then fires the paired GET (id 54).
    // Byte-for-byte the same shape as ProcessLoadTrafficLanesRequest above.
    void GameDataModule::ProcessLoadAILanesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                   const GameDataIO::GameDataAssetEvent* lpEvent,
                                                   s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for AI lanes\n");   // X360 line 4216 (0x1078)

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_AI_LANES_FILE_NAME);
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

    // ProcessLoadPropPhysicsRequest -- service a LOAD prop-physics request: stream the game's
    // single prop-physics bundle into the request's pool. Response id staged at the slot (34);
    // ProcessInternalLoadBundleResponse's case 34 then fires the paired GET (id 61) that
    // acquires "PRP_PHYSICS_" out of it. Producer: PropEntityModule::Prepare @0x82306DB8 ->
    // RequestInterface<1024>::LoadPropPhysics @0x82303DA0.
    //
    // ⚠️ NO OWN X360 ADDRESS. Every other Load handler in this file is a standalone function
    // that ProcessLoadGameDataEvent @0x82671EA0 calls; this one is inlined into that
    // dispatcher, which the export set does not carry (see the file-name constant's
    // provenance note above for how that was established). What IS attested:
    //   * the method exists with exactly this shape -- DecFIGS DWARF BrnGameDataModule.h:608,
    //     `ProcessLoadPropPhysicsRequest(InputBuffer*, const LoadGameDataEvent*, int32_t,
    //     int32_t)`;
    //   * response id 34 -- ProcessInternalLoadBundleResponse @0x82672630 case 0x22 is the
    //     slot state this handler must stage, and it dispatches ProcessGetPropPhysicsRequest
    //     with id 61;
    //   * the body shape -- the canonical 148-byte type-2 LoadBundleRequest build shared
    //     store-for-store by ProcessLoadPVSRequest @0x8266F9C0 / ProcessLoadTrafficLanes-
    //     Request @0x8266F398 / ProcessLoadAILanesRequest @0x8266F4B0 / ProcessLoadSurface-
    //     ListRequest @0x8266F718, all in this TU.
    // What is NOT attested and is therefore ABSENT rather than guessed: the asset-type assert
    // its siblings all open with (the X360 request carries meType == E_ASSETSET_PHYSICS, but
    // the assert text and its presence cannot be read), and any non-default mbUseHDCache
    // (left at the memset default, i.e. false -- the value four of the five siblings use;
    // only worldcol.bin sets it).
    void GameDataModule::ProcessLoadPropPhysicsRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                       const GameDataIO::GameDataAssetEvent* lpEvent,
                                                       s32 liEventId, s32 liSlotIndex)
    {
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_PROP_PHYSICS_BUNDLE_FILE_NAME);
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

    // @ 0x826703B0 -- service a GET traffic-lanes request: acquire the "BaseTraffic"
    // TrafficData resource (type 65538) from the request's pool, now that B5Traffic.bndl is
    // resident. Response id staged at the slot (55); ProcessInternalAcquireResponse's
    // case 55 posts the resolved handle back to the requester (WorldMap::LoadData state 3).
    //
    // Store-for-store identical to ProcessGetPVSRequest @0x82670880 apart from the resource
    // name and the assert text.
    //
    // ⚠️ Hex-Rays reads the id store as `HIDWORD(v14) = a1 + 363096` -- i.e. the receiver-queue
    // pointer in the id's HIGH half (known bug class (b)). It is a FUSION ARTIFACT, the same
    // one the WorldMap gate note and BrnWorldModule.cpp:191-194 already call out. The asm has
    // two INDEPENDENT stores to two different slots:
    //     0x82670498  std  r11, var_50(r1)   <- r11 = HashString(...), the 8-byte mResourceId
    //     0x8267049C  stw  r10, var_60(r1)   <- r10 = &mReceiverQueue, mpUser at +0x00
    // and HashString @0x828D84A8 ends `clrldi r3,r11,32`, so the stored id is the ZERO-EXTENDED
    // 32-bit CRC. Nothing is ORed into it.
    void GameDataModule::ProcessGetTrafficLanesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                       const GameDataIO::GameDataAssetEvent* lpEvent,
                                                       s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for traffic lanes\n");   // X360 line 4832 (0x12E0)

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(KPC_TRAFFIC_LANES_RESOURCE_NAME)))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x826704C0 -- service a GET AI-lanes request: acquire the "WorldMapData" AISections
    // resource (type 65537) from the request's pool. Response id staged at the slot (54).
    // Same shape as ProcessGetTrafficLanesRequest above (same Hex-Rays fusion artifact on the
    // id store at 0x826705A8/0x826705AC -- two separate stores in the asm).
    void GameDataModule::ProcessGetAILanesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                  const GameDataIO::GameDataAssetEvent* lpEvent,
                                                  s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for AI lanes\n");   // X360 line 4860 (0x12FC)

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(KPC_AI_LANES_RESOURCE_NAME)))));
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

    // @ 0x8266FC80 -- service a GET prop-graphics-list request: acquire the resource named
    // by the request's own id string ("PRP_GL__<zone>"). Response id staged at the slot (63).
    //
    // Instruction-for-instruction ProcessGetPropInstancesRequest @0x8266FB68 above -- the two
    // asm listings differ only in their entry addresses. Producer side: the "PRP_GL__%d"
    // GetGameDataEvent is built inline by PropEntityModule::PreSceneUpdate @0x82309A40
    // (`CgsCore::SPrintf(..., "PRP_GL__%d", zone)` -> VariableEventQueue<1024,16>::
    // AddEvent<GetGameDataEvent>(..., 49)).
    //
    // [resident-resource note] This GET needs no paired LOAD: the type-0x10010
    // (PropGraphicsList) resource is already inside the zone's world bundle. Verified on the
    // shipped data -- TRK_UNIT100_GR.BNDL carries entry id 0xDF80E759 / type 0x10010 whose
    // ResourceStringTable name is "PRP_GL__100", and CRC32-lowercase("PRP_GL__100") ==
    // 0xDF80E759. Same for every zone.
    void GameDataModule::ProcessGetPropGraphicsListRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
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

    // @ 0x8266FAD8 -- service a GET prop-physics request: acquire the game's ONE PropPhysics
    // resource (type 0x1000F). Response id staged at the slot (61).
    //
    // ⚠️ The one prop GET that does NOT hash its own id. The asm converts the request id into
    // the stack buffer (`ld r3,0x10(event); addi r4,r1,var_B0; bl CgsIDConvertToString`) and
    // then IGNORES the result: HashString is handed the fixed literal instead
    // (`lwz r3, off_82F2A744@l(r11)  # "PRP_PHYSICS_"` at 0x8266FB20). That dead conversion is
    // reproduced here because it is what the binary does -- the prop-physics table is global,
    // not per-zone, so the request's "PRP_PHYS..." id never varies and the copy-pasted
    // conversion was left in the original source. Everything else is store-for-store the
    // prop-instances twin.
    //
    // Hop 2 of a two-hop fetch: ProcessInternalLoadBundleResponse's case 34 dispatches here
    // once PROPS/PROPPHYSICS.BUNDLE is resident (that bundle -- unlike the prop-instance
    // resources -- is a real standalone file on disc, and the LOAD leg is what streams it).
    void GameDataModule::ProcessGetPropPhysicsRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                      const GameDataIO::GameDataAssetEvent* lpEvent,
                                                      s32 liEventId, s32 liSlotIndex)
    {
        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        // Dead in the X360 too (see the banner) -- kept so the reconstruction has no side
        // effect the binary lacks and none the binary has.
        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);
        (void)lacResourceName;

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(KPC_PROP_PHYSICS_RESOURCE_NAME)))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // ====================================================================================
    // The three LIST GET handlers -- @0x82666688 (vehicle, id 52), @0x826667C8 (ICE, id 64)
    // and @0x82666868 (wheel, id 59). All three are 40-instruction leaves.
    //
    // ⚠️ These do NOT stream anything, and that is the whole point: the tables were made
    // resident by Prepare stages 9/12, so the GET just posts a reply whose resource-memory
    // lane is a POINTER TO THE EMBEDDED MANAGER (the console literally stores `this+444336`
    // / `this+458696`) and recycles the slot immediately. Note the two IMMEDIATES the X360
    // bakes into the reply and does NOT take from the request: miPoolId = 5 and mId = the
    // fixed CgsID of the table ("VL__VEHICIST" 0xC98B447411F97E38 / "WL__WHEELIST"
    // 0xCF5D625701228838, both base-40 decoded); meType = 3 likewise.
    //
    // [x64 note] the reply's mHandle.mpResourceMemory carries the manager pointer directly,
    // matching the console's single `stw r11, +0x20` store; mpSourceEntry stays null (the
    // X360 leaves that word untouched -- there is no pool entry behind a resident member).
    // ====================================================================================

    void GameDataModule::ProcessGetVehicleListRequest(
            CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
            const GameDataIO::GameDataAssetEvent* lpEvent, s32 liEventId, s32 liSlotIndex)
    {
        GameDataIO::GameDataAssetEvent lResponse;
        memset(&lResponse, 0, sizeof(lResponse));
        lResponse.miEventId       = lpEvent->miEventId;
        lResponse.mpReceiverQueue = 0;
        lResponse.miPoolId        = 5;                       // X360 immediate
        lResponse.mId             = 0xC98B447411F97E38ull;   // CgsID("VL__VEHICIST")
        lResponse.meType          = static_cast<EAssetSet>(3);
        lResponse.mbFailFlag      = false;
        lResponse.mHandle.mpResourceMemory = &mVehicleList;
        lResponse.mHandle.mpSourceEntry    = 0;

        lpEvent->mpReceiverQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), liEventId,
            static_cast<s32>(sizeof(lResponse)));

        mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
    }

    // @ 0x826667C8 -- the ICE take-dictionary list GET (id 64). Instruction-for-instruction
    // the vehicle handler with two different immediates: the reply's mId is the fixed CgsID
    // 0x7DDFC0102EE70838 (base-40 "IL__ICEMLIST" -- the SAME id RequestInterface<512>::
    // GetICEList @0x82256358 puts in the request) and the resource-memory lane carries
    // `this + 457664`, i.e. &mICEList. (X360 `addis r11, r31, 7; addi r11, r11, -0x440` ==
    // this + 0x70000 - 0x440 -- the identical expression PrepareICEList's AddListResource
    // uses, which is what pins the member.)
    void GameDataModule::ProcessGetICEListRequest(
            CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
            const GameDataIO::GameDataAssetEvent* lpEvent, s32 liEventId, s32 liSlotIndex)
    {
        GameDataIO::GameDataAssetEvent lResponse;
        memset(&lResponse, 0, sizeof(lResponse));
        lResponse.miEventId       = lpEvent->miEventId;
        lResponse.mpReceiverQueue = 0;
        lResponse.miPoolId        = 5;                       // X360 immediate
        lResponse.mId             = 0x7DDFC0102EE70838ull;   // CgsID("IL__ICEMLIST")
        lResponse.meType          = static_cast<EAssetSet>(3);
        lResponse.mbFailFlag      = false;
        lResponse.mHandle.mpResourceMemory = &mICEList;
        lResponse.mHandle.mpSourceEntry    = 0;

        lpEvent->mpReceiverQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), liEventId,
            static_cast<s32>(sizeof(lResponse)));

        // [PC diagnostic -- PRODUCER end] one-shot. The consumer end prints the pointer it
        // stored (DirectorResourceManager::LogShotGroupBank); print what was actually sent so
        // the two can be compared without guessing which side dropped it.
        static bool s_bLoggedICEListReply = false;
        if (!s_bLoggedICEListReply)
        {
            s_bLoggedICEListReply = true;
            *CgsDev::Log::gpDebugPrint
                << "[GameData] ProcessGetICEListRequest: replied id " << liEventId
                << " with &mICEList=" << static_cast<void*>(&mICEList)
                << " (" << mICEList.GetICEMovieCount() << " takes)\n";
        }

        mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
    }

    void GameDataModule::ProcessGetWheelListRequest(
            CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
            const GameDataIO::GameDataAssetEvent* lpEvent, s32 liEventId, s32 liSlotIndex)
    {
        GameDataIO::GameDataAssetEvent lResponse;
        memset(&lResponse, 0, sizeof(lResponse));
        lResponse.miEventId       = lpEvent->miEventId;
        lResponse.mpReceiverQueue = 0;
        lResponse.miPoolId        = 5;                       // X360 immediate
        lResponse.mId             = 0xCF5D625701228838ull;   // CgsID("WL__WHEELIST")
        lResponse.meType          = static_cast<EAssetSet>(3);
        lResponse.mbFailFlag      = false;
        lResponse.mHandle.mpResourceMemory = &mWheelList;
        lResponse.mHandle.mpSourceEntry    = 0;

        lpEvent->mpReceiverQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), liEventId,
            static_cast<s32>(sizeof(lResponse)));

        mGameDataEventSlotPool.PushIndex(static_cast<s16>(liSlotIndex));
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
                ProcessGetVehicleRequest(lpResourceInput, &lpSlot->mEvent, 50, liSlotIndex);
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

        case 29:   // AI lanes -- hop 2: AI.dat is resident, acquire "WorldMapData"
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3238
            ProcessGetAILanesRequest(lpResourceInput, &lpSlot->mEvent, 54, liSlotIndex);
            break;

        case 30:   // traffic lanes -- hop 2: B5Traffic.bndl is resident, acquire "BaseTraffic"
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3245
            ProcessGetTrafficLanesRequest(lpResourceInput, &lpSlot->mEvent, 55, liSlotIndex);
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

        case 34:   // prop physics -- hop 2: PROPS/PROPPHYSICS.BUNDLE is resident, acquire
                   // "PRP_PHYSICS_" (X360 case 0x22 -> ProcessGetPropPhysicsRequest(...,61,...))
            CGS_ASSERT(!lbFailed, "Failed to load\n");   // X360 line 3302
            ProcessGetPropPhysicsRequest(lpResourceInput, &lpSlot->mEvent, 61, liSlotIndex);
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
                ProcessGetWheelRequest(lpResourceInput, &lpSlot->mEvent, 60, liSlotIndex);
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
