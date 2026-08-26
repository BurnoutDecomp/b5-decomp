#include "GameSource/Sound/Module/BrnRootSoundModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor (START stage)
#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"            // CgsSound::TestBed::Allocator (the four carve globals)
#include "GameShared/GameClasses/System/PC/CgsStreamHeadersPC.h"         // StreamHeadersPC::Preload (the REGISTRY_LOAD stage's data half)
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"       // AllocatorList (the bank carves)
#include "rw/rwcore_general_alloc.h"                                     // rw::core::GeneralResourceAllocator (bank 0x18/0x19)
#include "rw/audio/core/PlugIn.h"                                        // rw::audio::core::System (CreateInstance/Lock/...)
#include "SDKs/Csis/CsisSystem.h"                                        // Csis::System::SetAllocator / Init
#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h" // spPathPrefix
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // the audio-stream LinearMalloc (PLAYBACK stage)
#include "coreallocator/icoreallocator_interface.h"                      // EA::Allocator::ICoreAllocator (the bridge base)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint (bring-up trace)

// BrnSound::Module::RootSoundModule -- see the header. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (ctor 0x827E4808, Construct 0x826AF350, Prepare 0x826FABF8), cross-checked against the DecFIGS
// PS3 build (0x8D0570) whose symbols name every store this TU makes.

// ---------------------------------------------------------------------------------------------
// File-scope globals (DWARF BrnRootSoundModule.cpp:50-64; the PS3 Prepare names all four
// allocators). Each Prepare stage carves one RW general resource out of the game-data
// AllocatorList and hands it to the matching subsystem through one of these testbed wrappers:
//   gRwacTestBedAlloc     X360 0x82FFBF78  (RWAC stage, bank 0x18 -> rw::audio::core::System)
//   gCsisTestBedAlloc     X360 0x830086F0  (RWAC stage, bank 9    -> Csis::System)
//   gPlaybackTestBedAlloc X360 0x8300A6F8  (PLAYBACK stage, bank 9)
//   gLogicTestBedAlloc    X360 0x830060D0  (LOGIC stage, bank 0x19)
//   KB_TESTBED_ALLOCATORS_VERBOSE / _SANITY  X360 0x82FFB810/0x82FFB811 (debug toggles, off)
//   KI_DEBUG_PRINT_AUDIO_ALLOCATIONS         (debug toggle, off)
// ---------------------------------------------------------------------------------------------
namespace
{
    bool KB_TESTBED_ALLOCATORS_VERBOSE = false;
    bool KB_TESTBED_ALLOCATORS_SANITY  = false;
    s32  KI_DEBUG_PRINT_AUDIO_ALLOCATIONS = 0;

    // Static-init names from the X360 dynamic initializers (0x82C61A88/AC8/B08: Allocator(&g,
    // "<name>", 0); the gRwac one (0x82C61A48) was not exported -- name matched to the sibling
    // pattern, FLAG: unverified against the binary string).
    CgsSound::TestBed::Allocator gRwacTestBedAlloc("Rwac", 0);
    CgsSound::TestBed::Allocator gCsisTestBedAlloc("Csis", 0);
    CgsSound::TestBed::Allocator gPlaybackTestBedAlloc("Playback", 0);
    CgsSound::TestBed::Allocator gLogicTestBedAlloc("Logic", 0);

    // -----------------------------------------------------------------------------------------
    // FLAG [host interface seam, 2026-08-25 faithful-audio-engine phase A4]: on the console
    // rw::IResourceAllocator's vtable HEAD is ICoreAllocator-shaped (rwcore.pdb
    // IResourceAllocator_vtbl: {dtor, Alloc, Alloc, Free, ...}) -- i.e. the ONE testbed object
    // serves both the rw DoAllocate face and the ICoreAllocator face System::CreateInstance
    // consumes. The host rwcore_structs.h models the interfaces separately, so this adapter
    // presents the testbed through the ICoreAllocator face; every allocation still flows
    // through the testbed's tracked DoAllocate (guards/history intact). Free is the
    // IResourceAllocator::Free default (a no-op on the host testbed) -- the engine frees only
    // on teardown/failure paths, FLAG'd as a host leak-on-teardown until the testbed models it.
    // -----------------------------------------------------------------------------------------
    struct RwacCoreAllocatorBridge : public EA::Allocator::ICoreAllocator
    {
        explicit RwacCoreAllocatorBridge(CgsSound::TestBed::Allocator* lpTestBed)
            : mpTestBed(lpTestBed) {}

        virtual void* Alloc(size_t nSize, const char* pName, unsigned int nFlags)
        {
            return Alloc(nSize, pName, nFlags, 16, 0);
        }

