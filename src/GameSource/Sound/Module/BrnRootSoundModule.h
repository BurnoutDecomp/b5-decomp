#ifndef BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H
#define BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"             // CgsModule::IOBufferStack (Prepare args + scratch buffers)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"    // CgsModule::EventReceiverQueue (mReceiverQueue)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"    // SoundLogicModule (mLogicModule, embedded by value)
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"               // Io::RootInputBuffer / Io::RootOutputBuffer (Prepare args)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"  // Io::LogicOutputBuffer (Prepare scratch + bridge source)
#include "GameSource/Sound/BrnDebugComponent.h"                         // BrnSound::Debug::DebugComponent (mDebugComponent)

// Forward declaration: the game-data allocator list Prepare carves the sound heaps from
// (BrnResource::GameDataIO::AllocatorList, home GameSource/Resource/SharedIO/
// BrnGameDataAllocatorList.h). Pointer-only in this header; including the GameData IO
// header here would pull the resource-IO cascade into every sound include.
namespace BrnResource { namespace GameDataIO { class AllocatorList; } }

// Forward declaration: the RWAudio core runtime system (rw::audio::core::System; its
// vendor home is PlugIn.h -- the .cpp includes it for the RWAC bring-up, which is REAL
// since 2026-08-25, faithful-audio-engine phase A4; this header stays pointer-only to
// keep the vendor cascade out of every sound include).
namespace rw { namespace audio { namespace core { class System; } } }

// BrnSound::Module::RootSoundModule : public CgsModule::ModuleSingleBuffered -- the root sound
// module: the thin orchestrator over the real audio engine (the CgsSound::Playback module embedded
// in the logic module's engine base + the BrnSound SoundLogicModule). Shape from the DecFIGS DWARF
// (BrnRootSoundModule.h:107, gated on the X360 ledger); behaviour from BURNOUT_X360_ARTIST.XEX:
//   ctor            0x827E4808  (base + mutexes via base ctor; SoundLogicModule @ +0x280;
//                                Debug::DebugComponent @ +0x14D38)
//   Construct       0x826AF350  (vtable slot 0)
//   Prepare         0x826FABF8  (vtable +64; the 6-arg load entry LoadingScriptedState::
//                                LoadSoundModule (0x823E75A8) calls each frame until it
//                                returns true)
//   Release         0x82682898 / Destruct 0x826829C8 / Update 0x826FB238 / PreUpdate 0x826EB928
//                                (not on the boot path; their TU passes reconstruct them)
namespace BrnSound
{
namespace Module
{
    class RootSoundModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // DWARF BrnRootSoundModule.h:111 (the original's "RECIEVER" spelling is preserved).
        static const u32 KU_RECIEVER_QUEUE_SIZE = 5120;

        // DWARF BrnRootSoundModule.h:115. Prepare's resumable stage machine, persisted in
        // mePrepareStage (X360 this+0x14D1C). The EXECUTION order is 0,1,2,3,6,4,7 -- the
        // registry load runs between the playback and logic modules; E_PREPARESTAGE_RESOURCES(5)
        // is not visited by Prepare (it belongs to the PrepareOnEnteringGameplay machine,
        // 0x82704188). Prepare runs forward from the persisted stage each call until a stage
        // reports "still preparing" (returns false -> the loader retries next frame) or all
        // complete.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START           = 0,  // AddMonitor("Sound Logic")            [real]
            E_PREPARESTAGE_SELF            = 1,  // ModuleSingleBuffered::Prepare        [real]
            E_PREPARESTAGE_RWAC            = 2,  // rw::audio::core::System + Csis       (REAL, phase A4)
            E_PREPARESTAGE_PLAYBACK_MODULE = 3,  // CgsSound::Playback::Module::Prepare  [gated]
            E_PREPARESTAGE_LOGIC_MODULE    = 4,  // SoundLogicModule::Prepare + bridges  [real; carve REAL phase A4]
            E_PREPARESTAGE_RESOURCES       = 5,  // (PrepareOnEnteringGameplay only)
            E_PREPARESTAGE_REGISTRY_LOAD   = 6,  // RootSoundModule::RegistryLoad        [gated]
            E_PREPARESTAGE_DONE            = 7,
        };

