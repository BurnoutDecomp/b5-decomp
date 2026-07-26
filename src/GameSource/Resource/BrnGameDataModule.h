#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"          // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"        // mReceiverQueue
#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"        // mResourceModule (the streaming engine)
#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"                // mGameDataEventSlotPool
#include "GameSource/Resource/BrnGameDataEventSlot.h"                        // GameDataEventSlot (pool element)
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"           // GameDataIO::AllocatorList (mAllocatorList)

namespace CgsResource { namespace ResourceIO { struct InputBuffer; } }       // handler param (by pointer)
namespace CgsModule { struct Event; }                                        // pump param (by pointer)

// BrnResource::GameDataModule - Burnout's game-data streaming module: the ~480KB module that is
// the heart of resource loading. It embeds the CgsResource::ResourceModule (memory + pools +
// bundle loader + filesystem) and layers Burnout's game-data services on top (vehicles, wheels,
// traffic, world units, PVS, AI lanes, surfaces, props, attrib-sys vault, HUD messages, popups,
// freeburn challenges, ICE list, DLC). It is the module the loading machine drives via case 8
// (vtable prepare) while the loading screen / boot videos play.
//
// SOURCES (X360 ARTIST): ctor 0x827E3230, Construct 0x82671B90, Prepare 0x82673F38, Destruct
// 0x82664508, ConstructResourceModule 0x8266D570, CreateBanks 0x8266DA28, CreatePools 0x8266DB88,
// CreateAllocators 0x8266DD00, + the many ProcessGetXxxRequest game-data query handlers and the
// ProcessInternalXxxResponse resource-event handlers. The ctor embeds (by value): ResourceModule
// @a1+640, CgsAttribSys::AttribSysModule @a1+399000, 9x EA::Allocator::GeneralAllocator @a1+427416
// (stride 1312), the DLCManager + per-type IndexedLinkLists + HUD/popup controllers, etc.
//
// DEFER STATUS: reconstructed so far -- the rw-INDEPENDENT lifecycle spine (Prepare's base ->
// ResourceModule::Prepare core, 0x82673F38 stages 1-2) and, THIS BATCH, the world-request service
// path: the Update request pump (0x82674670, the GameDataIO input drain -> ProcessGameDataEvent
// routing), the request dispatchers (ProcessGameDataEvent 0x826744F0, ProcessLoadGameDataEvent
// 0x82671EA0, ProcessGetGameDataEvent 0x82672268, GetGameDataEventSlot 0x826664A0) and the FOUR
// world handlers (GetWorldUnit 0x826705D0, LoadPVS 0x8266F9C0, LoadSurfaceList 0x8266F718,
// LoadPropInstances 0x8266F178). The rw-allocator-gated stages (CreateBanks/CreatePools/
// CreateAllocators) are inert/bring-up stand-ins; the DLC / AttribSys / HUD / popup prepare
// stages, Destruct, the non-world ProcessXxxRequest handlers and the ProcessInternal*Response
// completion path are DEFERRED. The many embedded subsystems beyond ResourceModule are added
// with their own reconstruction passes.
namespace BrnResource
{
    namespace GameDataIO { struct GameDataAssetEvent; }   // dispatcher/handler param (by pointer)

    class GameDataModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        enum EPrepareStage
        {
            E_PREPARE_START = 0, E_PREPARE_BASE = 1, E_PREPARE_RESOURCE = 2, E_PREPARE_BANKS = 3,
            E_PREPARE_POOLS = 4, E_PREPARE_ALLOCATORS = 5, E_PREPARE_DONE = 6
        };

        // X360 @0x82671B90 Construct: event-slot pool capacity (96 slots wired inline:
        // `*(a1+439324) = 96` + element array @a1+439328 + free-index array @a1+443936).
        static const s32 KI_NUM_GAMEDATA_EVENT_SLOTS = 96;

        // [DIAG] ctor moved to the .cpp with logging to prove whether it runs + sets mbIsNewModule.
        GameDataModule();

        void Construct();   // 0x82671B90 (slot 0; the X360 takes NO arguments -- the old
                            // `const void* lpInitOptions` param here was fabricated)
        bool Prepare(void* lpInputBuffer, void* lpOutputBuffer);   // 0x82673F38
        bool Release();                               // deferred
        void Destruct();                              // deferred