        virtual void* Alloc(size_t nSize, const char* pName, unsigned int /*nFlags*/,
                            unsigned int nAlignment, unsigned int /*nAlignmentOffset*/)
        {
            rw::ResourceDescriptor lDescriptor;
            for (u32 lu = 0; lu < 4; ++lu)
            {
                lDescriptor.m_baseResourceDescriptors[lu].m_size      = 0;
                lDescriptor.m_baseResourceDescriptors[lu].m_alignment = 1;
            }
            lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(nSize);
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = nAlignment ? nAlignment : 16;

            rw::IResourceAllocator* lpBase = mpTestBed;   // the tracked DoAllocate path
            rw::Resource lResource = lpBase->DoAllocate(lDescriptor, pName ? pName : "Rwac");
            return lResource.m_baseResources[0];
        }

        virtual void Free(void* lpBlock, size_t /*nSize*/)
        {
            rw::IResourceAllocator* lpBase = mpTestBed;
            lpBase->Free(lpBlock, 0);   // host default no-op (see the FLAG above)
        }

        CgsSound::TestBed::Allocator* mpTestBed;
    };

    RwacCoreAllocatorBridge gRwacCoreBridge(&gRwacTestBedAlloc);
}

// The Csis mutex thunks the hooks below forward to (rw::audio::core, System.cpp).
namespace rw { namespace audio { namespace core {
    void CsisMutexLock();
    void CsisMutexUnlock();
} } }

namespace BrnSound
{
namespace Module
{
    // DWARF BrnRootSoundModule.cpp:133 (X360 dword_82FFB818).
    s32 RootSoundModule::msiMutexLockCount = 0;

    // The Csis mutex callbacks the RWAC stage installs into mpSystem's lock hooks
    // (+0x40/+0x44/+0x3C). Bodied 2026-08-25, faithful-audio-engine phase A4:
    //   MutexLockFn @0x82682A20:     bl CsisMutexLock ; ++dword_82FFB818
    //   MutexUnlockFn (ICF-folded on X360; PS3 0x8D0570 installs it @ +0x44):
    //                                bl CsisMutexUnlock ; --dword_82FFB818
    //   MutexIsLockedFn @0x82682A68: return dword_82FFB818 > 0
    void RootSoundModule::MutexLockFn()
    {
        rw::audio::core::CsisMutexLock();
        ++msiMutexLockCount;
    }
    void RootSoundModule::MutexUnlockFn()
    {
        rw::audio::core::CsisMutexUnlock();
        --msiMutexLockCount;
    }
    bool RootSoundModule::MutexIsLockedFn()
    {
        return msiMutexLockCount > 0;
    }

    // 0x827E4808. The full X360 body:
    //   *this = &off_820CE500;            base ModuleSingleBuffered subobject ctor -> installs the
    //   RWMutex(this+0x10, 0, 1);         base vtable and default-constructs the input/output
    //   RWMutex(this+0x118, 0, 1);        RWMutexes (all produced by the base ctor here)
    //   *this = off_820D1100;             our own vtable (emitted implicitly)
    //   SoundLogicModule(this+0x280);     mLogicModule   (member ctor, implicit)
    //   DebugComponent(this+0x14D38);     mDebugComponent (member ctor, implicit)
    // Nothing else: the X360 ctor leaves every state field (mpSystem, the stage words, the
    // perfmon handles) untouched -- Construct() seeds them and always runs first on the module
    // path, so this ctor body is intentionally empty.
    RootSoundModule::RootSoundModule()
    {
    }

