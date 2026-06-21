#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"            // base
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModule.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"

// CgsResource::ResourceModule - the resource-streaming engine: the container module that owns
// and orchestrates the four resource sub-modules (BundleLoaderModule, PoolModule, MemoryModule,
// FileSystem) plus a debug component. It is embedded by value inside BrnResource::GameDataModule,
// whose loading-machine case 8 drives it. It is a CgsModule::ModuleSingleBuffered; its Update
// pumps each sub-module and shuttles requests/responses between them (memory <-> pool <-> bundle
// <-> filesystem), which is the heart of resource streaming.
//
// SOURCES (X360 ARTIST): ctor 0x827E1F68, Construct 0x829055B0, Prepare 0x828F4140, Release
// 0x82906570, Destruct 0x828EC6B0, Update 0x82907948, + the Add*Request / Process* request
// shuttles. The ctor embeds (by value, in order): BundleLoaderModule @a1+160, PoolModule
// @a1+45280, MemoryModule @a1+72032, CgsFileSystem::FileSystem @a1+72204, DebugComponent @a1+84806.
//
// DEFER STATUS: this pass reconstructs the lifecycle ORCHESTRATION spine - Prepare (0x828F4140)
// and Release (0x82906570), the resumable stage machines that bring the four sub-modules up (base
// -> Memory -> FileSystem -> Bundle -> Pool + register debug) and tear them down in reverse. They
// are rw-INDEPENDENT (they only chain the embedded modules' own Prepare/Release). Construct
// (allocates everything through the rw allocator), Update and the request shuttles are DEFERRED
// (rw allocator middleware) as inert marked stubs.
namespace CgsResource
{
    // Minimal placeholder for the embedded resource debug component (real type
    // CgsResource::DebugComponent : CgsDev::DebugComponent - reached only via Register() here;
    // embedded by value so it is kept light to avoid the debug-UI include cascade).
    class DebugComponent
    {
    public:
        void Construct();
        void Register();
    };

    class ResourceModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // The bring-up order is base -> Memory -> FileSystem -> Bundle -> Pool (Release reverses it).
        enum EPrepareStage
        {
            E_PREPARE_START = 0, E_PREPARE_BASE = 1, E_PREPARE_MEMORY = 2, E_PREPARE_FILESYSTEM = 3,
            E_PREPARE_BUNDLE = 4, E_PREPARE_POOL = 5, E_PREPARE_DONE = 6
        };
        enum EReleaseStage
        {
            E_RELEASE_START = 0, E_RELEASE_POOL = 1, E_RELEASE_BUNDLE = 2, E_RELEASE_FILESYSTEM = 3,
            E_RELEASE_MEMORY = 4, E_RELEASE_BASE = 5, E_RELEASE_DONE = 6
        };

        void Construct(const void* lpInitOptions, void* lpAllocator);  // deferred
        bool Prepare();    // 0x828F4140
        bool Release();    // 0x82906570
        void Destruct();   // deferred
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);        // deferred

    private:
        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        EPrepareStage             mePrepareStage;       // +0x228 (a1[138])
        EReleaseStage             meReleaseStage;       // +0x22C (a1[139])
        BundleLoaderModule        mBundleLoaderModule;  // +0x280 (a1+160)
        PoolModule                mPoolModule;          // +0x2C500 (a1+45280)
        CgsMemory::MemoryModule   mMemoryModule;        // +0x46580 (a1+72032)
        CgsFileSystem::FileSystem mFileSystem;          // +0x4684C (a1+72204)
        DebugComponent            mDebugComponent;      // +0x14B96 ... (a1+84806)
    };
}
