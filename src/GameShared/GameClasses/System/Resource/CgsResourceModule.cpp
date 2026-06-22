#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>                                    // memset (InitOptions zero-init)

// CgsResource::ResourceModule - see the header. This pass reconstructs the lifecycle
// orchestration spine (Prepare / Release). Construct + Update + the request shuttles are
// DEFERRED (rw allocator middleware) as inert marked stubs.
namespace CgsResource
{
    // Minimal placeholder debug component (deferred).
    void DebugComponent::Construct() {}
    void DebugComponent::Register() {}

    // ResourceModule::InitOptions ctor - zero-init. The X360 ConstructResourceModule constructs this
    // then memset(0)s the whole 1216B block before filling fields, so a zero-init is the faithful net
    // state. (memset over the already-constructed members matches that ctor-then-memset sequence.)
    ResourceModule::InitOptions::InitOptions()
    {
        memset(this, 0, sizeof(*this));
    }

    // @ 0x828F4140 - resumable bring-up: base module, then Memory -> FileSystem -> Bundle ->
    // Pool, then register the debug component. Re-enters at the current stage if a sub-module
    // is not yet ready.
    bool ResourceModule::Prepare()
    {
        // [reliable] The X360 sets *(this+4)=1 (mbIsNewModule) in Construct; that Construct chain isn't
        // wired for the embedded static yet, and the header ctor doesn't take effect on it, so set it
        // here (before the base Prepare) so ModuleSingleBuffered skips the old DataStructure IO path
        // (and its virtual CreateInputDataStructure). Move to Construct once the bring-up chain lands.
        mbIsNewModule = true;
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_BASE:
            mePrepareStage = E_PREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_PREPARE_MEMORY:
            mePrepareStage = E_PREPARE_MEMORY;
            if (!mMemoryModule.Prepare())
                return false;
            // fall through
        case E_PREPARE_FILESYSTEM:
            mePrepareStage = E_PREPARE_FILESYSTEM;
            if (!mFileSystem.Prepare())
                return false;
            // fall through
        case E_PREPARE_BUNDLE:
            mePrepareStage = E_PREPARE_BUNDLE;
            if (!mBundleLoaderModule.Prepare())
                return false;
            // fall through
        case E_PREPARE_POOL:
            mePrepareStage = E_PREPARE_POOL;
            if (!mPoolModule.Prepare())
                return false;
            mDebugComponent.Register();
            // fall through
        case E_PREPARE_DONE:
            meReleaseStage = E_RELEASE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x82906570 - resumable tear-down: Pool -> Bundle -> FileSystem -> Memory, then the
    // base module (reverse of Prepare).
    bool ResourceModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
        case E_RELEASE_POOL:
            meReleaseStage = E_RELEASE_POOL;
            if (!mPoolModule.Release())
                return false;
            // fall through
        case E_RELEASE_BUNDLE:
            meReleaseStage = E_RELEASE_BUNDLE;
            if (!mBundleLoaderModule.Release())
                return false;
            // fall through
        case E_RELEASE_FILESYSTEM:
            meReleaseStage = E_RELEASE_FILESYSTEM;
            if (!mFileSystem.Release())
                return false;
            // fall through
        case E_RELEASE_MEMORY:
            meReleaseStage = E_RELEASE_MEMORY;
            if (!mMemoryModule.Release())
                return false;
            // fall through
        case E_RELEASE_BASE:
            meReleaseStage = E_RELEASE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Release())
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

    // @ 0x829055B0 - construct the sub-modules over the resource heaps. [5b in progress] lpInitOptions
    // is now the real CgsResource::ResourceModule::InitOptions composite (the GameDataModule builds it);
    // each sub-module gets its own option block. Brings up the MemoryModule (carves the bank/block tables
    // from the resource allocator, from mMemoryInitOptions) and the PoolModule structural front half
    // (base + 128 Pool::Construct + stages); the PoolModule allocator-gated back half (consumes
    // mPoolInitOptions), BundleLoaderModule/FileSystem Construct + the FileSystem GeneralResourceAllocator
    // remain deferred (next 5b passes). lpAllocator is the root rw::IResourceAllocator.
    void ResourceModule::Construct(const void* lpInitOptions, void* lpAllocator)
    {
        if (lpInitOptions == 0 || lpAllocator == 0)
            return;
        const InitOptions* lpOptions = static_cast<const InitOptions*>(lpInitOptions);
        rw::IResourceAllocator* lpRwAllocator = static_cast<rw::IResourceAllocator*>(lpAllocator);

        mMemoryModule.Construct(
            const_cast<CgsMemory::MemoryModule::InitOptions*>(&lpOptions->mMemoryInitOptions),
            lpRwAllocator);
        // PoolModule front half (rw-allocator-independent: base + 128 Pool::Construct + stage init).
        // It receives its own mPoolInitOptions; the front half ignores it (the type-registry /
        // ScratchPool / region allocations that consume it are deferred to the back half).
        mPoolModule.Construct(&lpOptions->mPoolInitOptions, lpAllocator);
    }
    // Update (0x82907948) pumps each sub-module + shuttles requests; Destruct (0x828EC6B0) tears them
    // down. Deferred.
    void ResourceModule::Destruct() {}
    bool ResourceModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
}
