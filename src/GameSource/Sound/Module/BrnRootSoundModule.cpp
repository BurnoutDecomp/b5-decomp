#include "GameSource/Sound/Module/BrnRootSoundModule.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / Message filter

// BrnSound::Module::RootSoundModule -- see the header. Minimal-then-grow reconstruction of the root
// sound module's Construct (0x826AF350) + Prepare (0x826FABF8). The real audio engine is grown on top.

namespace BrnSound
{
namespace Module
{
    // 0x827E4808. The X360 constructor, with the base ctor inlined by Hex-Rays:
    //   *a1 = &off_820CE500                      -> base ModuleSingleBuffered subobject ctor:
    //   RWMutex(a1 + 4,  0, 1)                      installs the base vtable and default-constructs
    //   RWMutex(a1 + 70, 0, 1)                      mInputMutex (+0x10) and mOutputMutex (+0x118)
    //                                               with RWMutex(NULL, true).
    //   *a1 = off_820D1100                       -> our own vtable (emitted implicitly).
    //   SoundLogicModule(a1 + 160)               -> mLogicModule   (embedded @ +0x280).
    //   DebugComponent(a1 + 21326)               -> mDebugComponent (embedded @ +0x14D38).
    // The two mutexes and both vtable stores are produced by the base subobject + member
    // construction; only the bookkeeping fields below (used by the grown Construct/Prepare path)
    // are seeded here.
    RootSoundModule::RootSoundModule()
        : mbConstructed(false)
        , mbPrepared(false)
        , mpAllocator(nullptr)
        , mePrepareStage(E_PREPSTAGE_PERFMON)
    {
    }

    // 0x826AF350. The X360 ctor-path: base ModuleSingleBuffered::Construct, zero a block of state
    // fields, init two embedded sub-objects (+0x4B8 / +0x280), build + register a Debug component,
    // construct an event-receiver queue (+0x13900), and set the constructed flag *(this+4)=1.
    // [minimal] mark the module constructed; the sub-objects + debug component + event queue are
    // grown with the audio engine (S6).
    void RootSoundModule::Construct()
    {
        // Faithful order from 0x826AF350 (steps that have a real PC backing are implemented; the
        // uncommitted ones are marked grow-ins):
        //   [skip] X360 step 1 (BaseCollisionGenerator::Destruct(3)) is a Hex-Rays/ICF artifact
        //          (r3=3 constant, not `this`) -- not a real call.

        // [step 3] base module construct (resets stages, builds the data buffers).
        CgsModule::ModuleSingleBuffered::Construct();

        // [steps 4-5] GROW-IN: the X360 virtual-inits two embedded sub-objects -- this+0x4B8
        //   (vtable slot 16, arg 6) and this+0x280 (slot 0) -- which live inside the embedded
        //   SoundLogicModule (~79KB) the minimal RootSoundModule does not yet mirror. Added with
        //   the audio engine (the Playback/Logic sub-modules).
        // [steps 6-7] GROW-IN: BrnSound::Debug::DebugComponent::Construct(this+0x4D38, +0x4B8,
        //   +0x280) + CgsDev::DebugComponent::Register -- the PC BrnDebugComponent only models a
        //   default ctor (no Construct(a,b)); wired with the sub-objects above.

        // [step 8] the event-receiver queue @ X360 this+0x13900 (capacity 5120, align 16). Real +
        //   faithful: Construct() performs the X360 capacity/alignment/Clear sequence.
        mEventQueue.Construct();

        // [steps 4-5, partial] bring up the embedded SoundLogicModule -- the X360 virtual-inits its
        //   sub-objects (+0x4B8/+0x280) as part of RootSoundModule::Construct. This runs the embedded
        //   ResourceRegistrar's bring-up (its request queues + requested/queued pools go live).
        mLogicModule.Construct();

        // Bring up the scratch logic IO buffers (CgsModule::IOBuffer requires Construct before use).
        // These back the LOGIC Prepare stage's non-null input/output -- the boot caller passes null
        // (see the scratch-buffer note in the header). X360: the equivalent buffers are CreateIOBuffer'd
        // on the IOBufferStack inside Prepare; the minimal model constructs owned buffers up front.
        mLogicInputScratch.Construct();
        mLogicOutputScratch.Construct();

        // [steps 2,9] GROW-IN: trailing state fields at X360 +0x14D18 / +0x14D1C..+0x14D2C (one
        //   seeded to 7) -- no PC members for them in the minimal layout yet.

        // [step 9] X360 seeds the Prepare stage word @ +0x14D20 to 0 (start of the resumable machine).
        mePrepareStage = E_PREPSTAGE_PERFMON;

        // [step 10] the constructed flag (X360 *(this+4)=1).
        mbConstructed = true;

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] RootSoundModule::Construct\n";
    }

