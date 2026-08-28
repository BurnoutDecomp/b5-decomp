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

#include "rw/rwcore_structs.h"   // rw::Resource / rw::IResourceAllocator (the Release-machine DoFree wraps)

#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                 // Prepare's stream-buffer bump path
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // the 8 environment CPU monitors
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GetDefaultRwacSystem + RwacSystemLock (phase B4)
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"        // AemsFactory::Create (the stage-3 create, cascade slice 2)
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
    // The console's `rw::IResourceAllocator::AllocateMemoryResource` @0x823FF7D0 is an
    // INLINE that builds a five-entry serialised descriptor and tail-calls the
    // allocator's DoAllocate slot; the PC rwcore models the narrower <4> alias, so the
    // descriptor is built as <5> and reinterpret_cast down at the call -- the same
    // idiom CgsDebugManager.cpp / CgsPhysicsSimulationModule.cpp already use. The name
    // rides through DoAllocate's second parameter.
    void* AllocateMemoryResource(rw::IResourceAllocator* lpAllocator, u32 luSize,
                                 u32 luAlignment, const char* lpcName)
    {
        rw::BaseResourceDescriptors<5> lDescriptor;
        for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;

        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), lpcName);
        return lResource.m_baseResources[0];
    }
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
    , mpStringTable(0)       // X360 +0x26FC (DWARF h:489)
{
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
// FLAG [the AEMS keystone -- stage 3's factory block is DEFERRED]: the console
// creates the three factories through GenericRwacFactory::Create @0x826C7AD0
// (spec {mpSystem=off_83271928, 128 entities, 32384 data} -- carve
// 4*(entities+4111)+sizes through the ENVIRONMENT's allocator, assert
// "lSpec.mpSystem" h:906), AemsFactory::Create @0x826DAC28 (spec
// {Handle<RwacFactory>, 128, 32384}; carve 4*(entities+354)+sizes; its refcount
// rides at +8 -- the AemsRWSampleFactory base) and SplicerFactory::Create
// @0x826DB130 (carve 4*(entities+448)+sizes), asserting mhRwacFactory /
// mhAemsFactory / mhSplicerFactory (cpp:196/:207/:219). NONE of the three
// factory ctors is reconstructed, AemsFactory does not derive Factory on the
// host yet (the ledgered keystone), and the mounted factory TUs must not gain
// unresolved ctor externals -- so the block is documented here and the handles
// stay null until that slice lands. The off_82FFBA0C interface-global publish
// (`= &this->+0x228 sub-object`) lands with the same slice (its consumer is the
// SndPlayer1 side).
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

        Handle<Environment> lhEnvironment = Environment::Create(lSpec);
        if (mhEnvironment.GetObject())
            mhEnvironment.GetObject()->Release();
        mhEnvironment.SetObject(lhEnvironment.GetObject());
        // (the transient handle's reference transfers to the member; the console
        // releases the temp after the assign -- net one owned reference)
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
        // Interim plain-store Handle model: the create's explicit Acquire IS the
        // member's owned ref, so no temp-release is emitted beside the store
        // (the console's temp Object::Release balances ITS real-ref-model
        // assign; emitting it here would drop the only ref).
        {
            GenericRwacFactorySpec lRwacSpec;
            lRwacSpec.mpSystem            = GetDefaultRwacSystem();
            lRwacSpec.mu32EntityCount     = 128;
            lRwacSpec.mu32DataSize        = 32384;
            lRwacSpec.mu32StringTableSize = 0;
            Handle<GenericRwacFactory> lhRwacFactory =
                GenericRwacFactory::Create(*mhEnvironment, lRwacSpec);
            mhRwacFactory = Handle<Factory>(lhRwacFactory.GetObject());
            CGS_ASSERT(!!mhRwacFactory, "mhRwacFactory");
        }
        // [2/3] AEMS -- REAL (AemsFactory::Create @0x826DAC28, cascade slice 2):
        // spec {the RWAC handle, 128, 32384, 0} passed by reference; the temp
        // assigned into +0x2260 and asserted (cpp:207). The console releases the
        // temp through the IAems vtable +4 slot and then the spec's RWAC handle
        // -- both releases balance real-ref-model acquires the interim
        // plain-store Handle model manages beside the stores instead (the
        // create's Acquire = the member ref; the ctor's own retain of the RWAC
        // pointer stands on its own).
        {
            AemsFactorySpec lAemsSpec;
            lAemsSpec.mpRwacFactory       = mhRwacFactory.GetObject();
            lAemsSpec.mu32EntityCount     = 128;
            lAemsSpec.mu32DataSize        = 32384;
            lAemsSpec.mu32StringTableSize = 0;
            Handle<AemsFactory> lhAemsFactory =
                AemsFactory::Create(*mhEnvironment, lAemsSpec);
            mhAemsFactory = Handle<Factory>(lhAemsFactory.GetObject());
            CGS_ASSERT(!!mhAemsFactory, "mhAemsFactory");
        }
        // [3/3] FLAG deferred: SplicerFactory::Create @0x826DB130 (assert
        // cpp:219) plus the off_82FFBA0C interface-global publish (`= &this->
        // +0x228`) that follows it -- the Splicer ctor slice next (SpliceManager
        // + VoicePool::Prepare @0x8268AC40, dossier re-exported; full decode in
        // progress/scratch_dossiers/aems_factory_cascade_codex.md).

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
                lpBuffer = AllocateMemoryResource(apAllocator, muStreamBufferSize, 16,
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
            using CgsDev::PerfMonCpu::AddMonitor;
            using CgsDev::PerfMonCpuPage;

            lrMonitors.miModule = AddMonitor(
                "Playback", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miEnvironmentUpdate = AddMonitor(
                " Environment", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miAemsFactoryUpdate = AddMonitor(
                "  Aems factory", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miRwacFactoryUpdate = AddMonitor(
                "  Rwac factory", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miSplicerFactoryUpdate = AddMonitor(
                "  Splicer factory", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miContentUpdate = AddMonitor(
                "  Content update", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miVoiceUpdate = AddMonitor(
                "  Voice update", static_cast<PerfMonCpuPage>(14), false, 1.0f, true);
            lrMonitors.miAemsFactoryUpdate2 = AddMonitor(
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
        CGS_ASSERT(false, "Invalid Prepare Stage");
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
            Environment* lpEnvironment = GetEnvironment();
            rw::Resource lResource;
            lResource.m_baseResources[0] = mpStringTable;
            for (u32 lu = 1; lu < 4; ++lu)
                lResource.m_baseResources[lu] = 0;
            lpEnvironment->GetAllocator()->DoFree(lResource);
            mpStringTable = lpNext;
        }
        meReleaseStage++;
        // fall through
    case 2:
        // Null-assign the three factory handles (the console's Handle null-assign
        // helpers @0x826A76A8 family: drop the owned reference, then store null).
        if (mhRwacFactory.GetObject())
            mhRwacFactory.GetObject()->Release();
        mhRwacFactory.SetObject(0);
        if (mhAemsFactory.GetObject())
            mhAemsFactory.GetObject()->Release();
        mhAemsFactory.SetObject(0);
        if (mhSplicerFactory.GetObject())
            mhSplicerFactory.GetObject()->Release();
        mhSplicerFactory.SetObject(0);
        meReleaseStage++;
        // fall through
    case 3:
        // Snapshot the main allocator BEFORE dropping the environment handle (the
        // stream-buffer frees below need it once the environment is gone).
        lpMainAllocator = GetEnvironment()->GetAllocator();
        if (mhEnvironment.GetObject())
            mhEnvironment.GetObject()->Release();
        mhEnvironment.SetObject(0);
        meReleaseStage++;
        // fall through
    case 4:
        for (u32 lu = 0; lu < 3; ++lu)
        {
            void* lpBuffer = maStreamBuffers[lu].mpBuffer;
            maStreamBuffers[lu].mReadStream = CgsFileSystem::ReadStream();   // the console +8 word zero
            maStreamBuffers[lu].mpBuffer    = 0;
            if (mbStreamsUsingMainAllocator)
            {
                CGS_ASSERT(lpMainAllocator != 0, "lpMainAllocator");
                rw::Resource lResource;
                lResource.m_baseResources[0] = lpBuffer;
                for (u32 luI = 1; luI < 4; ++luI)
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
        CGS_ASSERT(false, "Invalid Release Stage");
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
Environment* Module::GetEnvironment()
{
    CGS_ASSERT(mhEnvironment.GetObject(), "mpObject");
    return mhEnvironment.GetObject();
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

    Factory* lpAemsFactory = mhAemsFactory.GetObject();
    CGS_ASSERT(lpAemsFactory, "mpObject");
    GetAemsFactoryRegistry(lpAemsFactory)->Dump();
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
void Module::ImportStringTable(const u32* lpStringTableResource)
{
    if (!lpStringTableResource[4])
        return;

    const u8* lkpu8Base = reinterpret_cast<const u8*>(lpStringTableResource);
    const char* lkpcEnd = reinterpret_cast<const char*>(
        static_cast<uintptr_t>(lpStringTableResource[5]));
    const char* lkpcStart = reinterpret_cast<const char*>(
        lkpu8Base + (lpStringTableResource[1] + 7) * 4 + lpStringTableResource[2]);

    u32 lu32StringTableSize =
        static_cast<u32>(lkpcEnd - lkpcStart);
    if (lkpcEnd == lkpcStart)
        return;

    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    Module::StringTable* lpChunk = static_cast<Module::StringTable*>(
        lpEnvironment->Allocate(lu32StringTableSize + 4, 4, "StringTable"));
    if (!lpChunk)
        return;

    // Push the new chunk onto the head of the module's string-table list.
    lpChunk->mpNext = mpStringTable;
    mpStringTable   = lpChunk;

    // Re-derive the source start (the X360 recomputes it under the same a2[4] guard).
    const char* lkpcSource = reinterpret_cast<const char*>(
        lkpu8Base + (lpStringTableResource[1] + 7) * 4 + lpStringTableResource[2]);
    std::memcpy(lpChunk->macData, lkpcSource, lu32StringTableSize);

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

// ---------------------------------------------------------------------------
// Module::AttachVoice  @ 0x826D7D80  (DWARF h:345)
// Attaches lphContent to the named slot lu32SlotName on lphVoice (the real
// Voice::Attach(Name, Handle<Content>&) -- the u32 the asm builds on the stack and
// passes by reference IS the interned slot Name), then releases the transient
// references held by both handles (the X360 drops a ref on each owned object).
// ---------------------------------------------------------------------------
void Module::AttachVoice(Handle<Voice>* lphVoice, Handle<Content>* lphContent,
                         u32 lu32SlotName)
{
    CGS_ASSERT(lphVoice->GetObject(), "lhVoice");
    CGS_ASSERT(lphContent->GetObject(), "lhContent");

    CGS_ASSERT(lphVoice->GetObject(), "mpObject");

    bool lbAttached =
        lphVoice->GetObject()->Attach(Name(static_cast<uintptr_t>(lu32SlotName)),
                                      *lphContent);
    CGS_ASSERT(lbAttached, "lhVoice->Attach(lSlotName, lhContent)");

    if (lphVoice->GetObject())
        lphVoice->GetObject()->Release();
    if (lphContent->GetObject())
        lphContent->GetObject()->Release();
}

// ---------------------------------------------------------------------------
// Module::ConnectVoice  @ 0x826C14E0
// Resolves the submix voice for lu32SubmixId out of the environment, takes a
// reference on it, connects lphVoice's send lu32SendName to it, then releases the
// transient references.
// ---------------------------------------------------------------------------
void Module::ConnectVoice(Handle<Voice>* lphVoice, u32 lu32SendName, u32 lu32SubmixId)
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    // GetVoice (the IDA-truncated `GetV`, DWARF h:289) fills a transient handle
    // (var_80); the X360 reads the owned Voice*, takes a ref on it, then releases
    // the transient handle's reference.
    Handle<Voice> lhSubmixLookup = lpEnvironment->GetVoice(lu32SubmixId);
    Voice* lpSubmixVoice = lhSubmixLookup.GetObject();
    if (lpSubmixVoice)
        lpSubmixVoice->Acquire();
    if (lhSubmixLookup.GetObject())
        lhSubmixLookup.GetObject()->Release();

    CGS_ASSERT(lphVoice->GetObject(), "lhVoice");
    CGS_ASSERT(lpSubmixVoice, "lhVoice");  // X360 streams "Submix ID: <id>" here

    CGS_ASSERT(lphVoice->GetObject(), "mpObject");

    // The real Voice::Connect(Name, Handle<SubmixVoice>&) @0x826ACC90 -- the u32
    // built on the stack is the interned send Name, and the submix voice is the
    // SubmixVoice subclass (CgsSubmixVoice.h).
    Handle<SubmixVoice> lhSubmix(static_cast<SubmixVoice*>(lpSubmixVoice));
    bool lbConnected =
        lphVoice->GetObject()->Connect(Name(static_cast<uintptr_t>(lu32SendName)),
                                       lhSubmix);
    CGS_ASSERT(lbConnected, "lhVoice->Connect(lSendName, lhSubmixVoice)");

    if (lpSubmixVoice)
        lpSubmixVoice->Release();
    if (lphVoice->GetObject())
        lphVoice->GetObject()->Release();
}

// ---------------------------------------------------------------------------
// Module::CreateVoice  @ 0x826D7B00  (the IDA-truncated "Module::C"; DWARF h:318
// `Handle<Voice> CreateVoice(u32,u32,u32)` -- the by-value Handle return is the
// sret out-pointer this signature spells)
//
// Resolves the owning factory (by lFactoryName, the real Environment::GetFactory
// -- IDA's `Environment::Ge`) and the voice spec (by lu32SubmixName, looked up in
// the environment registry via the real Registry::GetEntity<VoiceSpec>) out of the
// environment, asks the factory to CreateVoice for the requested ident, runs the
// init-submix hack on the reserved ident, and stores the new voice into
// lphVoiceOut. On any failure path it clears lphVoiceOut and releases the
// resolved factory handle.
//
// FLAG: the X360 emits a streamed assert message
// ("E_COMMAND_VOICE_CREATE failed: VoiceSpec\n") on the failure path; reconstructed
// here as a plain CGS_ASSERT(false, ...) with the message text, matching the
// fire-assert effect (the StrStream machinery is style-only).
// ---------------------------------------------------------------------------
void Module::CreateVoice(Handle<Voice>* lphVoiceOut, u32 lu32SlotName,
                         const Name& lFactoryName, u32 lu32SubmixName)
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    // Resolve the owning factory handle by name (Environment::GetFactory, DWARF h:280).
    Handle<Factory> lhFactory = lpEnvironment->GetFactory(lFactoryName);
    Factory* lpFactory = lhFactory.GetObject();

    const VoiceSpec* lpSpec = 0;
    if (lpFactory)
    {
        Environment* lpEnv2 = mhEnvironment.GetObject();
        CGS_ASSERT(lpEnv2, "mpObject");
        Registry* lpRegistry = lpEnv2->GetRegistry();
        CGS_ASSERT(lpRegistry, "mpRegistry");
        Name lSpecName(static_cast<uintptr_t>(lu32SubmixName));
        lpSpec = lpRegistry->GetEntity<VoiceSpec>(lSpecName);
    }

    if (!lpFactory || !lpSpec)
    {
        CGS_ASSERT(false, "E_COMMAND_VOICE_CREATE failed: VoiceSpec");
        lphVoiceOut->SetObject(0);
        if (lpFactory)
            lpFactory->Release();
        return;
    }

    Handle<Voice> lhNewVoice;
    if (lpFactory->CreateVoice<Voice>(*lpSpec, lhNewVoice, lu32SlotName) ==
        static_cast<u32>(-1))
    {
        if (lhNewVoice.GetObject())
            lhNewVoice.GetObject()->Release();
        CGS_ASSERT(false, "E_COMMAND_VOICE_CREATE failed: VoiceSpec");
        lphVoiceOut->SetObject(0);
        if (lpFactory)
            lpFactory->Release();
        return;
    }

    // The `*(voice+12) = ident` store: the real Voice::SetIdent (DWARF CgsVoice.h:567
    // -- the old rival named it SetSlotName; the +0xC word is Voice::mIdent).
    lhNewVoice.GetObject()->SetIdent(lu32SlotName);

    if (lu32SlotName == KU_INIT_SND9_SUBMIX_IDENT)
    {
        CGS_ASSERT(lFactoryName == GenericRwacFactorySkName(),
                   "GenericRwacFactory::SK_NAME == lFactoryName");
        Handle<Voice> lhInitVoice(lhNewVoice.GetObject());
        if (lhInitVoice.GetObject())
            lhInitVoice.GetObject()->Acquire();
        HACK_SetSnd9InitSubmix(&lhInitVoice);
    }

    Voice* lpNewVoice = lhNewVoice.GetObject();
    lphVoiceOut->SetObject(lpNewVoice);
    if (!lpNewVoice)
    {
        if (lpFactory)
            lpFactory->Release();
        return;
    }

    lpNewVoice->Acquire();
    lpNewVoice->Release();
    lpFactory->Release();
}

// ---------------------------------------------------------------------------
// Module::CreateContent  @ 0x826C12A8
//
// Mirror of the sibling Module::C (CreateVoice @0x826D7B00). Resolves the owning
// Factory (by lContentClassName) and the ContentSpec (by lContentSpecName) out of the
// environment, asks the factory to CreateContent, wires the created Content's ident
// (+0x10 = mIdent) and owner iface (+0x14 = mpLoadService, = this+0x22C when non-null),
// stores it into *lppContentOut, refs it, and releases the transient factory handle.
// On failure it clears *lppContentOut and releases the factory handle.
//
// FLAG: the X360 streams an assert message on the failure path; reconstructed as a
// plain CGS_ASSERT(false, <leading literal>) exactly as Module::C reduced its own
// streamed voice-create failure -- the StrStream interpolation + trailing \n dropped.
// ---------------------------------------------------------------------------
Content** Module::CreateContent(Content** lppContentOut, u32 lu32Ident,
                                const Name& lContentClassName,
                                const Name& lContentSpecName)
{
    Environment* lpEnvironment = mhEnvironment.GetObject();
    CGS_ASSERT(lpEnvironment, "mpObject");

    // Resolve the owning content factory handle by class name (Environment::GetFactory).
    Handle<Factory> lhFactory = lpEnvironment->GetFactory(lContentClassName);
    Factory* lpFactory = lhFactory.GetObject();

    const ContentSpec* lpContentSpec = 0;
    if (lpFactory)
    {
        Environment* lpEnv2 = mhEnvironment.GetObject();
        CGS_ASSERT(lpEnv2, "mpObject");
        Registry* lpRegistry = lpEnv2->GetRegistry();
        CGS_ASSERT(lpRegistry, "mpRegistry");
        Name lSpecName(lContentSpecName);
        lpContentSpec = lpRegistry->GetEntity<ContentSpec>(lSpecName);
    }

    if (!lpFactory || !lpContentSpec)
    {
        CGS_ASSERT(false, "E_COMMAND_CONTENT_CREATE failed with content spec ");
        *lppContentOut = 0;
        if (lpFactory)
            lpFactory->Release();
        return lppContentOut;
    }

    Handle<Content> lhContent;
    if (lpFactory->CreateContent(*lpContentSpec, lhContent, lu32Ident) ==
        static_cast<u32>(-1))
    {
        if (lhContent.GetObject())
            lhContent.GetObject()->Release();
        CGS_ASSERT(false, "E_COMMAND_CONTENT_CREATE failed with content spec ");
        *lppContentOut = 0;
        if (lpFactory)
            lpFactory->Release();
        return lppContentOut;
    }

    // Wire the freshly created content's owner fields BY NAME (the module's own
    // `stw +0x10 / stw +0x14` stores after the factory call): mIdent, and the
    // IContentLoadService base sub-object pointer -- since phase B4 the REAL MI
    // upcast (the console's this+0x22C adjustment IS this static_cast).
    (*lhContent).mIdent = lu32Ident;                                       // Content +0x10
    (*lhContent).SetLoadService(static_cast<IContentLoadService*>(this));  // Content +0x14

    Content* lpContent = lhContent.GetObject();
    *lppContentOut = lpContent;
    if (!lpContent)
    {
        if (lpFactory)
            lpFactory->Release();
        return lppContentOut;
    }

    lpContent->Acquire();   // X360: ++*(content+4) inline
    lpContent->Release();
    lpFactory->Release();
    return lppContentOut;
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

    // The content-load response record (receiver event type 4) -- the resource
    // side's echo of the request DoServiceContentLoadRequest posts. Console word
    // view (the ProcessResourceReceiverQueue asm): [1] the requester's resource
    // pointer (the load request's lpUserData), [6..] the embedded ResourceHandle
    // whose mpResourceMemory the cpp:598 assert dereferences. Host contract:
    // the same fields at host widths (the PC producer -- the item-3 content
    // slice -- builds this record).
    struct ContentLoadResponse
    {
        void*                        mpUser;       // [0]  the receiver-queue route echo
        CgsResource::BaseResourcePtr* mpTarget;    // [1]  the requester's ResourcePtr
        void*                        mapReserved[4];// [2..5]
        CgsResource::ResourceHandle  mHandle;      // [6..] the resolved resource
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
    CgsDev::PerfMonCpu::StartMonitor(GetEnvironment()->GetCpuMonitors().miModule);

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

    CgsDev::PerfMonCpu::StartMonitor(GetEnvironment()->GetCpuMonitors().miEnvironmentUpdate);
    GetEnvironment()->Update(mf32TimeStep);
    CgsDev::PerfMonCpu::StopMonitor(GetEnvironment()->GetCpuMonitors().miEnvironmentUpdate);

    apOutputBuffer->UnlockForWrite();
    mpInputBuffer  = 0;
    mpOutputBuffer = 0;

    CgsDev::PerfMonCpu::StopMonitor(GetEnvironment()->GetCpuMonitors().miModule);
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
                const ContentLoadResponse* lpResponse =
                    reinterpret_cast<const ContentLoadResponse*>(lpEvent);
                ResourcePtrBinder::Bind(lpResponse->mpTarget, &lpResponse->mHandle);
                CGS_ASSERT(*static_cast<void* const*>(lpResponse->mHandle.mpResourceMemory) != 0,
                           "lpResponse->GetHandle().GetResource()->GetMemoryResource()");
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
                CGS_ASSERT(false, "Unanticiapated Resource Response. Eek.\n");
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
                lrBuffer.mReadStream      = CgsFileSystem::ReadStream();
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
    CGS_ASSERT(lpReadStream != 0, "0 != lpReadStream");

    for (s32 li = 0; li < static_cast<s32>(SKU_NUMBER_OF_STREAM_BUFFERS); ++li)
    {
        if (lpReadStream == &maStreamBuffers[li].mReadStream)
            return li;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Module::DoServiceContentLoadRequest  @ 0x826F9F88  (the IContentLoadService
// override; bodied phase B4)
//
// RESOURCE_MODULE loads only: assert the user data (cpp:1171), hash the content
// name, and post the 24-byte load request {&mResourceReceiverQueue, lpUserData,
// mi32PoolId, hash64} as event type 4 into the attached output buffer's request
// queue. The console widens the 32-bit hash into the trailing 64-bit id slot.
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

    // The 24-byte console request record: {route, userData, poolId, hash64}.
    struct ContentLoadRequest
    {
        CgsModule::BaseEventReceiverQueue* mpUser;
        void*                              mpUserData;
        s32                                miPoolId;
        u64                                muResourceId;
    } lRequest;
    lRequest.mpUser       = &mResourceReceiverQueue;
    lRequest.mpUserData   = lpUserData;
    lRequest.miPoolId     = mi32PoolId;
    lRequest.muResourceId = static_cast<u64>(static_cast<u32>(liHash));

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
    mStreamMutex.Lock();

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
        CGS_ASSERT(false, "We've run out of Audio Stream Buffers.");
        mStreamMutex.Unlock();
        return 0;
    }

    // Resolve the requesting plug-in's content out of the environment.
    Handle<Content> lhContent = GetEnvironment()->GetR(
        static_cast<u32>(reinterpret_cast<uintptr_t>(lrSpec.mpPlugin)));
    CGS_ASSERT(lhContent.GetObject() != 0, "lHandle");

    // Seed the record and hand the carve buffer back through the spec.
    StreamBuffer& lrBuffer = maStreamBuffers[liIndex];
    lrBuffer.mfGraceWaitTime  = 0.0f;
    lrBuffer.mReadStream      = CgsFileSystem::ReadStream();
    lrBuffer.mBufferStatus    = StreamBuffer::E_USING_BUFFER;
    lrBuffer.mVoiceId         = lhContent.GetObject()->mIdent;
    lrBuffer.mbQueuedForClose = false;
    *lrSpec.mppvBuffer = lrBuffer.mpBuffer;

    // Build + post the open request (event type 16) into the deferred queue.
    CgsResource::Events::OpenReadStreamRequest lRequest;
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
        CGS_ASSERT(false, "mDeferredResourceRequestQueue.OpenReadStream( lRequest )");
    }

    // Release the transient content reference (the console's inline refcount
    // decrement + DoDispose-at-zero == Object::Release).
    lhContent.GetObject()->Release();

    mStreamMutex.Unlock();
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
            CGS_ASSERT(false, "mDeferredResourceRequestQueue.CloseReadStream( lRequest )");
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

    CGS_ASSERT(leEnumIndex <= Module::E_PREPARESTAGE_DONE,
               "leEnumIndex <= Module::E_PREPARESTAGE_DONE");

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