    // 0x826AF350 (vtable slot 0). Steps, in X360 order:
    //   [0] the listing opens with `CgsSceneManager::CgsCollision::BaseCollisionGenerator::
    //       Destruct(3)` -- a Hex-Rays/ICF artifact (r3 = the constant 3, not a `this`); the
    //       folded callee is a no-op frame. Not a real call; nothing to reproduce.
    void RootSoundModule::Construct()
    {
        // [1] *(this+0x14D18) = 0 -- clear the RWAC system handle before anything else.
        mpSystem = 0;

        // [2] the module base (resets the prepare/release stage machines, constructs the two
        //     DataBuffers, clears mbIsNewModule).
        CgsModule::ModuleSingleBuffered::Construct();

        // [3] (*(vtbl(this+0x4B8) + 0x40))(this+0x4B8, 6) -- virtual-init the PLAYBACK module
        //     (mLogicModule's CgsSound::Logic::Module engine base embeds it at +0x238 --
        //     REAL since phase B5) with module-id 6.
        mLogicModule.GetPlaybackModule().Construct(6);

        // [4] (*(vtbl(this+0x280)))(this+0x280) -- the logic module's Construct (vtable slot 0).
        mLogicModule.Construct();

        // [5]+[6] BrnSound::Debug::DebugComponent::Construct(&mDebugComponent, this+0x4B8,
        //     this+0x280) + CgsDev::DebugComponent::Register(&mDebugComponent) -- wire the sound
        //     debug pages to the playback + logic modules and register the component. [gated]
        //     the @0x826A1230 Construct dossier is NOT in the export set (re-export needed --
        //     the playback arg EXISTS since phase B5); lands with the sound debug-page slice.

        // [7] the event-receiver queue @ this+0x13900: capacity 5120, align 16, bind + Clear.
        mReceiverQueue.Construct();

        // [8] seed the stage machines + registry cursors (X360 this+0x14D1C..+0x14D2C).
        mePrepareStage      = E_PREPARESTAGE_START;
        meReleaseStage      = E_RELEASESTAGE_DONE;
        meResourceStage     = E_RESOURCE_LOAD_NOT_STARTED;
        mu32CurrentRegistry = 0;
        meCurrentRegistry   = 0;

        // [9] *(this+4) = 1 -- the CgsModule::Module "new module" flag: this module runs the
        //     IOBufferStack path, so the base ModuleSingleBuffered::Prepare/Release skip their
        //     legacy DataBuffer stages for it (which is exactly what makes the SELF stage below
        //     complete).
        mbIsNewModule = true;
    }

