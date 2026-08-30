#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"            // base
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModule.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"
#include "rw/rwcore_structs.h"                                       // rw::IResourceAllocator (DebugComponentParams)

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

    struct DiskLayout;   // forward (ResourceModule::InitOptions holds a pointer only)
    namespace PoolIO { struct OutputBuffer; struct InputBuffer; }   // shuttle buffers
    namespace ResourceIO { struct InputBuffer; }                    // the module's request input
    // The write-stream request event types (deprecated adders take these by pointer only).
    namespace Events
    {
        struct OpenReadStreamRequest;
        struct CloseReadStreamRequest;
        struct OpenWriteStreamRequest;
        struct CloseWriteStreamRequest;
    }

    // CgsResource::DebugComponentParams (DWARF CgsResourceDebugComponent.h:57) - the debug-component
    // bring-up params carried in ResourceModule::InitOptions. The X360 callback signature is
    // void(*)(renderengine::Texture*, Vector2, Vector2, void*, bool, bool); modelled as a generic
    // function pointer to avoid the renderengine/Vector2 include cascade (a pointer either way, so the
    // layout is faithful). ConstructResourceModule fills: callback=TextureRenderCallback,
    // userData=the GameDataModule, debugAllocator=Allocators::mpInternalDebugAllocator.
    struct DebugComponentParams
    {
        void*                   mpTextureRenderCallback;   // :59 (DebugTextureRenderCallback)
        void*                   mpTextureRenderUserData;   // :60
        rw::IResourceAllocator* mpDebugAllocator;          // :61
    };

    class ResourceModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        static const s32 KI_MAX_PENDING_FILE_SYSTEM_RESPONSES = 16;

        // DecFIGS CgsResourceModule.h:209-213. A pending asynchronous file
        // response, its resource-queue event tag, and the file/stream slot id.
        struct PendingFileResponse
        {
            void* mpResponse;
            s32   meEvent;
            u32   muFileId;
        };

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

        // CgsResource::ResourceModule::InitOptions (DWARF CgsResourceModule.h:148) - the full bring-up
        // options ConstructResourceModule (X360 0x8266D570) builds on the stack (1216B on X360) and
        // ResourceModule::Construct consumes: the three sub-module option blocks (handed to each
        // sub-module's Construct) + the disk layout + debug params + the file search path / cache drive.
        struct InitOptions
        {
            CgsMemory::MemoryModule::InitOptions  mMemoryInitOptions;       // :150
            BundleLoaderModule::InitOptions       mLoaderInitOptions;       // :151
            PoolModule::InitOptions               mPoolInitOptions;         // :152
            DiskLayout*                           mpDiskLayout;             // :153
            DebugComponentParams                  mDebugParams;             // :155
            char                                  macFileSearchPath[1024];  // :157
            const char*                           mpcDiskCacheDrive;        // :158

            InitOptions();   // :148 zero-init (X360 ctor + memset 0)
        };

        // "New module": skip ModuleSingleBuffered's old DataStructure IO path (X360 *(this+4)=1).
        ResourceModule()
        {
            mbIsNewModule = true;
            mPendingFileResponses.Construct(
                maPendingFileResponses, maiPendingFileResponseFreeIndices,
                static_cast<s8>(KI_MAX_PENDING_FILE_SYSTEM_RESPONSES));
        }

        void Construct(const void* lpInitOptions, void* lpAllocator);  // deferred
        bool Prepare();    // 0x828F4140
        bool Release();    // 0x82906570
        void Destruct();   // deferred
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);        // deferred

        // ---- request/response shuttle steps (the pool-create slice of ResourceModule::Update) ---------
        // @ 0x829019F0 - forward the pool module's resource-memory requests (its PoolIO::OutputBuffer
        // resource-request queue) into the MemoryModule input queue. Caller holds memInput write-locked +
        // poolOutput read-locked (as ResourceModule::Update does).
        void ProcessPoolResourceRequests(CgsMemory::MemoryIO::InputBuffer* lpMemInput,
                                         PoolIO::OutputBuffer* lpPoolOutput);
        // @ 0x828EC6F0 - forward each MemoryModule response to its originating receiver queue
        // (response->GetUser()) tagged for that response type. Caller holds memOutput read-locked.
        void ProcessMemoryResponses(CgsMemory::MemoryIO::OutputBuffer* lpMemOutput);

        // @ 0x82907268 - route the module's inbound resource requests to the sub-module inputs. Pool-create
        // slice: CreatePool (id 0) -> pool input. Caller holds resIn read-locked + poolIn write-locked.
        void ProcessResourceRequests(ResourceIO::InputBuffer* lpResIn, PoolIO::InputBuffer* lpPoolIn);

        // Drain the pool module's output queue (acquire/etc. responses) and route each to its requester
        // (response->mpUser). The pool-response slice of the X360 ProcessResourceResponses shuttle. Caller
        // holds the pool output buffer read-locked.
        void ProcessPoolOutputResponses(PoolIO::OutputBuffer* lpPoolOut);

        // Accessor so the GameDataModule's CreatePools/CreateBanks can drive the embedded PoolModule
        // (the X360 reaches it through the resource-module vtable; here a by-name accessor).
        PoolModule&             GetPoolModule()   { return mPoolModule; }
        CgsMemory::MemoryModule& GetMemoryModule() { return mMemoryModule; }

        // ---- file-system request adders (Wave 49) --------------------------------------------
        // The two deprecated write-stream adders are bodied (bare "Write streams deprecated"
        // assert + return false; @0x828D8310 / @0x828D83A0). The four real file-system adders
        // (AddAddPatchRequest @0x828FD5C8 / AddCloseFileRequest @0x828F3EE0 /
        // AddRegisterMemFileRequest @0x828EC600 / AddUnregisterMemFileRequest @0x828EC660) are
        // PARTIAL: they need a cross-TU dependency web (FileSystem::{Lock,Unlock,Close,
        // Register,Unregister}Internal + GetPatchManager, PatchManager::AddPatch, FileSystemAlloc,
        // CgsContainers::IndexedPool8<PendingFileResponse,16>, and the FileEvent/FileHandleEvent/
        // CloseFileRequest/RegisterMemFileRequest/UnregisterMemFileRequest event structs) that is
        // not yet homed, so they are intentionally NOT declared/bodied here (left to a later wave
        // to grow coherently, same disposition as wave44's blocked BinaryFileResource members).
        bool AddOpenWriteStreamRequest(const Events::OpenWriteStreamRequest* lpRequest);   // @0x828D8310
        bool AddCloseWriteStreamRequest(const Events::CloseWriteStreamRequest* lpRequest); // @0x828D83A0
        bool AddOpenReadStreamRequest(const Events::OpenReadStreamRequest* lpRequest);     // @0x829070B0
        bool AddCloseReadStreamRequest(const Events::CloseReadStreamRequest* lpRequest);   // @0x82905428
        void ProcessPendingFileSystemResponses();                                           // @0x828F42B8

    private:
        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        EPrepareStage             mePrepareStage;       // +0x228 (a1[138])
        EReleaseStage             meReleaseStage;       // +0x22C (a1[139])
        BundleLoaderModule        mBundleLoaderModule;  // +0x280 (a1+160)
        PoolModule                mPoolModule;          // +0x2C500 (a1+45280)
        CgsMemory::MemoryModule   mMemoryModule;        // +0x46580 (a1+72032)
        CgsFileSystem::FileSystem mFileSystem;          // +0x4684C (a1+72204)
        DebugComponent            mDebugComponent;      // +0x14B96 ... (a1+84806)
        PendingFileResponse       maPendingFileResponses[KI_MAX_PENDING_FILE_SYSTEM_RESPONSES];
        s8                        maiPendingFileResponseFreeIndices[KI_MAX_PENDING_FILE_SYSTEM_RESPONSES];
        CgsContainers::IndexedPool<PendingFileResponse,
                                   KI_MAX_PENDING_FILE_SYSTEM_RESPONSES, s8>
                                  mPendingFileResponses;
    };
}