        // DWARF BrnRootSoundModule.h:129. Release's stage machine (X360 this+0x14D20). Prepare
        // maintains it as the "last completed prepare stage" so a partial Prepare can be unwound
        // from the right rung: Construct seeds it DONE(7), each completed prepare stage stamps
        // its own id, and a fully-prepared module resets it to START(0).
        enum EReleaseStage
        {
            E_RELEASESTAGE_START           = 0,
            E_RELEASESTAGE_SELF            = 1,
            E_RELEASESTAGE_RWAC            = 2,
            E_RELEASESTAGE_PLAYBACK_MODULE = 3,
            E_RELEASESTAGE_LOGIC_MODULE    = 4,
            E_RELEASESTAGE_RESOURCES       = 5,
            E_RELEASESTAGE_REGISTRY_LOAD   = 6,
            E_RELEASESTAGE_DONE            = 7,
        };

        // DWARF BrnRootSoundModule.h:143. The global-sound-resource acquire ladder
        // (PrepareOnEnteringGameplay's E_PREPARESTAGE_RESOURCES stage walks it).
        enum EResourceAcquireStage
        {
            E_RESOURCE_LOAD_NOT_STARTED    = 0,
            E_RESOURCE_LOAD_REQUESTED      = 1,
            E_RESOURCE_ACQUIRE_NOT_STARTED = 2,
            E_RESOURCE_ACQUIRE_REQUESTED   = 3,
            E_RESOURCE_ACQUIRE_COUNT       = 4,
        };

        // 0x827E4808. Base + members construct themselves (the base ctor installs the vtable and
        // default-constructs the two RWMutexes; mLogicModule and mDebugComponent run their own
        // ctors). The X360 ctor initialises NOTHING else -- the state fields are seeded by
        // Construct(), which always runs before use on the module path.
        RootSoundModule();

        // 0x826AF350 -- bring the module to the constructed state (see the .cpp for the
        // step-by-step X360 mapping).
        void Construct() override;

