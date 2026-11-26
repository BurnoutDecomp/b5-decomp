// =============================================================================
// CgsSound::Playback::Module::Module -- out-of-line members.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Module::Module            @ 0x827DFA98  (ctor -- EXECUTED in boot trace)
//   Module::GetEnvironment    @ 0x82694130
//   Module::DumpRegistries    @ 0x82694188
//   Module::ImportStringTable @ 0x826AD6B0
//   Module::AttachVoice       @ 0x826D7D80
//   Module::ConnectVoice      @ 0x826C14E0
//   Module::C  (CreateVoice)  @ 0x826D7B00
//
// Each method bodies store-for-store against the X360 asm. The asserts use the
// project CGS_ASSERT macro (the X360 BeginAssert/FireAssert/EndAssert triple);
// CGS_ASSERT supplies __FILE__/__LINE__ itself, so the original file/line numbers
// are dropped per AGENTS.md.
// =============================================================================

#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#define CGS_ARTIST_PLAYBACK_MODULE_CPP "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Module/CgsSoundPlaybackModule.cpp"

#include "rw/rwcore_structs.h"   // rw::Resource / rw::IResourceAllocator (the Release-machine DoFree wraps)

#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                 // Prepare's stream-buffer bump path
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // the 8 environment CPU monitors
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GetDefaultRwacSystem + RwacSystemLock (phase B4)
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"        // AemsFactory::Create (the stage-3 create, cascade slice 2)
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.h"  // SplicerFactory::Create (the stage-3 create, cascade slice 3)
#include <cstddef>

#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h" // SndPlayer1_CgsStreamMod::SetStreamProvider
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"    // OpenReadStreamRequest / ReadStreamEvent (phase B4)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"          // ID::HashString (DoServiceContentLoadRequest)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"         // BaseResourcePtr (the type-4 response rebind)

#include <cstring>  // memcpy / memset (the 0xF0 stream-buffer scrub)