    // 0x826FABF8 (vtable +64). DWARF-true signature (BrnRootSoundModule.cpp:249). A resumable
    // stage machine, run forward from mePrepareStage each call; each attempted stage stamps
    // mePrepareStage first and stamps meReleaseStage (the "last completed stage" the Release
    // machine unwinds from) on success. Any stage that reports "still preparing" falls to the
    // common exit, which destroys this call's scratch IO buffers and returns false so
    // LoadSoundModule retries next frame. Case order follows the X360 EXECUTION order
    // (0,1,2,3,6,4,7 -- fallthrough follows source order, not case value).
    bool RootSoundModule::Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                                  CgsModule::IOBufferStack* lpInputBufferStack,
                                  CgsModule::IOBufferStack* lpOutputBufferStack,
                                  Io::RootInputBuffer* lpSoundModuleInputBuffer,
                                  Io::RootOutputBuffer* lpSoundModuleOutputBuffer)
    {
        // X360 BrnRootSoundModule.cpp:256-259.
        CGS_ASSERT(lpInputBufferStack != 0, "lpInputBufferStack != NULL");
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");
        CGS_ASSERT(lpSoundModuleInputBuffer != 0, "lpSoundModuleInputBuffer != NULL");
        CGS_ASSERT(lpSoundModuleOutputBuffer != 0, "lpSoundModuleOutputBuffer != NULL");

        // Per-call scratch IO buffers -- all three REAL since phase B5:
        //   Io::LogicOutputBuffer                        on the OUTPUT stack ("SoundLogic")
        //   CgsSound::Playback::Module::Io::InputBuffer  on the INPUT stack  ("SoundPlayback")
        //   CgsSound::Playback::Module::Io::OutputBuffer on the OUTPUT stack ("SoundPlayback"),
        //     then Clear()ed (its queue Clear + freed-array count zero).
        Io::LogicOutputBuffer* lpLogicOutputBuffer = 0;
        lpOutputBufferStack->CreateIOBuffer<Io::LogicOutputBuffer>(&lpLogicOutputBuffer, "SoundLogic");
        CgsSound::Playback::Module::Io::InputBuffer* lpPlaybackInputBuffer = 0;
        lpInputBufferStack->CreateIOBuffer<CgsSound::Playback::Module::Io::InputBuffer>(
            &lpPlaybackInputBuffer, "SoundPlayback");
        CgsSound::Playback::Module::Io::OutputBuffer* lpPlaybackOutputBuffer = 0;
        lpOutputBufferStack->CreateIOBuffer<CgsSound::Playback::Module::Io::OutputBuffer>(
            &lpPlaybackOutputBuffer, "SoundPlayback");
        if (lpPlaybackOutputBuffer)
            lpPlaybackOutputBuffer->Clear();

        bool lbPrepared = false;
        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            // 5-arg AddMonitor (asm 0x826FAD88: r4=0xE page, r5=0 minimum, f1=2.0 budget,
            // r7=1 libperf-tagged; r6 is the float's reserved GPR slot, not an argument).
            // Page 14 is the sound page of the perfmon overlay; only GENERAL(0)/MAX(24) are
            // named in the recovered PerfMonCpuPage enum so far.
            miLogicUpdate = CgsDev::PerfMonCpu::AddMonitor(
                "Sound Logic", (CgsDev::PerfMonCpuPage)14, false, 2.0f, true);
            // fall through
        case E_PREPARESTAGE_SELF:
            mePrepareStage = E_PREPARESTAGE_SELF;
            // The module base's own prepare. Completes immediately for this module: Construct
            // set mbIsNewModule, so the base skips its legacy DataBuffer stages.
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                break;
            meReleaseStage = E_RELEASESTAGE_SELF;
            // fall through
        case E_PREPARESTAGE_RWAC:
        {
            mePrepareStage = E_PREPARESTAGE_RWAC;
            // The RWAC + Csis bring-up, REAL since 2026-08-25 (faithful-audio-engine phase
            // A4 -- the vendor System.cpp + Csis::System::SetAllocator + the mounted
            // rw/audio subset retired the old "console-only middleware" gate). X360 order
            // preserved; two asm-verified corrections vs the old comment recipe:
            // Init(SetAllocator(...)) is ONE expression, and VectorToCsisMutex runs BEFORE
            // the three hook stores (the stores overwrite what it vectored).

            // * carve bank 0x18 (GetRWGeneralResource 0x823F3F98) -> gRwacTestBedAlloc.
            //   [deferred slice] the console also puts the carve's linear heap in
            //   no-coalesce mode first (rw::LinearResourceAllocator::GetLinearHeapBase
            //   @0x82BBC488 + EA::Allocator::GeneralAllocator::SetOption(1,0) @0x82B4DF60)
            //   -- a heap-tuning step whose EA-GeneralAllocator plumbing is not homed on
            //   the host GeneralResourceAllocator yet; behaviourally inert for bring-up.
            rw::core::GeneralResourceAllocator* lpRwacBank =
                lpAllocatorList->GetRWGeneralResourceAllocator(0x18);
            // field_0x0 = the +0 IResourceAllocator interface subobject (the host
            // composition stand-in for the console IS-A base; established pattern).
            gRwacTestBedAlloc.SetAllocator(&lpRwacBank->field_0x0);
            gRwacTestBedAlloc.SetVerbose(KB_TESTBED_ALLOCATORS_VERBOSE);
            gRwacTestBedAlloc.SetSanityCheck(KB_TESTBED_ALLOCATORS_SANITY);

            // * rw::audio::core::System::CreateInstance(&gRwacTestBedAlloc, 0x30000) ->
            //   mpSystem (assert "mpSystem", cpp:336). The bridge presents the testbed
            //   through the ICoreAllocator face (see RwacCoreAllocatorBridge above);
            //   byte_82FFBF89 = 1 == the testbed's rwac-locked test toggle.
            mpSystem = rw::audio::core::System::CreateInstance(&gRwacCoreBridge, 196608);
            CGS_ASSERT(mpSystem != 0, "mpSystem");
            gRwacTestBedAlloc.EnableRwacLockedTest(true);

            // Bring-up trace (host log line; the console has no equivalent print).
            *CgsDev::Log::gpDebugPrint << "[rwac] System::CreateInstance -> "
                                       << (mpSystem ? "LIVE" : "NULL")
                                       << " (bank 0x18 carve "
                                       << (lpRwacBank ? "present" : "MISSING") << ")\n";

            // * the Csis side: Init(SetAllocator(&gCsisTestBedAlloc)) -- one expression.
            //   [deferred slice] the console first carves bank 9 into a dedicated
            //   "CsisPrivateHeap" sub-allocator (GetResourceDescriptor 0x2000/4 + the
            //   bank allocator's virtual CreateAllocator + Initialize) and backs
            //   gCsisTestBedAlloc with it; the host GeneralResourceAllocator does not
            //   model the sub-allocator factory yet, and the committed Csis slice never
            //   allocates (Subscribe/Lock only), so gCsisTestBedAlloc stays un-backed
            //   here -- the wiring shape (SetAllocator's return feeding Init) is the
            //   X360's.
            gCsisTestBedAlloc.SetVerbose(KB_TESTBED_ALLOCATORS_VERBOSE);
            gCsisTestBedAlloc.SetSanityCheck(KB_TESTBED_ALLOCATORS_SANITY);
            Csis::System::SetAllocator(&gCsisTestBedAlloc);   // X360: Init(SetAllocator(&g)) --
            Csis::System::Init();                             // SetAllocator's return feeds Init

            // * VectorToCsisMutex FIRST, then the three hook installs overwrite what it
            //   vectored (the X360 store order); then the locked tuning pair.
            rw::audio::core::System::VectorToCsisMutex(mpSystem);
            mpSystem->mpfnLock     = &RootSoundModule::MutexLockFn;
            mpSystem->mpfnUnlock   = &RootSoundModule::MutexUnlockFn;
            mpSystem->mpfnIsLocked = &RootSoundModule::MutexIsLockedFn;
            rw::audio::core::System::Lock(mpSystem);
            mpSystem->mfCpuLoadPercent = 100.0f;   // *(mpSystem+0x10CC), the DAC watermark
            rw::audio::core::System::SetThreadProcessor(mpSystem, 4);
            rw::audio::core::System::Unlock(mpSystem);

            // * the stream-file path prefix (X360 dword_82FFBA08).
            rw::audio::core::SndPlayer1_CgsStreamMod::spPathPrefix = "SOUND\\STREAMS\\";

            meReleaseStage = E_RELEASESTAGE_RWAC;
            // fall through
        }
        case E_PREPARESTAGE_PLAYBACK_MODULE:
        {
            mePrepareStage = E_PREPARESTAGE_PLAYBACK_MODULE;
            // REAL since phase B5. X360: carve bank 9 ("Playback") -> gPlaybackTestBedAlloc;
            // System::Lock(mpSystem); virtual Prepare on the playback module (this+0x4B8,
            // vtable +0x44 = CgsSound::Playback::Module::Prepare 0x826E90C0) with
            // (&gPlaybackTestBedAlloc, the audio-stream LinearMalloc @ AllocatorList +660);
            // Unlock. A false return unwinds to the common exit (still preparing).
            // [deferred slice] the console's no-coalesce heap tuning on the bank-9 carve is
            // elided here for the same reason as the RWAC stage's (see above).
            gPlaybackTestBedAlloc.SetAllocator(
                &lpAllocatorList->GetRWGeneralResourceAllocator(9)->field_0x0);
            gPlaybackTestBedAlloc.SetVerbose(KB_TESTBED_ALLOCATORS_VERBOSE);
            gPlaybackTestBedAlloc.SetSanityCheck(KB_TESTBED_ALLOCATORS_SANITY);

            // FLAG (PC seam): the host CreateAllocators Construct()s the audio-stream
            // linear region but does not Create() it with a backing carve yet, so its
            // GetSize() is 0 -- hand Prepare a null LinearMalloc so it takes its own
            // REAL no-linear-region stream path (0x20000 x 4 blocks through the main
            // allocator) until that region carve lands.
            CgsMemory::LinearMalloc* lpStreamMalloc = lpAllocatorList->GetAudioStreamAllocator();
            if (lpStreamMalloc != 0 && lpStreamMalloc->GetSize() == 0)
                lpStreamMalloc = 0;

            rw::audio::core::System::Lock(mpSystem);
            const bool lbPlaybackPrepared = mLogicModule.GetPlaybackModule().Prepare(
                &gPlaybackTestBedAlloc, lpStreamMalloc);
            rw::audio::core::System::Unlock(mpSystem);
            if (!lbPlaybackPrepared)
                break;

            meReleaseStage = E_RELEASESTAGE_PLAYBACK_MODULE;
            // fall through
        }
        case E_PREPARESTAGE_REGISTRY_LOAD:
            mePrepareStage = E_PREPARESTAGE_REGISTRY_LOAD;
            // [gated] X360: if (!RegistryLoad(this, lpLogicOutputBuffer)) { LockForWrite(
            //   lpSoundModuleOutputBuffer); LockForRead(lpLogicOutputBuffer); BridgeLogicToRoot
            //   (lpLogicOutputBuffer, lpSoundModuleOutputBuffer); unlock both; -> still
            //   preparing }. RegistryLoad (0x826EBA08) streams the CSIS/AEMS registries through
            //   the playback module; blocked on the playback stage above + the RootOutputBuffer
            //   request interfaces.
            //
            // ⭐ WHAT IS NOT BLOCKED IS THE TIMING, restored 2026-08-16 (boot audit
            // F-P5-11/F7). The console reads SOUND\STREAMS\StreamHeaders.bundle and the
            // StreamsRegistry HERE -- RegistryLoad merges the registry's ContentSpecs into
            // the playback Registry, and StreamingStateManager::Prepare @0x826EE680 loads
            // the headers bundle beside it -- i.e. during loading-screen stage 4, with the
            // loading screen up. The PC read the same two files LAZILY, on the first
            // lookup, which in practice was "when the first boot video asks for its audio":
            // several seconds and a whole flow transition later, and inside the very frame
            // that wanted to start playing. StreamHeadersPC::Preload does that read now.
            // The full stage stays [gated] on the rw::audio engine; this is its data half.
            // FLAG (PC leaf): host-only call, no @0x82 anchor of its own -- the console
            // equivalent is the RegistryLoad + StreamingStateManager::Prepare pair above.
            CgsSystem::StreamHeadersPC::Preload();
            meReleaseStage = E_RELEASESTAGE_REGISTRY_LOAD;
            // fall through
        case E_PREPARESTAGE_LOGIC_MODULE:
        {
            mePrepareStage = E_PREPARESTAGE_LOGIC_MODULE;
            // The allocator carve, REAL since 2026-08-25 (phase A4): bank 0x19 ->
            // gLogicTestBedAlloc. [deferred slice] the console's no-coalesce heap tuning
            // is elided here for the same reason as the RWAC stage's (see above).
            gLogicTestBedAlloc.SetAllocator(
                &lpAllocatorList->GetRWGeneralResourceAllocator(0x19)->field_0x0);
            gLogicTestBedAlloc.SetVerbose(KB_TESTBED_ALLOCATORS_VERBOSE);
            gLogicTestBedAlloc.SetSanityCheck(KB_TESTBED_ALLOCATORS_SANITY);

            // The logic module's own prepare (X360: virtual, this+0x280 vtable +0x58 =
            // SoundLogicModule::Prepare 0x82703C18) with the logic allocator, the ROOT input
            // buffer and this call's logic-output scratch. The X360 treats a false return as
            // "still preparing" -- but only AFTER the bridge/dispatch block below runs, so the
            // module's resource requests still flow out while it loads.
            bool lbLogicPrepared = mLogicModule.Prepare(
                &gLogicTestBedAlloc, lpSoundModuleInputBuffer, lpLogicOutputBuffer);

            // [gated] the per-call bridge + playback dispatch (X360, in order):
            //   LockForWrite(lpSoundModuleOutputBuffer);
            //   LockBuffersForIO(playbackIn, lpLogicOutputBuffer);        (CgsModuleUtils.h:259)
            //   BridgeLogicToRoot(lpLogicOutputBuffer, lpSoundModuleOutputBuffer);
            //   UnlockBuffersForIO(playbackIn, lpLogicOutputBuffer);      (CgsModuleUtils.h:272)
            //   UnlockForWrite(lpSoundModuleOutputBuffer);
            //   playback module Update (this+0x4B8 vtable +0x48) with (playbackIn, playbackOut);
            //   LockForWrite(lpSoundModuleOutputBuffer);
            //   LockBuffersForIO(lpSoundModuleInputBuffer, playbackOut);
            //   RootOutputBuffer::GetReso... queue Append<4096,16> from the playback output's
            //   event queue + Array<u32,3>::AppendArray(this->..., playbackOut+4);
            //   UnlockBuffersForIO + UnlockForWrite.
            // Blocked on: the playback Io pair + the RootOutputBuffer request interfaces +
            // CgsModuleUtils.h (LockBuffersForIO). All land with the playback-module TU group.

            if (!lbLogicPrepared)
                break;
            meReleaseStage = E_RELEASESTAGE_LOGIC_MODULE;
            // fall through
        }
        case E_PREPARESTAGE_DONE:
            // Fully prepared: the release machine now covers everything (START) and the prepare
            // machine parks at DONE (a re-entered Prepare returns true again, as on the X360).
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            lbPrepared = true;
            break;
        default:
            // X360 BrnRootSoundModule.cpp:576.
            CGS_ASSERT(false, "Invalid Stage\n");
            break;
        }