    // 0x826FABF8 (vtable+64). The X360 stage machine constructs the playback + logic audio modules
    // from the allocator, wires the IO buffers, and returns false until fully prepared. [minimal]
    // capture the allocator for the grow-in engine; the audio-system/playback/registry stages still
    // advance immediately, while the LOGIC stage now drives the embedded SoundLogicModule::Prepare.
    // Faithful resumable stage machine: each frame it runs forward from the
    // persisted mePrepareStage, falling through the stages until one reports "still preparing"
    // (returns false -> the loading screen retries next frame) or all complete (returns true). The
    // X360 also brackets each call with three scratch IO buffers (LogicOutputBuffer + Playback In/Out)
    // used by the playback/logic stages; the logic input/output pair is modelled as owned scratch
    // buffers (see header). Stage order mirrors the X360 (0,1,2,3,6,4,7). Stages 2/3/6 are guarded
    // GROW-IN stubs that advance immediately until their engines (rw::audio::core::System, the Playback
    // module, RegistryLoad) are built; stage 4 (LOGIC) now drives the real SoundLogicModule::Prepare.
    bool RootSoundModule::Prepare(rw::core::GeneralResourceAllocator* lpAllocator,
                                  void* /*lpRootInputBuffer*/, void* /*lpRootOutputBuffer*/,
                                  void* /*lpInputData*/, void* /*lpOutputData*/)
    {
        mpAllocator = lpAllocator;

        switch (mePrepareStage)
        {
        case E_PREPSTAGE_PERFMON:
            // [grow-in] X360 stage 0: CgsDev::PerfMonCpu::AddMonitor("Sound Logic") -> the CPU HUD
            //   monitor handle. A debug-HUD nicety (its colour/budget/parent params come from the X360
            //   body); skipped until the monitor is wired. Advance.
            mePrepareStage = E_PREPSTAGE_BASE;
            // fall through

        case E_PREPSTAGE_BASE:
            // [grow-in] X360 stage 1: the base ModuleSingleBuffered::Prepare (a resumable machine that
            //   builds the module's input/output DataStructures via PrepareDataStructures). On PC it
            //   never reports complete until the grown module supplies those structures, so calling it
            //   now returns false forever and stalls the loading screen -- deferred until the module
            //   layout + PrepareDataStructures land (with stages 2-4). Advance.
            mePrepareStage = E_PREPSTAGE_AUDIOSYSTEM;
            // fall through

        case E_PREPSTAGE_AUDIOSYSTEM:
            // [grow-in] X360 stage 2: carve the rw audio allocators, rw::audio::core::System::Create-
            //   Instance(196608), Csis::System::SetAllocator/Init, the mutex callbacks + stream path.
            //   The whole RWAudio core + Csis allocator hook is deferred. Advance.
            mePrepareStage = E_PREPSTAGE_PLAYBACK;
            // fall through

        case E_PREPSTAGE_PLAYBACK:
            // [grow-in] X360 stage 3: lock the audio system + CgsSound::Playback::Module::Prepare
            //   (0x826E90C0) with a carved heap. The Playback module facade is deferred. Advance.
            mePrepareStage = E_PREPSTAGE_REGISTRY;
            // fall through

        case E_PREPSTAGE_REGISTRY:
            // [grow-in] X360 stage 6: RootSoundModule::RegistryLoad (0x826EBA08) + a partial
            //   BridgeLogicToRoot. Deferred. Advance.
            mePrepareStage = E_PREPSTAGE_LOGIC;
            // fall through

        case E_PREPSTAGE_LOGIC:
            // X360 stage 4 (LABEL_22): SoundLogicModule::Prepare (0x82703C18, via vtable+0x58) with the
            //   logic RW allocator + the input buffer (a5) + the scratch LogicOutputBuffer (v15), then
            //   BridgeLogicToRoot under the output lock. Now REAL: drive the embedded module's Prepare
            //   with the captured allocator + the owned scratch IO buffers (the boot caller's buffers are
            //   null). The X360 treats the bool result as resumable -- `if (!v35) goto LABEL_26` returns
            //   0 (still preparing) so the loading screen retries next frame; only a true result advances
            //   the stage. The minimal SoundLogicModule::Prepare completes in one call and reports
            //   prepared, so this advances immediately; the stall path is honoured for when its stage
            //   machine grows resumable (Voice / state-manager build).
            // [grow-in] the per-stage logic allocator carve (GetRWGeneralResource(a2,0x19) + SetAllocator)
            //   and BridgeLogicToRoot (LogicOutputBuffer -> RootOutputBuffer, under the IOBuffer write lock)
            //   are deferred until the bridge + RootOutputBuffer paths land.
            if (!mLogicModule.Prepare(mpAllocator, &mLogicInputScratch, &mLogicOutputScratch))
            {
                // Still preparing -- persist at the LOGIC stage and retry next frame (X360 `goto LABEL_26`).
                return false;
            }
            mePrepareStage = E_PREPSTAGE_DONE;
            // fall through

        case E_PREPSTAGE_DONE:
        default:
            break;
        }

        // All stages complete: reset the machine (X360 sets mePrepareStage=0 at done) and report prepared.
        mePrepareStage = E_PREPSTAGE_PERFMON;
        mbPrepared     = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] RootSoundModule::Prepare: resumable stage machine complete "
                                          "(perfmon/base/audio/playback/registry/logic stages grow-in) allocator="
                                       << (lpAllocator ? "ok" : "null") << " -> prepared\n";
        return true;
    }
}
}
