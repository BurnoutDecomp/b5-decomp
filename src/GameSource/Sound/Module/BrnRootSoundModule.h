#ifndef BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H
#define BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"    // CgsModule::EventReceiverQueue (mEventQueue)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"    // SoundLogicModule (embedded sub-module)
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"               // Io::LogicInputBuffer (logic Prepare input scratch)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"  // Io::LogicOutputBuffer (logic Prepare output scratch)
#include "GameSource/Sound/BrnDebugComponent.h"

namespace rw { namespace core { struct GeneralResourceAllocator; } }  // Prepare's allocator arg

// BrnSound::Module::RootSoundModule : public CgsModule::ModuleSingleBuffered -- the root sound module,
// the thin orchestrator the loading screen brings up over the real audio engine (the Playback +
// Logic sub-modules). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   RootSoundModule::Construct 0x826AF350  (base Construct + field init + a Debug component + an
//                                           event-receiver queue; sets the constructed flag *(this+4)=1)
//   RootSoundModule::Prepare   0x826FABF8  (vtable+64; the 6-arg load entry the loading screen's
//                                           LoadSoundModule calls -- allocator + the two Root IO
//                                           buffers + their data. A resumable stage machine: returns
//                                           false while still preparing, true when fully prepared.)
//
// MINIMAL-THEN-GROW: Construct marks the module constructed; Prepare reports "prepared" immediately so
// the loading-screen stage advances. The real audio engine -- the CgsSound::Playback::Module +
// CgsSound::Logic / BrnSound::Module::SoundLogicModule sub-modules, the per-allocator memory carves,
// the Debug component, and the resource-request forwarding the X360 does on the "still preparing"
// path -- is grown incrementally on top of this (see allocator-gate-and-modules memory).
namespace BrnSound
{
namespace Module
{
    class RootSoundModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        RootSoundModule();

        // The resumable Prepare stages (X360 mePrepareStage @ this+0x14D20). Prepare runs forward
        // from the persisted stage each frame until a stage reports "still preparing" (returns false)
        // or all complete. X360 case order is 0,1,2,3,6,4,7 (the registry between playback and logic).
        enum EPrepareStage
        {
            E_PREPSTAGE_PERFMON = 0,  // X360 0 -- PerfMon "Sound Logic" monitor          [grow-in]
            E_PREPSTAGE_BASE,         // X360 1 -- ModuleSingleBuffered::Prepare           [real]
            E_PREPSTAGE_AUDIOSYSTEM,  // X360 2 -- rw::audio::core::System + Csis::System   [grow-in]
            E_PREPSTAGE_PLAYBACK,     // X360 3 -- CgsSound::Playback::Module::Prepare      [grow-in]
            E_PREPSTAGE_REGISTRY,     // X360 6 -- RootSoundModule::RegistryLoad            [grow-in]
            E_PREPSTAGE_LOGIC,        // X360 4 -- SoundLogicModule::Prepare + BridgeLogicToRoot [grow-in]
            E_PREPSTAGE_DONE          // X360 7 -- fully prepared
        };

        // 0x826AF350 -- bring the module to the constructed state.
        void Construct();

        // 0x826FABF8 (vtable+64) -- the loading-screen load entry. Returns true when fully prepared
        // (the stage advances), false while still preparing (the loader forwards the module's resource
        // requests and retries next frame). The IO-buffer args are opaque here (the minimal Prepare
        // does not consume them yet); they regain their real RootInputBuffer*/RootOutputBuffer* types
        // when Prepare wires the audio engine (S6).
        bool Prepare(rw::core::GeneralResourceAllocator* lpAllocator,
                     void* lpRootInputBuffer, void* lpRootOutputBuffer,
                     void* lpInputData, void* lpOutputData);

    private:
        bool                                mbConstructed;   // X360 *(this+4) constructed flag
        bool                                mbPrepared;
        rw::core::GeneralResourceAllocator* mpAllocator;     // captured from Prepare for the grow-in audio engine
        s32                                 mePrepareStage;  // X360 this+0x14D20 -- resumable Prepare stage
        // X360 this+0x13900: the module's event-receiver queue (capacity 0x1400 = 5120, align 16).
        // Real + faithful with existing types; Construct() does the X360 capacity/align/Clear sequence.
        CgsModule::EventReceiverQueue<5120, 16> mEventQueue;

        // The embedded sound-logic sub-module (X360: SoundLogicModule lives inside RootSoundModule;
        // its sub-objects @ +0x4B8/+0x280 are the ones RootSoundModule::Construct virtual-inits).
        // Construct() brings it up (which brings up its embedded ResourceRegistrar); the LOGIC
        // Prepare stage drives its Prepare with the scratch IO buffers below.
        SoundLogicModule mLogicModule;
        BrnSound::Debug::DebugComponent mDebugComponent;

        // Scratch IO buffers the LOGIC Prepare stage hands to mLogicModule.Prepare. The X360
        // creates these per Prepare call on the IOBufferStack -- CreateIOBuffer<LogicOutputBuffer>
        // (..,"SoundLogic") for the output (v42/v15); the input is the caller's lpSoundModuleInputBuffer
        // (a5). The minimal boot caller (BrnGameMainFlowStates LoadSoundModule) passes null IO buffers,
        // so the module owns its own constructed scratch buffers to satisfy SoundLogicModule::Prepare's
        // non-null input/output asserts. FLAG: owned-scratch stand-in -- the faithful path routes
        // per-frame buffers through the IOBufferStack + the playback->logic bridge (grow-in, stages 2-3),
        // and destroys them per call (X360 LABEL_24/26); the owned buffers stay live across frames here.
        Io::LogicInputBuffer  mLogicInputScratch;
        Io::LogicOutputBuffer mLogicOutputScratch;
    };
}
}

// The game's single RootSoundModule (a BrnGameModule member). Mirrors BrnGame::GetMainGameDataModule()
// so the loading-screen flow can reach the module without the BrnGameModule mega-header.
namespace BrnGame { BrnSound::Module::RootSoundModule* GetMainSoundModule(); }

#endif // BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H
