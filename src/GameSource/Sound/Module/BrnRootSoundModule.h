#ifndef BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H
#define BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"    // CgsModule::EventReceiverQueue (mEventQueue)

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
        RootSoundModule() : mbConstructed(false), mbPrepared(false), mpAllocator(0) {}

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
        // X360 this+0x13900: the module's event-receiver queue (capacity 0x1400 = 5120, align 16).
        // Real + faithful with existing types; Construct() does the X360 capacity/align/Clear sequence.
        CgsModule::EventReceiverQueue<5120, 16> mEventQueue;
    };
}
}

// The game's single RootSoundModule (a BrnGameModule member). Mirrors BrnGame::GetMainGameDataModule()
// so the loading-screen flow can reach the module without the BrnGameModule mega-header.
namespace BrnGame { BrnSound::Module::RootSoundModule* GetMainSoundModule(); }

#endif // BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_H