namespace CgsSound
{
namespace Playback
{
namespace Module
{

namespace
{
}

// ---------------------------------------------------------------------------
// Module::Module  @ 0x827DFA98
//
// The X360 ctor runs the ModuleSingleBuffered base ctor (the intermediate vtable
// off_820CE500 + the two RWMutex(NULL, true) builds @+0x10/+0x118 -- since phase
// B1 that IS the inherited base ctor here), installs the two interface-base
// vptrs (off_820CE350/58, overwritten with the final off_820CFA24/20 by the
// derived hand-off -- since phase B4 those ARE the real IStreamProvider /
// IContentLoadService base sub-objects, and the paired installs are exactly the
// compiler's MI base-then-derived construction), zeroes the environment +
// factory handles, builds the stream mutex (Mutex(NULL, true) -- asm r4=0,
// r5=1), zeroes the deferred queue's leading count byte (+0x2298; the queue's
// Construct() re-establishes it), and nulls the string-table head.
// ---------------------------------------------------------------------------
Module::Module()
    : CgsModule::ModuleSingleBuffered()
    , mhEnvironment()        // X360 +0x2258 = 0
    , mhRwacFactory()        // X360 +0x225C = 0
    , mhAemsFactory()        // X360 +0x2260 = 0
    , mhSplicerFactory()     // X360 +0x2264 = 0 (DWARF h:374)
    , mStreamMutex(0, true)  // X360 +0x2268: Mutex(NULL, true) (DWARF h:379)
{
    // mpStringTable (+0x26FC, DWARF h:489) is left to Construct (ARTIST 0x827DFA98 never stores it).
}

// ---------------------------------------------------------------------------
// Module::Construct(int)  @ 0x826C0EC0  (vtable +0x40; RootSoundModule::Construct
// step [3] calls it with module-id 6)
//
// Store map (console offsets; every store lands on a named member):
//   +0x2700 mf32TimeStep = 0.0    +0x2708 mi32PoolId = a2   +0x2704 mf32TotalTime = 0.0
//   +0x26F0/F4/F8 sizes + flag = 0
//   ModuleSingleBuffered::Construct(this)
//   +0x230 mePrepareStage = 0     +0x234 meReleaseStage = 6 (DONE)
//   receiver queue: cap 0x2000 @+0x248, align 16 @+0x24C, storage this+0x250
//     @+0x238, then Clear  == EventReceiverQueue<0x2000,16>::Construct()
//   +0x26FC mpStringTable = 0     +0x2250/+0x2254 in/out buffers = 0
//   VariableEventQueue<1024,16>::Construct(+0x2298)
//   the 3 StreamBuffer records: {0, 4, 0, 0, 0, 0.0} each
//   +4 mbIsNewModule = 1 (base)
// ---------------------------------------------------------------------------
void Module::Construct(s32 li32PoolId)
{
    mf32TimeStep  = 0.0f;
    mi32PoolId    = li32PoolId;
    mf32TotalTime = 0.0f;

    muStreamBufferSize          = 0;
    muStreamNumBlocks           = 0;
    mbStreamsUsingMainAllocator = false;

    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_DONE;

    mResourceReceiverQueue.Construct();

    mpStringTable  = 0;
    mpInputBuffer  = 0;
    mpOutputBuffer = 0;

    mDeferredResourceRequestQueue.Construct();

    for (u32 lu = 0; lu < SKU_NUMBER_OF_STREAM_BUFFERS; ++lu)
    {
        maStreamBuffers[lu].Construct();   // {0, E_FREE_BUFFER, 0, 0, false, 0.0}
    }

    mbIsNewModule = true;
}

// ---------------------------------------------------------------------------
// Module::Prepare  @ 0x826E90C0  (DWARF: Prepare(rw::IResourceAllocator*,
// CgsMemory::LinearMalloc*); bodied 2026-08-25, faithful-audio-engine phase B3)
//
// The stage machine (each rung bumps mePrepareStage via the @0x82681C70
// operator and lowers the meReleaseStage countdown cursor):
//   0 -> 1: bump.
//   1: base Prepare + receiver Clear + deferred-queue Prepare (either false ->
//      still preparing); release cursor = 5.
//   2: Environment::Create with the DWARF-constant spec {allocator,
//      SKU32_FACTORY_COUNT=4, SKU32_VOICE_COUNT=192, SKU32_CONTENT_COUNT=128,
//      {SKU32_MAIN_REGISTRY_ENTITY_COUNT=2048, SKU_MAIN_REGISTRY_DATA_SIZE=
//      388608, 0}} -> mhEnvironment (assert cpp:177) + the filtered debug size
//      print; release cursor = 3.
//   3: the three factory creates + the stream-buffer carve + the 8 environment
//      CPU monitors; release cursor = 2 (via the raw bump + the .h:500 assert).
//   4 (DONE): release cursor = 0; return true.
//
// Stage 3 creates the three factories through GenericRwacFactory::Create
// (spec {default RWAC system, 128 entities, 32384 data}), AemsFactory::Create and
// SplicerFactory::Create (each spec carrying the RWAC factory, 128, 32384), all
// carved from the environment's allocator, asserting mhRwacFactory /
// mhAemsFactory / mhSplicerFactory, then publishes the
// module's stream-provider sub-object (+0x228) to the stream-provider global.
// ---------------------------------------------------------------------------
bool Module::Prepare(rw::IResourceAllocator* apAllocator,
                     CgsMemory::LinearMalloc* apLinearMalloc)
{
    CGS_ASSERT(apAllocator != 0, "lpAllocator");

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        mePrepareStage++;
        // fall through
    case E_PREPARESTAGE_MANAGER:
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            return false;
        mResourceReceiverQueue.Clear();
        if (!mDeferredResourceRequestQueue.Prepare())
            return false;
        mePrepareStage++;
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        // fall through
    case E_PREPARESTAGE_ENVIRONMENT:
    {
        EnvironmentSpec lSpec;
        lSpec.mpAllocator      = apAllocator;
        lSpec.mu32FactoryCount = 4;      // SKU32_FACTORY_COUNT (DWARF h:346)
        lSpec.mu32VoiceCount   = 192;    // SKU32_VOICE_COUNT (h:348; the 0xC0 word)
        lSpec.mu32ContentCount = 128;    // SKU32_CONTENT_COUNT (h:347; the 0x80 word)
        lSpec.mRegistrySpec.mu32EntityCount   = 2048;    // SKU32_MAIN_REGISTRY_ENTITY_COUNT (h:349)
        lSpec.mRegistrySpec.muDataSize        = 388608;  // SKU_MAIN_REGISTRY_DATA_SIZE (h:350)
        lSpec.mRegistrySpec.muStringTableSize = 0;

        // Assign the created handle (operator= takes a reference and drops the old
        // one); the temporary's destructor then drops its own (ARTIST 0x826E91E8..F8).
        mhEnvironment = Environment::Create(lSpec);
        CGS_ASSERT(mhEnvironment.GetObject() != 0, "mhEnvironment");

        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "Sound Playback - Environment (+ Registry) is "
                << static_cast<s32>(mhEnvironment.GetObject()->GetAllocatedSize())
                << " bytes in size.\n";
        }

        mePrepareStage++;
        meReleaseStage = E_RELEASESTAGE_ENVIRONMENT;
        // fall through
    }
    case E_PREPARESTAGE_FACTORIES:
    {
        // The THREE factory creates (console @0x826E92B8..; AEMS-cascade wave).
        // [1/3] RWAC -- REAL (GenericRwacFactory::Create @0x826C7AD0): spec
        // {off_83271928, 128 entities, 32384 data, 0 strings} built at
        // @0x826E92B8-D8 (the by-value r5:r6 packing); the returned temp handle
        // is assigned into +0x225C and asserted (cpp:196 "mhRwacFactory").
        // operator= takes the member's reference; the temporary's reference is
        // then released as its console destructor does (the same pattern for all
        // three factories).
        {
            GenericRwacFactorySpec lRwacSpec;
            lRwacSpec.mpSystem            = GetDefaultRwacSystem();
            lRwacSpec.mRegistrySpec.mu32EntityCount = 128;
            lRwacSpec.mRegistrySpec.muDataSize = 32384;
            lRwacSpec.mRegistrySpec.muStringTableSize = 0;
            mhRwacFactory = GenericRwacFactory::Create(*mhEnvironment, lRwacSpec);
            CGS_ASSERT(!!mhRwacFactory, "mhRwacFactory");
        }
        // [2/3] AEMS -- REAL (AemsFactory::Create @0x826DAC28, cascade slice 2):
        // spec {the RWAC handle, 128, 32384, 0} passed by reference; the temp
        // assigned into +0x2260 and asserted (cpp:207). The console releases the
        // temp through the IAems vtable +4 slot and then the spec's RWAC handle;
        // the spec here carries a raw pointer (no reference taken, none dropped),
        // and the ctor's own retain of the RWAC pointer stands on its own.
        {
            AemsFactorySpec lAemsSpec;
            lAemsSpec.mpRwacFactory       = mhRwacFactory.GetObject();
            lAemsSpec.mu32EntityCount     = 128;
            lAemsSpec.mu32DataSize        = 32384;
            lAemsSpec.mu32StringTableSize = 0;
            mhAemsFactory = AemsFactory::Create(*mhEnvironment, lAemsSpec);
            CGS_ASSERT(!!mhAemsFactory, "mhAemsFactory");
        }
        // [3/3] SPLICER -- REAL (SplicerFactory::Create @0x826DB130, cascade
        // slice 3): spec {the RWAC handle, 128, 32384, 0}; the temp assigned
        // into +0x2264 and asserted (cpp:219). The console then releases the
        // temp and the spec's RWAC handle (the [1/3]-[2/3] precedent), and finally
        // PUBLISHES the module's IStreamProvider sub-object (+0x228) to the
        // off_82FFBA0C interface global -- the SndPlayer1_CgsStreamMod side's
        // stream-provider hook.
        {
            SplicerFactorySpec lSplicerSpec;
            lSplicerSpec.mpRwacFactory       = mhRwacFactory.GetObject();
            lSplicerSpec.mu32EntityCount     = 128;
            lSplicerSpec.mu32DataSize        = 32384;
            lSplicerSpec.mu32StringTableSize = 0;
            mhSplicerFactory = SplicerFactory::Create(*mhEnvironment, lSplicerSpec);
            CGS_ASSERT(!!mhSplicerFactory, "mhSplicerFactory");

            // DWARF: SndPlayer1_CgsStreamMod::SetStreamProvider inlined here (BrnSoundUnity);
            // the store to spStreamProvider (X360 off_82FFBA0C) of `&this->+0x228`.
            rw::audio::core::SndPlayer1_CgsStreamMod::SetStreamProvider(static_cast<IStreamProvider*>(this));
        }

        // The stream-buffer carve (real): LinearMalloc-backed when supplied
        // (size = GetSize()/3, 2 blocks, main-allocator flag OFF -> Malloc), else
        // 0x20000-byte buffers, 4 blocks, flag ON -> AllocateMemoryResource.
        u32 lu32BufferSize;
        if (apLinearMalloc)
        {
            apLinearMalloc->SetAlignment(16);
            lu32BufferSize = static_cast<u32>(apLinearMalloc->GetSize() / 3u);
            muStreamNumBlocks           = 2;
            mbStreamsUsingMainAllocator = false;
        }
        else
        {
            muStreamNumBlocks           = 4;
            lu32BufferSize              = 0x20000;
            mbStreamsUsingMainAllocator = true;
        }
        muStreamBufferSize = lu32BufferSize;

        for (u32 lu = 0; lu < 3; ++lu)
        {
            void* lpBuffer;
            if (mbStreamsUsingMainAllocator)
                lpBuffer = apAllocator->AllocateMemoryResource(muStreamBufferSize, 16,
                                                  "SndPlayer1_CgsStreamMod Stream Buffer");
            else
                lpBuffer = apLinearMalloc->Malloc(muStreamBufferSize);
            CGS_ASSERT(lpBuffer != 0, "lpBuffer");
            maStreamBuffers[lu].mpBuffer      = lpBuffer;
            maStreamBuffers[lu].mBufferStatus = StreamBuffer::E_FREE_BUFFER;
        }

        // The 8 environment CPU monitors, BY NAME through the env's CpuMonitors
        // (the console v27[2/4/6/5/8/9/10/7] word stores == env+8 .. -- the
        // CpuMonitors sub-object's fields in declaration order).
        CGS_ASSERT(mhEnvironment.GetObject(), "mpObject");
        CpuMonitors& lrMonitors = mhEnvironment.GetObject()->GetCpuMonitors();
        {
            using CgsDev::PerfMonCpuPage;

            lrMonitors.miModule = CgsDev::PerfMonCpu::AddMonitor(
                "Playback", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miEnvironmentUpdate = CgsDev::PerfMonCpu::AddMonitor(
                " Environment", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miAemsFactoryUpdate = CgsDev::PerfMonCpu::AddMonitor(
                "  Aems factory", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miRwacFactoryUpdate = CgsDev::PerfMonCpu::AddMonitor(
                "  Rwac factory", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miSplicerFactoryUpdate = CgsDev::PerfMonCpu::AddMonitor(
                "  Splicer factory", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miContentUpdate = CgsDev::PerfMonCpu::AddMonitor(
                "  Content update", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miVoiceUpdate = CgsDev::PerfMonCpu::AddMonitor(
                "  Voice update", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miAemsFactoryUpdate2 = CgsDev::PerfMonCpu::AddMonitor(
                "Aems Update", static_cast<PerfMonCpuPage>(19), false, 1.0f, true);
        }

        mePrepareStage++;   // the raw bump with the .h:500 bound assert (inlined op++)
        meReleaseStage = E_RELEASESTAGE_FACTORIES;
        // fall through
    }
    case E_PREPARESTAGE_DONE:
        meReleaseStage = E_RELEASESTAGE_START;
        return true;
    default:
        CGS_FAIL("Invalid Prepare Stage");
        return false;
    }
}

// ---------------------------------------------------------------------------
// Module::Release  @ 0x826C0FB8  (the release COUNTDOWN machine, run forward
// from meReleaseStage; each rung bumps the cursor via the @0x82681CD0 operator)
// ---------------------------------------------------------------------------
bool Module::Release()
{
    rw::IResourceAllocator* lpMainAllocator = 0;

    switch (meReleaseStage)
    {
    case 0:
        meReleaseStage++;
        // fall through
    case 1:
        // Free the string-table chain through the environment allocator's DoFree
        // (each chunk wrapped as a {ptr, 0 x4} resource -- the wave-3 dispose idiom).
        while (mpStringTable)
        {
            StringTable* lpNext = mpStringTable->mpNext;
            Environment* lpEnvironment = &GetEnvironment();
            rw::Resource lResource;
            lResource.m_baseResources[0] = mpStringTable;
            for (u32 lu = 1; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
                lResource.m_baseResources[lu] = 0;
            lpEnvironment->GetAllocator()->DoFree(lResource);
            mpStringTable = lpNext;
        }
        meReleaseStage++;
        // fall through
    case 2:
        // The AEMS handle releases through the sample-player factory interface.
        mhRwacFactory    = Handle<Factory>();
        mhAemsFactory    = Handle<AemsFactory>();
        mhSplicerFactory = Handle<Factory>();
        meReleaseStage++;
        // fall through
    case 3:
        // Snapshot the main allocator BEFORE dropping the environment handle (the
        // stream-buffer frees below need it once the environment is gone).
        lpMainAllocator = GetEnvironment().GetAllocator();
        mhEnvironment = Handle<Environment>();
        meReleaseStage++;
        // fall through
    case 4:
        for (u32 lu = 0; lu < 3; ++lu)
        {
            void* lpBuffer = maStreamBuffers[lu].mpBuffer;
            maStreamBuffers[lu].mReadStream = static_cast<CgsFileSystem::StreamDeviceDiskRead*>(0);   // the console +8 word zero
            maStreamBuffers[lu].mpBuffer    = 0;
            if (mbStreamsUsingMainAllocator)
            {
                CGS_ASSERT(lpMainAllocator != 0, "lpMainAllocator");
                rw::Resource lResource;
                lResource.m_baseResources[0] = lpBuffer;
                for (u32 luI = 1; luI < rw::KU_RESOURCE_LANE_COUNT; ++luI)
                    lResource.m_baseResources[luI] = 0;
                lpMainAllocator->DoFree(lResource);
            }
        }
        meReleaseStage++;   // the inlined @0x82681CD0 bump (bound assert inside)
        // fall through
    case 5:
        if (!mDeferredResourceRequestQueue.Release())
            return false;
        mResourceReceiverQueue.Clear();
        if (!CgsModule::ModuleSingleBuffered::Release())
            return false;
        meReleaseStage++;
        // fall through
    case E_RELEASESTAGE_DONE:
        mePrepareStage = E_PREPARESTAGE_START;
        return true;
    default:
        CGS_FAIL("Invalid Release Stage");
        return false;
    }
}

// ---------------------------------------------------------------------------
// Module::Destruct  @ 0x826C1268
// ---------------------------------------------------------------------------
void Module::Destruct()
{
    mDeferredResourceRequestQueue.Destruct();
    mResourceReceiverQueue.Clear();
    CgsModule::ModuleSingleBuffered::Destruct();
}

// ---------------------------------------------------------------------------
// Module::GetEnvironment  @ 0x82694130
// Asserts the environment handle is non-null (CgsHandle.h:305 -> "mpObject") then
// returns the owned Environment pointer.
// ---------------------------------------------------------------------------
Environment& Module::GetEnvironment()
{
    // DWARF: Environment& GetEnvironment(); the assert is Handle::operator*'s (CgsHandle.h:305).
    return *mhEnvironment;
}

// ---------------------------------------------------------------------------
// Module::DumpRegistries  @ 0x82694188
// Dumps the registries owned by the environment, the RWAC factory and the AEMS
// factory in turn. Each owning object is asserted non-null ("mpObject") and each
// registry pointer is asserted non-null ("mpRegistry") before the dump.
//
// X360 member reads: a1[2198] = mhEnvironment (env, registry @ env+0x4C),
// a1[2199] = mhRwacFactory (factory, registry @ +0x401C),
// a1[2200] = mhAemsFactory (factory, registry @ +0x60).
// ---------------------------------------------------------------------------
void Module::DumpRegistries()
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");
    Registry* lpRegistry = lpEnvironment->GetRegistry();
    CGS_ASSERT(lpRegistry, "mpRegistry");
    lpRegistry->Dump();

    Factory* lpRwacFactory = mhRwacFactory.GetObject();
    CGS_ASSERT(lpRwacFactory, "mpObject");
    GetRwacFactoryRegistry(lpRwacFactory)->Dump();

    AemsFactory* lpAemsFactory = mhAemsFactory.GetObject();
    CGS_ASSERT(lpAemsFactory, "mpObject");
    lpAemsFactory->GetRegistry().Dump();
}

// ---------------------------------------------------------------------------
// Module::ImportStringTable  @ 0x826AD6B0
//
// Imports a CSIS string-table resource: if the resource has a populated string
// region, allocate an environment-owned chunk of (size + 4) bytes, push it onto the
// mpStringTableHead list, copy the string bytes in, then walk every NUL-terminated
// string in the copied region and intern it via Name::MakeHash.
//
// The resource layout the asm reads (a2 = lpStringTableResource):
//   a2[4]            (+0x10) -- "has string region" flag (the outer `if`)
//   a2[5]            (+0x14) -- end pointer of the string region (v4)
//   a2[1], a2[2]     (+0x04, +0x08) -- count and byte-offset used to compute the
//                      start pointer:  start = &a2[a2[1] + 7] + a2[2]
//                      i.e. base = (const char*)a2 + (a2[1] + 7) * 4 + a2[2]
//   size = end - start  (v6, the byte length to copy)
// ---------------------------------------------------------------------------
void Module::ImportStringTable(const Registry& arRegistry)
{
    u32 lu32StringTableSize =
        static_cast<u32>(arRegistry.GetStringTableUsedSize());
    if (lu32StringTableSize == 0)
        return;

    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    Module::StringTable* lpChunk = static_cast<Module::StringTable*>(
        lpEnvironment->Allocate(
            lu32StringTableSize + static_cast<u32>(offsetof(Module::StringTable, macData)),
            4, "StringTable"));
    if (!lpChunk)
        return;

    // Push the new chunk onto the head of the module's string-table list.
    lpChunk->mpNext = mpStringTable;
    mpStringTable   = lpChunk;

    std::memcpy(lpChunk->macData, arRegistry.GetStringTableData(),
                lu32StringTableSize);

    // Intern every NUL-terminated string in the copied region.
    char* lpcCursor = lpChunk->macData;
    do
    {
        char* lpcScan = lpcCursor;
        while (*lpcScan++)
            ;
        u32 lu32StringLen = static_cast<u32>(lpcScan - lpcCursor);
        CGS_ASSERT(lu32StringTableSize >= lu32StringLen,
                   "luStringTableSize >= luStringLen");
        Name::MakeHash(lpcCursor);
        lu32StringTableSize -= lu32StringLen;
        lpcCursor           += lu32StringLen;
    } while (lu32StringTableSize);
}

void Module::AddRegistry(Registry& arRegistry, u32 au32RegistryId)
{

    arRegistry.FixUp();
    arRegistry.Resolve(arRegistry);
    ImportStringTable(arRegistry);

    switch (au32RegistryId)
    {
    case 1:
    {
        Registry* lpRegistry = GetEnvironment().GetRegistry();
        CGS_ASSERT(lpRegistry != 0, "mpRegistry");
        *lpRegistry += arRegistry;
        break;
    }
    case 2:
    {
        AemsFactory* lpFactory = mhAemsFactory.GetObject();
        CGS_ASSERT(lpFactory != 0, "mpObject");
        lpFactory->GetRegistry() += arRegistry;   // AemsFactory::GetRegistry asserts mpRegistry (h:422)
        break;
    }
    case 3:
    {
        GenericRwacFactory* lpFactory =
            static_cast<GenericRwacFactory*>(mhRwacFactory.GetObject());
        CGS_ASSERT(lpFactory != 0, "mpObject");
        lpFactory->AddRegistry(arRegistry);
        break;
    }
    default:
        CGS_FAIL("");
        break;
    }
}

// ---------------------------------------------------------------------------
// Module::AttachVoice  @ 0x826D7D80  (DWARF h:345)
// Attaches lphContent to the named slot lu32SlotName on lphVoice (the real
// Voice::Attach(Name, Handle<Content>&) -- the u32 the asm builds on the stack and
// passes by reference IS the interned slot Name), then releases the transient
// references held by both handles (the X360 drops a ref on each owned object).
// ---------------------------------------------------------------------------
void Module::AttachVoice(Handle<Voice> lhVoice, Handle<Content> lhContent, u32 lu32SlotName)
{
    CGS_ASSERT_AT(lhVoice, "lhVoice", CGS_ARTIST_PLAYBACK_MODULE_CPP, 1128);
    CGS_ASSERT_AT(lhContent, "lhContent", CGS_ARTIST_PLAYBACK_MODULE_CPP, 1129);

    const Name lSlotName(static_cast<uintptr_t>(lu32SlotName));
    const bool lbAttached = lhVoice->Attach(lSlotName, lhContent);
    CGS_ASSERT_AT(lbAttached, "lhVoice->Attach(lSlotName, lhContent)", CGS_ARTIST_PLAYBACK_MODULE_CPP, 1130);
    // The by-value handles release their references as they are destroyed
    // (ARTIST 0x826D7E58..0x826D7E74).
}

// ---------------------------------------------------------------------------
// Module::ConnectVoice  @ 0x826C14E0
// Resolves the submix voice for lu32SubmixId out of the environment, takes a
// reference on it, connects lphVoice's send lu32SendName to it, then releases the
// transient references.
// ---------------------------------------------------------------------------
void Module::ConnectVoice(Handle<Voice> lhVoice, u32 lu32SendName, u32 lu32SubmixId)
{
    // GetVoice (the IDA-truncated `GetV`, DWARF h:289) returns a temporary
    // Handle<Voice> (ARTIST var_80); its object is copied into the SubmixVoice handle
    // through the CgsHandle.h:239 view (one reference, 0x826C1548), then the temporary
    // is destroyed (0x826C155C).
    Handle<SubmixVoice> lhSubmixVoice = GetEnvironment().GetVoice(lu32SubmixId);

    CGS_ASSERT_AT(lhVoice, "lhVoice", CGS_ARTIST_PLAYBACK_MODULE_CPP, 1151);
    CGS_ASSERT_STREAM_AT(lhSubmixVoice, "Submix ID: " << lu32SubmixId,
                         CGS_ARTIST_PLAYBACK_MODULE_CPP, 1152);

    // The real Voice::Connect(Name, Handle<SubmixVoice>&) @0x826ACC90.
    Name lSendName(static_cast<uintptr_t>(lu32SendName));
    const bool lbConnected = lhVoice->Connect(lSendName, lhSubmixVoice);
    CGS_ASSERT_AT(lbConnected, "lhVoice->Connect(lSendName, lhSubmixVoice)",
                  CGS_ARTIST_PLAYBACK_MODULE_CPP, 1153);
}

// ---------------------------------------------------------------------------
// Module::CreateVoice  @ 0x826D7B00  (the IDA-truncated "Module::C"; DWARF h:318,
// cpp:1022 `Handle<Voice> CreateVoice(uint32_t, uint32_t, uint32_t)`)
//
// Resolve the owning factory (by name) and the voice spec (by name) out of the
// environment, ask the factory to create the voice for the requested ident, stamp
// the ident, hand the reserved init-submix ident's voice to the Snd9 hack, and
// return the voice. Any failure streams the spec name into the assert and returns
// an empty handle. The handles' constructors and destructors carry every reference
// (ARTIST: the out-of-line Handle<Factory>/<Voice>::operator-> at 0x826D7BF4 /
// 0x826D7C14, the copy into HACK_SetSnd9InitSubmix's by-value parameter at
// 0x826D7C54, the returned copy at 0x826D7C78, then lhVoice's and lhFactory's
// releases).
// ---------------------------------------------------------------------------
Handle<Voice> Module::CreateVoice(u32 lu32Ident, u32 lu32FactoryName, u32 lu32VoiceSpecName)
{
    Handle<Factory> lhFactory = GetEnvironment().GetFactory(Name(lu32FactoryName));
    if (lhFactory)
    {
        Name lVoiceSpecName(lu32VoiceSpecName);
        const VoiceSpec* lpVoiceSpec =
            GetEnvironment().GetRegistry()->GetEntity<VoiceSpec>(lVoiceSpecName);
        if (lpVoiceSpec)
        {
            Handle<Voice> lhVoice;
            if (lhFactory->CreateVoice<Voice>(*lpVoiceSpec, lhVoice, lu32Ident) !=
                static_cast<u32>(-1))
            {
                // The `*(voice+12) = ident` store: Voice::SetIdent (DWARF CgsVoice.h:567).
                lhVoice->SetIdent(lu32Ident);

                if (lu32Ident == KU_INIT_SND9_SUBMIX_IDENT)
                {
                    CGS_ASSERT_AT(GenericRwacFactory::SK_NAME == Name(lu32FactoryName),
                                  "GenericRwacFactory::SK_NAME == lFactoryName",
                                  CGS_ARTIST_PLAYBACK_MODULE_CPP, 1051);
                    ::HACK_SetSnd9InitSubmix(lhVoice);
                }
                return lhVoice;
            }
        }
    }

    CGS_FAIL_STREAM_AT("E_COMMAND_VOICE_CREATE failed: VoiceSpec"
                           << Name(lu32VoiceSpecName) << "\n",
                       CGS_ARTIST_PLAYBACK_MODULE_CPP, 1063);
    return Handle<Voice>();
}

// ---------------------------------------------------------------------------
// Module::CreateContent  @ 0x826C12A8  (DWARF h:327, cpp:1075 `Handle<Content>
// CreateContent(uint32_t, uint32_t, uint32_t)`)
//
// The content mirror of CreateVoice: resolve the owning factory (by class name) and
// the ContentSpec (by name), ask the factory to create the content, wire the
// created content's ident (+0x10 mIdent) and owner interface (+0x14 mpLoadService,
// the IContentLoadService base at this+0x22C when non-null), and return it.
// ---------------------------------------------------------------------------
Handle<Content> Module::CreateContent(u32 lu32Ident, u32 lu32ContentClassName,
                                      u32 lu32ContentSpecName)
{
    Handle<Factory> lhFactory = GetEnvironment().GetFactory(Name(lu32ContentClassName));
    if (lhFactory)
    {
        Name lContentSpecName(lu32ContentSpecName);
        const ContentSpec* lpContentSpec =
            GetEnvironment().GetRegistry()->GetEntity<ContentSpec>(lContentSpecName);
        if (lpContentSpec)
        {
            Handle<Content> lhContent;
            if (lhFactory->CreateContent(*lpContentSpec, lhContent, lu32Ident) !=
                static_cast<u32>(-1))
            {
                lhContent->mIdent = lu32Ident;                                       // Content +0x10
                lhContent->SetLoadService(static_cast<IContentLoadService*>(this));  // Content +0x14
                return lhContent;
            }
        }
    }

    CGS_FAIL_STREAM_AT("E_COMMAND_CONTENT_CREATE failed with content spec "
                           << Name(lu32ContentSpecName) << "\n",
                       CGS_ARTIST_PLAYBACK_MODULE_CPP, 1108);
    return Handle<Content>();
}

namespace
{
    // Access adapter for the type-4 receiver arm: the console's bare
    // `bl BaseResourcePtr::CreateFromHandle` there is the inlined ResourcePtr
    // rebind (the AddListResource family emits the identical two-instruction
    // shape); CreateFromHandle is protected on the committed type, so the rebind
    // is expressed through this data-free derived adapter rather than loosening
    // the committed access.
    struct ResourcePtrBinder : public CgsResource::BaseResourcePtr
    {
        static void Bind(CgsResource::BaseResourcePtr* lpTarget,
                         const CgsResource::ResourceHandle* lpHandle)
        {
            static_cast<ResourcePtrBinder*>(lpTarget)->CreateFromHandle(lpHandle);
        }
    };

}

// ---------------------------------------------------------------------------
// Module::Update  @ 0x826E9700  (bodied 2026-08-25, faithful-audio-engine
// phase B4 -- the per-frame pump)
//
// Console offsets, all closed by name: env handle +0x2258 (each read re-guarded
// through the CgsHandle.h:305 assert == GetEnvironment()), the env's
// miModule/miEnvironmentUpdate monitors (env+8/+16), buffers +0x2250/+0x2254,
// the output queue Clear + freed-count zero (== OutputBuffer::Clear), the
// deferred-queue drain (Append<1024,16> into the 4096 queue) + stream tick
// under mStreamMutex, the receiver drain, then Environment::Update(+0x2700
// mf32TimeStep).
// ---------------------------------------------------------------------------
void Module::Update(Io::InputBuffer* apInputBuffer, Io::OutputBuffer* apOutputBuffer)
{
    CgsDev::PerfMonCpu::StartMonitor(GetEnvironment().GetCpuMonitors().miModule);

    mpInputBuffer  = apInputBuffer;
    mpOutputBuffer = apOutputBuffer;
    apInputBuffer->LockForRead();
    apOutputBuffer->LockForWrite();

    apOutputBuffer->Clear();   // queue Clear + freed-array count zero (the console inline)

    mStreamMutex.Lock();
    apOutputBuffer->GetResourceRequestQueue().Append(mDeferredResourceRequestQueue);
    mDeferredResourceRequestQueue.Clear();
    UpdateStreamBuffers(apOutputBuffer->GetStreamBuffersFreed());
    mStreamMutex.Unlock();

    apInputBuffer->UnlockForRead();

    ProcessResourceReceiverQueue();

    CgsDev::PerfMonCpu::StartMonitor(GetEnvironment().GetCpuMonitors().miEnvironmentUpdate);
    GetEnvironment().Update(mf32TimeStep);
    CgsDev::PerfMonCpu::StopMonitor(GetEnvironment().GetCpuMonitors().miEnvironmentUpdate);

    apOutputBuffer->UnlockForWrite();
    mpInputBuffer  = 0;
    mpOutputBuffer = 0;

    CgsDev::PerfMonCpu::StopMonitor(GetEnvironment().GetCpuMonitors().miModule);
}

// ---------------------------------------------------------------------------
// Module::ProcessResourceReceiverQueue  @ 0x826A25C0  (bodied phase B4)
//
// Drain mResourceReceiverQueue (see the header note per arm), then Clear it.
// The stream arms run under the RWAC system lock + the stream mutex, exactly
// bracketing the console's Lock/Unlock pairs.
// ---------------------------------------------------------------------------
void Module::ProcessResourceReceiverQueue()
{
    if (mResourceReceiverQueue.GetCount() > 0)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = mResourceReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            switch (liType)
            {
            case 4:   // content-load response
            {
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);

                // The console stores lpUserData in PoolEvent::miEventId, a 32-bit word.
                // The PC resource/content heaps are intentionally carved below 4 GB, so
                // reconstruct the same pointer from that echoed word after the producer's
                // range check in DoServiceContentLoadRequest.
                CgsResource::BaseResourcePtr* lpTarget =
                    reinterpret_cast<CgsResource::BaseResourcePtr*>(
                        static_cast<uintptr_t>(static_cast<u32>(lpResponse->miEventId)));
                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry = lpResponse->mpSourceEntry;
                CGS_ASSERT(lpTarget != 0, "lpResponse->GetUserData()");
                ResourcePtrBinder::Bind(lpTarget, &lHandle);
                break;
            }
            case 16:  // stream-open response
            {
                const CgsResource::Events::ReadStreamEvent* lpResponse =
                    reinterpret_cast<const CgsResource::Events::ReadStreamEvent*>(lpEvent);

                rw::audio::core::System* lpSystem = GetDefaultRwacSystem();
                CGS_ASSERT(lpSystem != 0, "mpSystem");
                rw::audio::core::RwacSystemLock(lpSystem);
                mStreamMutex.Lock();

                const u32 luIndex = static_cast<u32>(lpResponse->GetEventId());
                CGS_ASSERT(luIndex < SKU_NUMBER_OF_STREAM_BUFFERS,
                           "( liIndex >= 0 ) && ( liIndex < SKU_NUMBER_OF_STREAM_BUFFERS )");
                StreamBuffer& lrBuffer = maStreamBuffers[luIndex];
                CGS_ASSERT(lrBuffer.GetStatus() == StreamBuffer::E_USING_BUFFER,
                           "E_USING_BUFFER == GetStatus()");
                lrBuffer.SetReadStream(lpResponse->GetStream());
                lrBuffer.SetStatus(StreamBuffer::E_STREAM_OPEN);

                mStreamMutex.Unlock();
                rw::audio::core::RwacSystemUnlock(lpSystem);
                break;
            }
            case 18:  // stream-close response
            {
                const CgsResource::Events::ReadStreamEvent* lpResponse =
                    reinterpret_cast<const CgsResource::Events::ReadStreamEvent*>(lpEvent);

                rw::audio::core::System* lpSystem = GetDefaultRwacSystem();
                CGS_ASSERT(lpSystem != 0, "mpSystem");
                rw::audio::core::RwacSystemLock(lpSystem);
                mStreamMutex.Lock();

                const u32 luIndex = static_cast<u32>(lpResponse->GetEventId());
                CGS_ASSERT(luIndex < SKU_NUMBER_OF_STREAM_BUFFERS,
                           "( lIndex >= 0 ) && ( lIndex < SKU_NUMBER_OF_STREAM_BUFFERS )");
                StreamBuffer& lrBuffer = maStreamBuffers[luIndex];
                CGS_ASSERT(lrBuffer.GetStatus() == StreamBuffer::E_WAITING_FOR_CLOSE,
                           "StreamBuffer::E_WAITING_FOR_CLOSE == mStreamBuffers[ lIndex ].GetStatus()");
                std::memset(lrBuffer.mpBuffer, 0xF0, muStreamBufferSize);   // the scrub pattern
                lrBuffer.SetStatus(StreamBuffer::E_WAITING_GRACE_PERIOD);

                mStreamMutex.Unlock();
                rw::audio::core::RwacSystemUnlock(lpSystem);
                break;
            }
            default:
                // The console streams the message then fires; kept as a static
                // string (the typo is the X360's own).
                CGS_FAIL("Unanticiapated Resource Response. Eek.\n");
                break;
            }

            const CgsModule::Event* lpNext = 0;
            liType = mResourceReceiverQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    mResourceReceiverQueue.Clear();
}

// ---------------------------------------------------------------------------
// Module::UpdateStreamBuffers  @ 0x826A28E8  (bodied phase B4; caller holds
// mStreamMutex)
//
// Per record: a queued close on a now-open stream re-dispatches the virtual
// DoCloseStream (the console `vtbl+4` on the +0x228 IStreamProvider sub-object);
// an E_WAITING_GRACE_PERIOD record accumulates mf32TimeStep, stomp-scans its
// 0xF0-scrubbed buffer (assert "STOMP OCCURRED" -- the console streams the
// time/index/address; kept static), and after the first tick appends its voice
// id to the freed list and resets to E_FREE_BUFFER (mpBuffer kept).
// ---------------------------------------------------------------------------
void Module::UpdateStreamBuffers(Io::OutputBuffer::FreedBuffersArray& arFreedBuffers)
{
    for (u32 lu = 0; lu < SKU_NUMBER_OF_STREAM_BUFFERS; ++lu)
    {
        StreamBuffer& lrBuffer = maStreamBuffers[lu];

        if (lrBuffer.GetQueuedForClose() &&
            lrBuffer.GetStatus() == StreamBuffer::E_STREAM_OPEN)
        {
            DoCloseStream(&lrBuffer.GetReadStream());
        }

        if (lrBuffer.GetStatus() == StreamBuffer::E_WAITING_GRACE_PERIOD)
        {
            lrBuffer.mfGraceWaitTime += mf32TimeStep;

            bool lbStomped = false;
            const u8* lpBytes = static_cast<const u8*>(lrBuffer.mpBuffer);
            for (u32 luByte = 0; luByte < muStreamBufferSize; ++luByte)
            {
                if (lpBytes[luByte] != 0xF0u)
                    lbStomped = true;
            }
            CGS_ASSERT(!lbStomped, "STOMP OCCURRED");

            if (lrBuffer.mfGraceWaitTime > 0.0f)
            {
                arFreedBuffers.Append(lrBuffer.mVoiceId);
                lrBuffer.mfGraceWaitTime  = 0.0f;
                lrBuffer.mReadStream      = static_cast<CgsFileSystem::StreamDeviceDiskRead*>(0);
                lrBuffer.mVoiceId         = 0;
                lrBuffer.mbQueuedForClose = false;
                lrBuffer.mBufferStatus    = StreamBuffer::E_FREE_BUFFER;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Module::FindStreamBuffer  @ 0x82689CE0  (bodied phase B4)
//   index = (lpReadStream - &maStreamBuffers[0].mReadStream) / record; -1 when
//   the pointer is not one of the 3 records' mReadStream members.
// ---------------------------------------------------------------------------
s32 Module::FindStreamBuffer(const CgsFileSystem::ReadStream* lpReadStream)
{
    // ARTIST assertion at 0x82689D0C.
    CGS_ASSERT_AT(lpReadStream != 0,
        "0 != lpReadStream",
        "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Module/CgsSoundPlaybackModule.cpp", 1426);

    for (s32 li = 0; li < static_cast<s32>(SKU_NUMBER_OF_STREAM_BUFFERS); ++li)
    {
        if (&maStreamBuffers[li].mReadStream == lpReadStream)  // FAKEMATCH: ARTIST compares the member first.
            return li;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Module::DoServiceContentLoadRequest  @ 0x826F9F88  (the IContentLoadService
// override; bodied phase B4)
//
// RESOURCE_MODULE loads only: assert the user data (cpp:1171), hash the content
// name, and post the AcquireResourceRequest {&mResourceReceiverQueue, lpUserData
// in the event-id word, mi32PoolId, hash64} as event type 4 into the attached
// output buffer's request queue. The console widens the 32-bit hash into the
// trailing 64-bit id slot.
// ---------------------------------------------------------------------------
bool Module::DoServiceContentLoadRequest(u32 lu32Ident, EContentLoadMethod leMethod,
                                         const char* lpcName, void* lpUserData)
{
    (void)lu32Ident;   // carried by the request protocol's event id on the console side

    if (leMethod != E_CONTENT_LOAD_RESOURCE_MODULE)
        return false;

    CGS_ASSERT(lpUserData != 0, "lpUserData");

    const s32 liHash = CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lpcName));

    const uintptr_t luUserData = reinterpret_cast<uintptr_t>(lpUserData);
    CGS_ASSERT(luUserData <= 0xFFFFFFFFull,
               "content loader user data must live in the low-address arena");
    if (luUserData > 0xFFFFFFFFull)
        return false;

    CgsResource::Events::AcquireResourceRequest lRequest;
    lRequest.mpUser = &mResourceReceiverQueue;
    lRequest.miEventId = static_cast<s32>(static_cast<u32>(luUserData));
    lRequest.miPoolId = mi32PoolId;
    lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(liHash)));

    CGS_ASSERT(mpOutputBuffer != 0, "mpOutputBuffer");
    return mpOutputBuffer->GetResourceRequestQueue().AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 4, sizeof(lRequest));
}

// ---------------------------------------------------------------------------
// Module::DoOpenStream  @ 0x826FA020  (the IStreamProvider override; bodied
// phase B4 -- see the header note for the full contract)
// ---------------------------------------------------------------------------
CgsFileSystem::ReadStream* Module::DoOpenStream(IStreamProvider::StreamSpec& lrSpec)
{
    // DWARF BrnSoundUnity locals: lRequest (cpp:1202), lAutoMutex (cpp:1203, the scoped
    // stream lock), liBufferToUse / liCount, lHandle (cpp:1226).
    CgsResource::Events::OpenReadStreamRequest lRequest;
    EA::Thread::AutoMutex lAutoMutex(mStreamMutex);

    // First free record.
    s32 liIndex = -1;
    for (u32 lu = 0; lu < SKU_NUMBER_OF_STREAM_BUFFERS; ++lu)
    {
        if (maStreamBuffers[lu].GetStatus() == StreamBuffer::E_FREE_BUFFER)
        {
            liIndex = static_cast<s32>(lu);
            break;
        }
    }
    if (liIndex < 0)
    {
        CGS_FAIL("We've run out of Audio Stream Buffers.");
        return 0;
    }

    // Resolve the requesting plug-in's owning voice out of the environment.
    Handle<Voice> lHandle =
        GetEnvironment().GetRwacVoiceByPlugin(lrSpec.mpPlugin);
    CGS_ASSERT(lHandle.GetObject() != 0, "lHandle");

    // Seed the record and hand the carve buffer back through the spec.
    StreamBuffer& lrBuffer = maStreamBuffers[liIndex];
    lrBuffer.mfGraceWaitTime  = 0.0f;
    lrBuffer.mReadStream      = static_cast<CgsFileSystem::StreamDeviceDiskRead*>(0);
    lrBuffer.mBufferStatus    = StreamBuffer::E_USING_BUFFER;
    lrBuffer.mVoiceId         = lHandle.GetObject()->GetIdent();
    lrBuffer.mbQueuedForClose = false;
    *lrSpec.mppvBuffer = lrBuffer.mpBuffer;

    // Build + post the open request (event type 16) into the deferred queue.
    lRequest.Construct(&mResourceReceiverQueue, liIndex);
    lRequest.SetFileName(lrSpec.mpFilename);
    lRequest.SetBuffer(*lrSpec.mppvBuffer);
    lRequest.SetBufferSize(muStreamBufferSize);
    lRequest.SetNumBlocks(muStreamNumBlocks);
    lRequest.SetNormalPriority(lrSpec.mi32PriorityLow);
    lRequest.SetHighPriority(lrSpec.mi32PriorityHigh);
    lRequest.SetUseHDCache(false);

    if (!mDeferredResourceRequestQueue.AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 16, sizeof(lRequest)))
    {
        CGS_FAIL("mDeferredResourceRequestQueue.OpenReadStream( lRequest )");
    }

    // lHandle releases the voice, then lAutoMutex unlocks (ARTIST 0x826FA250..0x826FA2A8).
    return &lrBuffer.mReadStream;
}

// ---------------------------------------------------------------------------
// Module::DoCloseStream  @ 0x826FA2B8  (the IStreamProvider override; bodied
// phase B4 -- see the header note for the full contract)
// ---------------------------------------------------------------------------
void Module::DoCloseStream(const CgsFileSystem::ReadStream* lpReadStream)
{
    mStreamMutex.Lock();

    const s32 liIndex = FindStreamBuffer(lpReadStream);
    CGS_ASSERT(liIndex != -1, "Can't Find this Stream Index");

    StreamBuffer& lrBuffer = maStreamBuffers[liIndex];
    if (lrBuffer.GetStatus() == StreamBuffer::E_STREAM_OPEN)
    {
        // Post the close request (event type 18) into the deferred queue, then
        // advance the record to E_WAITING_FOR_CLOSE.
        CgsResource::Events::CloseReadStreamRequest lRequest;
        lRequest.Construct(&mResourceReceiverQueue, liIndex, lrBuffer.GetReadStream());

        if (!mDeferredResourceRequestQueue.AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest), 18, sizeof(lRequest)))
        {
            CGS_FAIL("mDeferredResourceRequestQueue.CloseReadStream( lRequest )");
        }

        lrBuffer.SetStatus(StreamBuffer::E_WAITING_FOR_CLOSE);
    }
    else
    {
        // Not open yet: it must still be opening -- queue the close for
        // UpdateStreamBuffers to re-dispatch once the open response lands.
        CGS_ASSERT(lrBuffer.GetStatus() == StreamBuffer::E_USING_BUFFER,
                   "StreamBuffer::E_USING_BUFFER == mStreamBuffers[ lIndex ].GetStatus()");
        CGS_ASSERT(!lrBuffer.GetQueuedForClose(), "!mbQueuedForClose");
        lrBuffer.SetQueuedForClose();
    }

    mStreamMutex.Unlock();
}

// ---------------------------------------------------------------------------
// operator++(Module::EPrepareStage&, int)  @ 0x82681C70
//   old=*a1; new=old+1; *a1=new (stored UNCONDITIONALLY, before the guard);
//   assert(new <= E_PREPARESTAGE_DONE); return old. The increment applies even on the
//   assert path; the return is always the saved OLD stage.
// ---------------------------------------------------------------------------
Module::EPrepareStage operator++(Module::EPrepareStage& leEnumIndex, int)
{
    const Module::EPrepareStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<Module::EPrepareStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT_AT(leEnumIndex <= Module::E_PREPARESTAGE_DONE,
        "leEnumIndex <= Module::E_PREPARESTAGE_DONE",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Module/CgsSoundPlaybackModule.h", 500 /* ARTIST 0x82681C9C */);


    return leOldEnumIndex;
}

// ---------------------------------------------------------------------------
// operator++(Module::EReleaseStage&, int)  @ 0x82681CD0
//   Same shape as the prepare sibling: store the incremented cursor
//   UNCONDITIONALLY, assert the bound (CgsSoundPlaybackModule.h:501,
//   "leEnumIndex <= Module::E_RELEASESTAGE_DONE" == 6), return the saved OLD
//   value. Release's case-4 rung inlines this exact sequence.
// ---------------------------------------------------------------------------
Module::EReleaseStage operator++(Module::EReleaseStage& leEnumIndex, int)
{
    const Module::EReleaseStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<Module::EReleaseStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT(leEnumIndex <= Module::E_RELEASESTAGE_DONE,
               "leEnumIndex <= Module::E_RELEASESTAGE_DONE");

    return leOldEnumIndex;
}

} // namespace Module
} // namespace Playback
} // namespace CgsSound
