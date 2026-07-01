#include "GameSource/Sound/Module/BrnRootSoundModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor (START stage)
#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"            // CgsSound::TestBed::Allocator (the four carve globals)

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
}

namespace BrnSound
{
namespace Module
{
    // DWARF BrnRootSoundModule.cpp:133 (X360 dword_82FFB818).
    s32 RootSoundModule::msiMutexLockCount = 0;

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
        //     (mLogicModule's CgsSound::Logic::Module engine base embeds it at +0x238) with
        //     module-id 6. [gated] the SoundLogicModule slice does not model that engine base
        //     yet, so there is no playback member to reach; lands with the playback-module TU.

        // [4] (*(vtbl(this+0x280)))(this+0x280) -- the logic module's Construct (vtable slot 0).
        mLogicModule.Construct();

        // [5]+[6] BrnSound::Debug::DebugComponent::Construct(&mDebugComponent, this+0x4B8,
        //     this+0x280) + CgsDev::DebugComponent::Register(&mDebugComponent) -- wire the sound
        //     debug pages to the playback + logic modules and register the component. [gated]
        //     Construct(playback, logic) is not in the DebugComponent slice (no export was
        //     dumped for it) and its playback arg does not exist yet (step [3]); both land
        //     together.

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

        // Per-call scratch IO buffers. The X360 creates three:
        //   Io::LogicOutputBuffer                       on the OUTPUT stack ("SoundLogic")
        //   CgsSound::Playback::Module::Io::InputBuffer  on the INPUT stack  ("SoundPlayback")
        //   CgsSound::Playback::Module::Io::OutputBuffer on the OUTPUT stack ("SoundPlayback"),
        //     then Clear()s it (VariableEventQueue<4096,16>::Clear on its queue + zero its count).
        // [gated] the playback Io pair's types are not reconstructed (they land with the
        // playback-module TU); only the logic output scratch is created here.
        Io::LogicOutputBuffer* lpLogicOutputBuffer = 0;
        lpOutputBufferStack->CreateIOBuffer<Io::LogicOutputBuffer>(&lpLogicOutputBuffer, "SoundLogic");

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
            mePrepareStage = E_PREPARESTAGE_RWAC;
            // [gated] the RWAC + Csis bring-up. X360 body (PS3 names in brackets):
            //   * carve bank 0x18 from lpAllocatorList (GetRWGeneralResource 0x823F3F98), put the
            //     linear heap in no-coalesce mode (rw::LinearResourceAllocator::GetLinearHeapBase
            //     + EA::Allocator::GeneralAllocator::SetOption(1,0)), SetAllocator(gRwacTestBed-
            //     Alloc); rw::audio::core::System::CreateInstance(&gRwacTestBedAlloc, 196608) ->
            //     mpSystem (assert "mpSystem", cpp:336); gRwacTestBedAlloc.mbTestRwac = 1.
            //   * carve bank 9, build the "CsisPrivateHeap" sub-allocator (GetResourceDescriptor
            //     0x2000/4 + the allocator's virtual CreateAllocator + Initialize), SetAllocator
            //     (gCsisTestBedAlloc), Csis::System::SetAllocator + Csis::System::Init.
            //   * rw::audio::core::System::VectorToCsisMutex(mpSystem); install MutexLockFn/
            //     MutexUnlockFn/MutexIsLockedFn at mpSystem+0x40/+0x44/+0x3C; Lock(); the DAC
            //     watermark *(mpSystem+0x10CC) = 100.0; SetThreadProcessor(4); Unlock().
            //   * rw::audio::core::SndPlayer1_CgsStreamMod::spPathPrefix = "SOUND\\STREAMS\\".
            // Blocked on: the rw::audio::core System/runtime reconstruction (no vendor System.h;
            // bodies are console-only middleware, no PC lib) + Csis::System::SetAllocator + a
            // populated AllocatorList (the game-data CreateAllocators is still the allocator
            // gate). Until then this stage completes without creating the audio system
            // (mpSystem stays null -- the boot's audio output runs through the PC movie/XAudio2
            // path instead).
            meReleaseStage = E_RELEASESTAGE_RWAC;
            // fall through
        case E_PREPARESTAGE_PLAYBACK_MODULE:
            mePrepareStage = E_PREPARESTAGE_PLAYBACK_MODULE;
            // [gated] X360: carve bank 9 -> gPlaybackTestBedAlloc; rw::audio::core::System::Lock
            //   (mpSystem); virtual Prepare on the playback module (this+0x4B8, vtable +0x44 =
            //   CgsSound::Playback::Module::Prepare 0x826E90C0) with (&gPlaybackTestBedAlloc,
            //   lpAllocatorList->mpPlaybackParams @ +660); Unlock. A false return unwinds to the
            //   common exit (still preparing). Blocked on: the playback module's Prepare/vtable
            //   (the CgsSoundPlaybackModule slice has neither) + the logic module's engine base
            //   that embeds it. Until then this stage completes without preparing playback.
            meReleaseStage = E_RELEASESTAGE_PLAYBACK_MODULE;
            // fall through
        case E_PREPARESTAGE_REGISTRY_LOAD:
            mePrepareStage = E_PREPARESTAGE_REGISTRY_LOAD;
            // [gated] X360: if (!RegistryLoad(this, lpLogicOutputBuffer)) { LockForWrite(
            //   lpSoundModuleOutputBuffer); LockForRead(lpLogicOutputBuffer); BridgeLogicToRoot
            //   (lpLogicOutputBuffer, lpSoundModuleOutputBuffer); unlock both; -> still
            //   preparing }. RegistryLoad (0x826EBA08) streams the CSIS/AEMS registries through
            //   the playback module; blocked on the playback stage above + the RootOutputBuffer
            //   request interfaces. Until then this stage completes without loading registries.
            meReleaseStage = E_RELEASESTAGE_REGISTRY_LOAD;
            // fall through
        case E_PREPARESTAGE_LOGIC_MODULE:
        {
            mePrepareStage = E_PREPARESTAGE_LOGIC_MODULE;
            // [gated] the allocator carve: X360 carves bank 0x19 from lpAllocatorList, puts the
            // linear heap in no-coalesce mode and points gLogicTestBedAlloc at it
            // (SetAllocator). Blocked on a populated AllocatorList (allocator gate);
            // gLogicTestBedAlloc is still handed to the logic module below so the wiring shape
            // is the X360's.
            (void)lpAllocatorList;

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
        // playback output, the playback input, then the logic output ([gated] only the logic
        // output exists here) -- and report the machine's verdict.
        lpOutputBufferStack->DestroyIOBuffer<Io::LogicOutputBuffer>(&lpLogicOutputBuffer);
        return lbPrepared;
    }
}
}
