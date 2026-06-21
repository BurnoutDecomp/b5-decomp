#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"          // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"        // mReceiverQueue
#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"        // mResourceModule (the streaming engine)

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
// DEFER STATUS: this pass lands the type + the rw-INDEPENDENT lifecycle spine - Prepare's base ->
// ResourceModule::Prepare core (0x82673F38 stages 1-2). The rw-allocator-gated stages (CreateBanks
// /CreatePools/CreateAllocators) are inert stubs that report success so the stage machine advances;
// the DLC / AttribSys / HUD / popup prepare stages, Construct (0x82671B90), Update, Destruct and all
// the ProcessXxxRequest handlers are DEFERRED (rw allocator middleware + those subsystems). The many
// embedded subsystems beyond ResourceModule are added with their own reconstruction passes.
namespace BrnResource
{
    class GameDataModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        enum EPrepareStage
        {
            E_PREPARE_START = 0, E_PREPARE_BASE = 1, E_PREPARE_RESOURCE = 2, E_PREPARE_BANKS = 3,
            E_PREPARE_POOLS = 4, E_PREPARE_ALLOCATORS = 5, E_PREPARE_DONE = 6
        };

        // [DIAG] ctor moved to the .cpp with logging to prove whether it runs + sets mbIsNewModule.
        GameDataModule();

        void Construct(const void* lpInitOptions);   // deferred
        bool Prepare(void* lpInputBuffer, void* lpOutputBuffer);   // 0x82673F38
        bool Release();                               // deferred
        void Destruct();                              // deferred
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);    // deferred

    private:
        // rw-allocator-gated bring-up steps (deferred stubs report success).
        bool CreateBanks(void* lpInputBuffer, void* lpOutputBuffer);       // 0x8266DA28
        bool CreatePools(void* lpInputBuffer, void* lpOutputBuffer);       // 0x8266DB88
        bool CreateAllocators(void* lpInputBuffer, void* lpOutputBuffer);  // 0x8266DD00

        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        EPrepareStage                  mePrepareStage;   // +0x228 (a1[138])
        EPrepareStage                  meReleaseStage;   // +0x22C (a1[139])
        CgsResource::ResourceModule    mResourceModule;  // +0x280 (a1+160) the streaming engine
        CgsModule::BaseEventReceiverQueue mReceiverQueue; // +0x58A18 (a1+90774) game-data events
        // (AttribSysModule, the 9 GeneralAllocators, DLCManager, per-type IndexedLinkLists, HUD
        //  message / popup controllers and the game-data tables are added with their own passes.)
    };
}

// Accessor for the one game-owned GameDataModule instance (BrnMain's gGameModule.mGameDataModule).
// Lets the loading flow drive the real instance without pulling in the heavy BrnGameModule.hpp.
namespace BrnGame { BrnResource::GameDataModule* GetMainGameDataModule(); }