        // @ 0x82674670 -- the per-frame pump: drain the GameDataIO::InputBuffer request
        // interface (raw CgsResource requests forward into the embedded ResourceModule input;
        // GameData-level requests route through ProcessGameDataEvent), then run the
        // ResourceModule. lpInputBuffer/lpOutputBuffer are the module's GameDataIO
        // InputBuffer/OutputBuffer (see the .cpp for the reconstructed slice + deferrals).
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);

    private:
        // rw-allocator-gated bring-up steps (deferred stubs report success).
        bool CreateBanks(void* lpInputBuffer, void* lpOutputBuffer);       // 0x8266DA28
        bool CreatePools(void* lpInputBuffer, void* lpOutputBuffer);       // 0x8266DB88
        bool CreateAllocators(void* lpInputBuffer, void* lpOutputBuffer);  // 0x8266DD00

        // ---- GameData request dispatch (X360 addresses on each body) --------------------
        // Route one queued GameData-level event by its queue type id (26 load / 39 unload /
        // 49 get / 67 swap-out / 68 swap-in).
        void ProcessGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                  const CgsModule::Event* lpEvent, s32 liEventType);   // 0x826744F0

        // Dispatch a LoadGameDataEvent / GetGameDataEvent by the uncompressed prefix of its
        // CgsID (liSlotIndex -1 = allocate a fresh event slot).
        void ProcessLoadGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                      const GameDataIO::GameDataAssetEvent* lpEvent,
                                      s32 liSlotIndex);                                // 0x82671EA0
        void ProcessGetGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                     const GameDataIO::GameDataAssetEvent* lpEvent,
                                     s32 liSlotIndex);                                 // 0x82672268
        void ProcessUnloadGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                        const GameDataIO::GameDataAssetEvent* lpEvent,
                                        s32 liSlotIndex);                              // 0x826733F8 (deferred body)
        void ProcessSwapInCollisionWorldRequest();                                     // 0x826717A8 (deferred body)
        void ProcessSwapOutCollisionWorldRequest();                                    // 0x82671530 (deferred body)

        // Fetch (liSlotIndex >= 0) or allocate (liSlotIndex < 0) an event slot and capture
        // the request event into it.
        GameDataEventSlot* GetGameDataEventSlot(const GameDataIO::GameDataAssetEvent* lpEvent,
                                                s32 liSlotIndex);                      // 0x826664A0

        // PC bring-up scaffolding (NOT an X360 function): shared deferral tail for the
        // dispatch cases whose ProcessXxxRequest handlers are not yet reconstructed --
        // log the drop and return the slot to the pool. [marked deviation: the X360
        // dispatches to the real handler and the slot lives until its
        // ProcessInternal*Response completion.]
        void DeferredGameDataRequest(const char* lpcHandlerName, GameDataEventSlot* lpSlot);

        // ---- the four reconstructed world handlers --------------------------------------
        void ProcessGetWorldUnitRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                        const GameDataIO::GameDataAssetEvent* lpEvent,
                                        s32 liEventId, s32 liSlotIndex);               // 0x826705D0
        void ProcessLoadPVSRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                   const GameDataIO::GameDataAssetEvent* lpEvent,
                                   s32 liEventId, s32 liSlotIndex);                    // 0x8266F9C0
        void ProcessLoadSurfaceListRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                           const GameDataIO::GameDataAssetEvent* lpEvent,
                                           s32 liEventId, s32 liSlotIndex);            // 0x8266F718
        void ProcessLoadPropInstancesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                             const GameDataIO::GameDataAssetEvent* lpEvent,
                                             s32 liEventId, s32 liSlotIndex);          // 0x8266F178

        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        EPrepareStage                  mePrepareStage;   // +0x228 (a1[138])
        EPrepareStage                  meReleaseStage;   // +0x22C (a1[139])
        CgsResource::ResourceModule    mResourceModule;  // +0x280 (a1+160) the streaming engine
        // X360 a1+363096: the module's internal receiver queue -- every request the world
        // handlers post into the ResourceModule names it as mpUser, and Update drains the
        // ProcessInternal*Response completions from it. The X360 Construct binds a 0x8000-byte
        // buffer with 16-byte alignment (`*(a1+363112)=0x8000; *(a1+363116)=16`), i.e. the
        // EventReceiverQueue<32768,16> instantiation.
        CgsModule::EventReceiverQueue<32768, 16> mReceiverQueue;
        // X360 a1+426400: the bank->allocator registry published to the OutputBuffer each
        // Update (SetAllocatorList). Populated by CreateAllocators (deferred); Construct
        // resets it (the inline `map[0..66] = -1` loop == AllocatorList::Construct).
        GameDataIO::AllocatorList      mAllocatorList;
        // X360 a1+439312: the in-flight request slot pool (IndexedPool<GameDataEventSlot,short>,
        // capacity 96) + its module-embedded backing arrays (X360 a1+439328 / a1+443936).
        CgsContainers::IndexedPool<GameDataEventSlot, KI_NUM_GAMEDATA_EVENT_SLOTS> mGameDataEventSlotPool;
        GameDataEventSlot              maGameDataEventSlots[KI_NUM_GAMEDATA_EVENT_SLOTS];
        s16                            masGameDataEventSlotFreeIndices[KI_NUM_GAMEDATA_EVENT_SLOTS];
        // (AttribSysModule, the 9 GeneralAllocators, DLCManager, per-type IndexedLinkLists, HUD
        //  message / popup controllers and the game-data tables are added with their own passes.)
    };
}

// Accessor for the one game-owned GameDataModule instance (BrnMain's gGameModule.mGameDataModule).
// Lets the loading flow drive the real instance without pulling in the heavy BrnGameModule.hpp.
namespace BrnGame { BrnResource::GameDataModule* GetMainGameDataModule(); }