        // 0x826FABF8 (vtable +64). The loading-screen load entry -- DWARF-true signature
        // (BrnRootSoundModule.cpp:249):
        //   Prepare(const AllocatorList*, IOBufferStack* in, IOBufferStack* out,
        //           RootInputBuffer*, RootOutputBuffer*)
        // Returns true when fully prepared; false while still preparing (the caller forwards the
        // module's resource requests out of the RootOutputBuffer and retries next frame). A NEW
        // virtual (the no-arg CgsModule::Module::Prepare stays at its own slot); hides the base
        // overload by design, as on the X360 vtable.
        virtual bool Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                             CgsModule::IOBufferStack* lpInputBufferStack,
                             CgsModule::IOBufferStack* lpOutputBufferStack,
                             Io::RootInputBuffer* lpSoundModuleInputBuffer,
                             Io::RootOutputBuffer* lpSoundModuleOutputBuffer);

    private:
        // --- private helpers (DWARF BrnRootSoundModule.cpp / BrnRootSoundModuleBridges.cpp) ---

        // X360 0x826EBA08 (no per-function export was dumped -- the address is the `bl` target
        // in Prepare's REGISTRY_LOAD stage). Streams the CSIS/AEMS registry files (the file-scope
        // RegistryBootInfo table, DWARF BrnRootSoundModule.cpp:1029) into the playback module,
        // returning false while a registry is still loading. [gated: needs the playback module +
        // resource-request path; declared for shape, no body yet -- nothing calls it while the
        // REGISTRY_LOAD stage is gated.]
        bool RegistryLoad(Io::LogicOutputBuffer* lpLogicOutputBuffer);

        // X360 0x826EBF18 (DWARF BrnRootSoundModuleBridges.cpp:88). Appends the logic output's
        // results queue (VariableEventQueue<4096,16>), its AttribSys queue (<2048,16>) and its
        // replay request block into the RootOutputBuffer's interfaces. [gated: RootOutputBuffer
        // is still the minimal slice without its request-interface members/getters; declared for
        // shape, no body yet -- only the gated REGISTRY_LOAD/LOGIC retry paths call it.]
        void BridgeLogicToRoot(const Io::LogicOutputBuffer* lpLogicOutputBuffer,
                               Io::RootOutputBuffer* lpRootOutputBuffer);

        // The Csis mutex callbacks the RWAC stage installs into mpSystem (+0x3C/+0x40/+0x44):
        //   MutexLockFn     0x82682A20  { rw::audio::core::CsisMutexLock();   ++msiMutexLockCount; }
        //   MutexUnlockFn   (ICF-folded on X360; PS3 0x8D0570 installs it @ +0x44)
        //                   { rw::audio::core::CsisMutexUnlock(); --msiMutexLockCount; }
        //   MutexIsLockedFn 0x82682A68  { return msiMutexLockCount > 0; }
        // Static (plain function pointers are stored into the C callback slots). [gated: the
        // rw::audio::core CsisMutex entry points land with the RWAC stage; declared for shape,
        // no bodies yet -- nothing references them until the RWAC stage is real.]
        static void MutexLockFn();
        static void MutexUnlockFn();
        static bool MutexIsLockedFn();

        // --- members, in DWARF source order (BrnRootSoundModule.h:224-259) ---
        // [gated] DWARF :224 `JobScheduler mScheduler` is OMITTED: the JobScheduler type is not
        // reconstructed yet and none of the boot-path functions touch it. Restore it at the head
        // of the member list when its type lands (semantic parity is by named members, not byte
        // offsets, so inserting it later moves nothing logically).

        // DWARF :228. The embedded sound-logic sub-module (X360 this+0x280). Its engine base
        // (CgsSound::Logic::Module) embeds the playback module at +0x238 -- X360 this+0x4B8 --
        // which is what Construct virtual-inits and the PLAYBACK_MODULE stage prepares. That
        // base is not in the SoundLogicModule slice yet, so those touches are gated there.
        SoundLogicModule mLogicModule;

        // DWARF :229 (X360 this+0x13900). Construct() performs the X360 capacity(5120)/align(16)/
        // Clear sequence.
        CgsModule::EventReceiverQueue<KU_RECIEVER_QUEUE_SIZE, 16> mReceiverQueue;

        // DWARF :230 (X360 this+0x14D18). The RWAC runtime instance the RWAC stage creates
        // (rw::audio::core::System::CreateInstance, X360 0x82B6F420). Null until that stage lands.
        rw::audio::core::System* mpSystem;

        EPrepareStage         mePrepareStage;      // DWARF :231 (X360 this+0x14D1C)
        EReleaseStage         meReleaseStage;      // DWARF :232 (X360 this+0x14D20)
        EResourceAcquireStage meResourceStage;     // DWARF :233 (X360 this+0x14D24)
        u32                   mu32CurrentRegistry; // DWARF :241 (X360 this+0x14D28)
        u32                   meCurrentRegistry;   // DWARF :242 (X360 this+0x14D2C)

        // DWARF :246/:247 (X360 this+0x14D30/+0x14D34). The "Sound Logic" / "Bridge Root To
        // Logic" CPU perfmon handles. Written by Prepare's START stage / Update before first
        // read (the X360 never zero-seeds them in the ctor or Construct).
        s32 miLogicUpdate;
        s32 miBridgeRootToLogic;

        // DWARF :259 (X360 this+0x14D38). Construct registers it with the debug system.
        Debug::DebugComponent mDebugComponent;

        // DWARF BrnRootSoundModule.cpp:133 (X360 dword_82FFB818). The Csis mutex-callback
        // lock counter.
        static s32 msiMutexLockCount;
    };
}
}

// The game's single RootSoundModule (a BrnGameModule member). Mirrors BrnGame::GetMainGameDataModule()
// so the loading-screen flow can reach the module without the BrnGameModule mega-header.
namespace BrnGame { BrnSound::Module::RootSoundModule* GetMainSoundModule(); }

#endif // BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H