        // Common exit (X360 LABEL_24/LABEL_26): destroy this call's scratch IO buffers -- the
        // playback output, the playback input, then the logic output (LIFO, all three real
        // since phase B5) -- and report the machine's verdict.
        lpOutputBufferStack->DestroyIOBuffer<CgsSound::Playback::Module::Io::OutputBuffer>(
            &lpPlaybackOutputBuffer);
        lpInputBufferStack->DestroyIOBuffer<CgsSound::Playback::Module::Io::InputBuffer>(
            &lpPlaybackInputBuffer);
        lpOutputBufferStack->DestroyIOBuffer<Io::LogicOutputBuffer>(&lpLogicOutputBuffer);
        return lbPrepared;
    }

    // X360 0x826EB928 (bodied 2026-08-25, faithful-audio-engine phase C1). The
    // per-frame PRE-update chain: scratch LogicPreUpdateOutputBuffer -> the logic
    // module publishes its block -> the (locked) bridge copy into the caller's
    // root pre-update output -> destroy the scratch.
    void RootSoundModule::PreUpdate(CgsModule::IOBufferStack* lpOutputBufferStack,
                                    Io::RootPreUpdateOutputBuffer* lpRootPreUpdateOutput)
    {
        CGS_ASSERT(lpOutputBufferStack != 0, "0 != lpOutputBufferStack");
        CGS_ASSERT(lpRootPreUpdateOutput != 0, "0 != lpRootPreUpdateOutput");

        Io::LogicPreUpdateOutputBuffer* lpLogicPreUpdateOutput = 0;
        lpOutputBufferStack->CreateIOBuffer<Io::LogicPreUpdateOutputBuffer>(
            &lpLogicPreUpdateOutput, "SoundLogicPreUpdateOutput");

        mLogicModule.PreUpdate(lpLogicPreUpdateOutput);

        lpLogicPreUpdateOutput->LockForRead();
        lpRootPreUpdateOutput->LockForWrite();
        BridgeLogicToRootPreUpdate(lpLogicPreUpdateOutput, lpRootPreUpdateOutput);
        lpRootPreUpdateOutput->UnlockForWrite();
        lpLogicPreUpdateOutput->UnlockForRead();

        lpOutputBufferStack->DestroyIOBuffer<Io::LogicPreUpdateOutputBuffer>(&lpLogicPreUpdateOutput);
    }

    // DWARF BrnRootSoundModule.h:163 (phase C1) -- the copy step the console
    // inlines in PreUpdate: the (ICF-folded Root) GetPreUpdateOutput read + the
    // SetPreUpdateOutput @0x826E0C10 copy.
    void RootSoundModule::BridgeLogicToRootPreUpdate(
        const Io::LogicPreUpdateOutputBuffer* lpLogicPreUpdateOutput,
        Io::RootPreUpdateOutputBuffer* lpRootPreUpdateOutput) const
    {
        lpRootPreUpdateOutput->SetPreUpdateOutput(lpLogicPreUpdateOutput->GetPreUpdateOutput());
    }

    // X360 0x826FB238 (bodied 2026-08-25, faithful-audio-engine phase C2). The
    // per-frame pump, console step-for-step:
    //   [1] asserts cpp:880-883; the three per-call scratch buffers (logic output
    //       "SoundLogic"; playback in/out "SoundPlayback").
    //   [2] miLogicUpdate monitor bracket around the LOGIC module Update (the
    //       engine virtual vtbl+0x5C: game/sim dts + the ROOT input buffer as its
    //       input and the logic-output scratch as its output).
    //   [3] BridgeLogicToRoot under the console's lock set -- logic-out READ,
    //       root-out WRITE, playback-in WRITE (the console brackets the playback
    //       input for the frame even though this build's bridge writes none of
    //       it), unlocked in the console's exact order.
    //   [4] the playback time-pair store (AdvanceTime: mf32TimeStep = gameDt,
    //       mf32TotalTime += gameDt) then the playback Module::Update virtual
    //       (vtbl+0x48) with the playback scratch pair.
    //   [5] root-out WRITE + playback-out READ: append the playback output's
    //       resource-request queue into the root output's 4096 interface and its
    //       freed-stream-buffer ids into the logic module's freed-id list
    //       (Array<u32,3>::AppendArray, X360 root+0x5460).
    //   [6] the command-ring high-water assert ("Command buffer high water mark.
    //       Possibly SPU crashed." cpp:977; muDeferredRingHighWater >= 157286 of
    //       the 0x30000 ring).
    //   [7] the debug tail -- Debug::Statistics::Update, the three TestBed
    //       SanityChecks (logic/playback banks + the RWAC bank under the system
    //       lock), and the one-shot registry/memory dump flag. [gated] the
    //       TestBed SanityCheck body and the Statistics::Update surface are their
    //       own recon slices (the interim TestBed pass-through tracks nothing to
    //       check), and the dump one-shot (X360 dword_82FFB814 +
    //       DebugAudioMemoryDump) is debug-page surface; documented, not run.
    //   [8] destroy the scratches (playback in, playback out, logic out).
    // The trailing BrnUpdateSet is carried but unread, exactly as the console
    // body never touches its copy.
    void RootSoundModule::Update(f32 af32GameTimeStep, f32 af32SimTimeStep,
                                 CgsModule::IOBufferStack* lpInputBufferStack,
                                 CgsModule::IOBufferStack* lpOutputBufferStack,
                                 Io::RootInputBuffer* lpSoundModuleInputBuffer,
                                 Io::RootOutputBuffer* lpSoundModuleOutputBuffer,
                                 BrnUpdateSet /*leUpdateSet*/)
    {
        CGS_ASSERT(lpInputBufferStack != 0, "lpInputBufferStack != NULL");
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");
        CGS_ASSERT(lpSoundModuleInputBuffer != 0, "lpSoundModuleInputBuffer != NULL");
        CGS_ASSERT(lpSoundModuleOutputBuffer != 0, "lpSoundModuleOutputBuffer != NULL");

        Io::LogicOutputBuffer* lpLogicOutputBuffer = 0;
        CgsSound::Playback::Module::Io::InputBuffer* lpPlaybackInputBuffer = 0;
        CgsSound::Playback::Module::Io::OutputBuffer* lpPlaybackOutputBuffer = 0;
        lpOutputBufferStack->CreateIOBuffer<Io::LogicOutputBuffer>(&lpLogicOutputBuffer, "SoundLogic");
        lpInputBufferStack->CreateIOBuffer<CgsSound::Playback::Module::Io::InputBuffer>(
            &lpPlaybackInputBuffer, "SoundPlayback");
        lpOutputBufferStack->CreateIOBuffer<CgsSound::Playback::Module::Io::OutputBuffer>(
            &lpPlaybackOutputBuffer, "SoundPlayback");

        CgsDev::PerfMonCpu::StartMonitor(miLogicUpdate);
        mLogicModule.Update(af32GameTimeStep, af32SimTimeStep,
                            lpSoundModuleInputBuffer, lpLogicOutputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(miLogicUpdate);

        lpLogicOutputBuffer->LockForRead();
        lpSoundModuleOutputBuffer->LockForWrite();
        lpPlaybackInputBuffer->LockForWrite();
        BridgeLogicToRoot(lpLogicOutputBuffer, lpSoundModuleOutputBuffer);
        lpLogicOutputBuffer->UnlockForRead();
        lpSoundModuleOutputBuffer->UnlockForWrite();
        lpPlaybackInputBuffer->UnlockForWrite();

        mLogicModule.GetPlaybackModule().AdvanceTime(af32GameTimeStep);
        mLogicModule.GetPlaybackModule().Update(lpPlaybackInputBuffer, lpPlaybackOutputBuffer);

        lpSoundModuleOutputBuffer->LockForWrite();
        lpPlaybackOutputBuffer->LockForRead();
        lpSoundModuleOutputBuffer->GetResourceRequestInterface()->mRequestQueue.Append(
            lpPlaybackOutputBuffer->GetResourceRequestQueue());
        mLogicModule.GetFreedStreamBufferIds().AppendArray(
            lpPlaybackOutputBuffer->GetStreamBuffersFreed());
        lpPlaybackOutputBuffer->UnlockForRead();
        lpSoundModuleOutputBuffer->UnlockForWrite();

        CGS_ASSERT(mpSystem == 0 || mpSystem->muDeferredRingHighWater < 157286u,
                   "Command buffer high water mark. Possibly SPU crashed.");

        // [7] the debug tail -- see the banner (gated recon slices; not run).

        lpInputBufferStack->DestroyIOBuffer<CgsSound::Playback::Module::Io::InputBuffer>(
            &lpPlaybackInputBuffer);
        lpOutputBufferStack->DestroyIOBuffer<CgsSound::Playback::Module::Io::OutputBuffer>(
            &lpPlaybackOutputBuffer);
        lpOutputBufferStack->DestroyIOBuffer<Io::LogicOutputBuffer>(&lpLogicOutputBuffer);
    }

    // X360 0x826EBF18 (bodied phase C1). The logic -> root output bridge: the two
    // request-queue appends (4096 resource / 2048 AttribSys -- the same pairs
    // SoundLogicModule::ResourceBridging fills) + the replay request-interface
    // merge (BrnReplays::ReplayIO::RequestInterface::Append @0x823A6868).
    void RootSoundModule::BridgeLogicToRoot(const Io::LogicOutputBuffer* lpLogicOutputBuffer,
                                            Io::RootOutputBuffer* lpRootOutputBuffer)
    {
        lpRootOutputBuffer->GetResourceRequestInterface()->mRequestQueue.Append(
            lpLogicOutputBuffer->GetResourceRequestInterface()->mRequestQueue);

        lpRootOutputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpLogicOutputBuffer->GetAttribSysRequestInterface()->mRequestQueue);

        lpRootOutputBuffer->GetReplayRequestInterface()->Append(
            lpLogicOutputBuffer->GetReplayRequestInterface());
    }
}
}
