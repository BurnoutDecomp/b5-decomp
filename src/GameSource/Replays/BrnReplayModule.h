#pragma once

// BrnReplays::ReplayModule -- the replay record/playback module that owns the
// write/read streams, the per-serialiser book-keeping, and the action-replay
// control flags. DWARF/assert home path: GameSource/Replays/BrnReplayModuleIO.h
// (the IO buffers are homed in BrnReplayModuleIO.h; the module's own bodies live
// in this ReplayModule TU).
//
// It is a full CgsModule::ModuleSingleBuffered: the X360 ctor @0x827E03D0 stores
// the base ModuleSingleBuffered vtable (off_820CE500) then the derived ReplayModule
// vtable (off_820CFEF0), and constructs the two EA::Thread::RWMutex members the base
// owns at +0x10 / +0x118 -- exactly the base sub-object construction. The ctor body
// (in the .cpp) reproduces, BY NAMED MEMBER, every store the asm makes past the base:
// a bool at +0x23C, the embedded EA::Jobs::Job objects, two RtlInitializeCriticalSection
// locks (the committed EA::Thread::Futex members whose ctors do that init), and a
// trailing contained-interface vtable stamp at +0x6264.
//
// The DEBUG-OVERLAY accessor slice (the surface BrnReplays::DebugComponent reads
// through -- it never touches raw layout, only these named accessors) is preserved
// verbatim. The constructor-touched members are added alongside; both sets sit at
// their X360-attested absolute offsets, with honest explicit padding between. X360
// offsets are the 4-byte-pointer console ABI; access is by name, so the bodies are
// faithful regardless of host pointer width (shape-faithful, not host-byte-exact --
// same rule as the committed module siblings).
//
// X360 offsets the debug overlay touches (from BrnReplayDebugComponent.cpp asm):
//   +0x230 (560)   meState         -- EStreamState (RenderStatus/RenderMainWindow)
//   +0x234 (564)   meStreamStage   -- EStreamStage
//   +0x84C..+0x874 mapSerialisers[11] -- BaseSerialiser* per ESerialiserId
//   +0x89C (2204)  mpWriteStream   -- WriteStream* (RenderWriteStreamBlocks)
//   +0x8C0 (2240)  miWriteIndex    -- s32 write cursor / block-start
//   +0x8C4 (2244)  miWriteStalls   -- s32 stall count
//   +0x8E0 (2272)  mpReadStream    -- ReadStream* (RenderReadStreamBlocks)
//   +0x8E4 (2276)  miReadBlockStart-- s32 read block-start
//   +0x5C3F..+0x5C45 (23615..23621) action-replay control flags (bool)
//
// X360 offsets the ctor / Update_Dispatch / WaitForSerialiseJobs touch:
//   +0x23C  (572)   mbFlag23C        -- bool, ctor stores 0
//   +0x980  (2432)  mGpuWriteStream  -- BrnReplays::GPUDiskWriteStream (Update_Dispatch
//                                        calls .Dispatch())
//   +0x2B10 (11024) job              -- EA::Jobs::Job (placeholder; ctor sub-ctor DEFERRED)
//   +0x3B00 (15104) mLockA           -- EA::Thread::Futex (RtlInitializeCriticalSection)
//   +0x3F80 (16256) mLockB           -- EA::Thread::Futex (RtlInitializeCriticalSection)
//   +0x4940 (18752) maJobs[4]        -- EA::Jobs::Job[4] (placeholders; sub-ctors DEFERRED)
//   +0x5680 (22144) mSerialiseJob    -- EA::Jobs::Job (WaitForSerialiseJobs WaitOn's it)
//   +0x59D0 (22992) mbSerialiseActive-- bool (WaitForSerialiseJobs gate)
//   +0x5A80 (23168) mCommandPoster   -- CgsMemory::DataStreamCommandPoster (WaitForSerialiseJobs
//                                        calls .End())
//   +0x5B00 (23296) mbPosterActive   -- bool (WaitForSerialiseJobs clears it)
//   +0x6264 (25188) mpTailInterfaceVTable -- contained-interface vtable (off_820CE890)

#include "types.hpp"
#include "GameSource/Replays/BrnReplayShared.h"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // CgsModule::ModuleSingleBuffered (base; vtable + the two RWMutexes via its DataBuffers)
#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"   // BrnReplays::GPUDiskWriteStream (embedded @ +0x980)
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h" // CgsMemory::DataStreamCommandPoster (embedded @ +0x5A80)
#include "eathread/eathread_futex.h"                                 // EA::Thread::Futex (CRITICAL_SECTION-backed; its ctor does RtlInitializeCriticalSection)

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }   // Prepare's argument
namespace CgsMemory { class LinearMalloc; }                               // mpLinearMalloc

namespace BrnReplays
{
    // The live per-serialiser book-keeping objects the overlay snapshots each frame
    // are BaseSerialiser instances (PreUpdateRecord @0x8264BD08).
    class BaseSerialiser;

    // DWARF home: BrnReplayModuleIO.h:96 et al. The write/read replay streams the
    // debug overlay's block view reaches by pointer; their block-table layout is
    // owned by the ReplayModule TU.
    class WriteStream;
    class ReadStream;

    // DWARF: BrnReplayShared.h -- the replay stream-state machine the overlay labels.
    enum EStreamState
    {
        E_STREAM_STATE_IDLE      = 0,
        E_STREAM_STATE_RECORDING = 1,
        E_STREAM_STATE_PLAYING   = 2,
        E_STREAM_STATE_COUNT     = 3,
    };

    // The per-state stream stage the overlay labels (five named stages).
    enum EStreamStage
    {
        E_STREAM_STAGE_CLOSED  = 0,
        E_STREAM_STAGE_OPENING = 1,
        E_STREAM_STAGE_OPEN    = 2,
        E_STREAM_STAGE_CLOSING = 3,
        E_STREAM_STAGE_ERROR   = 4,
        E_STREAM_STAGE_COUNT   = 5,
    };

    namespace ReplayIO { struct RequestInterface; }   // StoreSerialisers' argument

    class ReplayModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // Number of serialiser slots the overlay snapshots == ESerialiserId count.
        static const s32 KI_NUM_SERIALISERS = E_ID_COUNT; // 11
        // ReplayModule::Prepare @0x82652768: `GetLinearAllocator(list, 48)`.
        static const s32 KI_REPLAY_LINEAR_BANK = 48;

        ReplayModule();

        // X360 0x8265D1B0. Dispatch the GPU disk write stream (BrnGameModule::DispatchThread).
        void Update_Dispatch();

        // X360 0x82652768 -- the module prepare: base prepare, then the linear allocator and
        // the module's own stream buffers. Takes the game-data allocator list, exactly as the
        // scripted load's stage 8 passes it.
        bool Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList);

        // X360 0x8264B600 -- adopt every serialiser the request interface has collected and
        // give each one its stream buffer and its STATIC buffer out of the module's linear
        // region. Nothing else in the engine allocates them.
        void StoreSerialisers(const ReplayIO::RequestInterface& lrRequestInterface);

        // X360 0x8264B4A8. If a serialise job is in flight, wait for it, close the command
        // poster, and clear the in-flight flags. (UpdateRecording_PostSim drives this.)
        void WaitForSerialiseJobs();

        // --- accessors the debug overlay reads (RenderStatus/RenderMainWindow) ---
        EStreamState GetState() const          { return meState; }
        EStreamStage GetStreamStage() const    { return meStreamStage; }

        // The live per-serialiser record for a given id, or null if that slot is
        // empty. PreUpdateRecord walks all KI_NUM_SERIALISERS slots.
        BaseSerialiser* GetSerialiser(s32 liId) const { return mapSerialisers[liId]; }

        // --- write/read stream surface (RenderWriteStreamBlocks/...Status) ---
        WriteStream* GetWriteStream() const    { return mpWriteStream; }
        ReadStream*  GetReadStream() const     { return mpReadStream; }
        s32          GetWriteIndex() const      { return miWriteIndex; }
        s32          GetWriteStalls() const     { return miWriteStalls; }
        s32          GetReadBlockStart() const  { return miReadBlockStart; }

        // --- action-replay control flags driven by the overlay's menu callbacks ---
        void RequestStartPlaying()    { mbStartPlaying = true; }
        void RequestStopPlaying()     { mbStopPlaying = true; }
        void RequestStartRecording()  { mbStartRecording = true; }
        void RequestStopRecording()   { mbStopRecording = true; }
        void RequestMarkActionReplay(){ mbMarkActionReplay = true; }
        void RequestStartActionReplay(){ mbStartActionReplay = true; }

        // OnActivate registers this flag with the debug menu (Enable auto-start).
        bool* GetAutoStartFlagPtr()   { return &mbAutoStart; }

    private:
        // X360 sequence markers (offsets in the comments). The host ModuleSingleBuffered
        // base and the embedded real types host-inflate past their X360 byte spans, so the
        // members are declared in X360 ORDER (shape-faithful, not byte-exact); the fixed
        // explicit pads below preserve the relative spacing where the spanned bytes are
        // pure padding. All access is BY NAME, so the bodies are faithful regardless.

        // meState directly follows the ModuleSingleBuffered base sub-object (@0x230 on X360).
        // The console keeps its prepare stage in the module base at +0x228 (`v4 = a1[138]`,
        // `a1[138] = 1`, `a1[139] = 0`). This port's CgsModule base does not expose that
        // slot, so the same two-state machine is carried here BY NAME. Same meaning, same
        // guard: run the allocations once, then report ready every call.
        // The console reads its prepare stage out of the module base at +0x228 (`v4 = a1[138]`),
        // and the BASE is what advances it past 1 so the allocations never run twice. This port's
        // base does not expose that slot, so the same "already done" fact is carried by name.
        bool                 mbPrepared;              // stands in for the base stage at +0x228
        EStreamState         meState;                 // @0x230
        EStreamStage         meStreamStage;           // @0x234
        u8                   maPad238[0x23C - 0x238];  // @0x238
        bool                 mbFlag23C;                // @0x23C  (ctor stores 0)
        u8                   maPad23D[0x84C - 0x23D];  // @0x23D
        BaseSerialiser*      mapSerialisers[KI_NUM_SERIALISERS]; // @0x84C..@0x874 (11 ptrs)
        // @0x878 (2168): the module's LINEAR allocator -- ReplayModule::Prepare @0x82652768
        // takes it from the game-data allocator list (`GetLinearAllocator(list, 48)`, then
        // `SetAlignment(16)`) and StoreSerialisers @0x8264B600 carves every serialiser's stream
        // and static buffers out of it (`Malloc(*(a1 + 2168), size)`).
        CgsMemory::LinearMalloc* mpLinearMalloc;       // @0x878
        u8                   maPad87C[0x89C - 0x87C];  // @0x87C
        WriteStream*         mpWriteStream;            // @0x89C
        u8                   maPad8A0[0x8C0 - 0x8A0];  // @0x8A0
        s32                  miWriteIndex;             // @0x8C0
        s32                  miWriteStalls;            // @0x8C4
        u8                   maPad8C8[0x8E0 - 0x8C8];  // @0x8C8
        ReadStream*          mpReadStream;             // @0x8E0
        s32                  miReadBlockStart;         // @0x8E4
        u8                   maPad8E8[0x980 - 0x8E8];  // @0x8E8

        // ------------------------------------------------------------------------
        // From here on the embedded REAL types (GPUDiskWriteStream / Futex /
        // DataStreamCommandPoster) host-inflate beyond their X360 byte spans (pointer
        // widening + a wider CRITICAL_SECTION), so the tail is modelled in X360
        // DECLARATION ORDER without absolute-offset pads: the members exist BY NAME at
        // their proven sequence positions (shape-faithful, not byte-exact -- the same
        // rule the committed module siblings follow once a real heavy embed appears).
        // The job sub-objects stay asm-sized opaque placeholders (0x350 == the X360
        // Job stride) so their footprint is documented; their EA::Jobs::Job::Job(.,0)
        // sub-constructions are DEFERRED (see the .cpp).
        // ------------------------------------------------------------------------

        // @0x980 (2432): the embedded GPU disk write stream (Update_Dispatch dispatches it).
        GPUDiskWriteStream   mGpuWriteStream;          // @0x980

        // FLAG: PLACEHOLDER. EA::Jobs::Job @ +0x2B10 (X360 stride 0x350). Sub-ctor DEFERRED.
        u8                   maJob2B10Placeholder[0x350]; // @0x2B10

        // @0x3B00 (15104) / @0x3F80 (16256): the two RtlInitializeCriticalSection'd locks
        // (each committed EA::Thread::Futex ctor performs that init).
        EA::Thread::Futex    mLockA;                   // @0x3B00
        EA::Thread::Futex    mLockB;                   // @0x3F80

        // FLAG: PLACEHOLDER. 4 x EA::Jobs::Job @ +0x4940 (stride 0x350; ctor loops
        // i=3..0 Job::Job(v,0), v += 0x350). Sub-ctors DEFERRED.
        static const s32     KI_NUM_SERIALISE_JOBS = 4;
        u8                   maJobs4940Placeholder[KI_NUM_SERIALISE_JOBS * 0x350]; // @0x4940

        // FLAG: PLACEHOLDER. EA::Jobs::Job @ +0x5680 (WaitForSerialiseJobs WaitOn's it).
        // Sub-ctor DEFERRED; the WaitOn on it is FLAGGED in the .cpp.
        u8                   maSerialiseJobPlaceholder[0x350]; // @0x5680

        bool                 mbSerialiseActive;        // @0x59D0
        // @0x5A80 (23168): the data-stream command poster (WaitForSerialiseJobs ends it).
        CgsMemory::DataStreamCommandPoster mCommandPoster; // @0x5A80
        bool                 mbPosterActive;           // @0x5B00

        // @0x5C3F..+0x5C45: the action-replay control flags (in X360 order).
        bool                 mbStartPlaying;           // @0x5C3F (23615)
        bool                 mbStopPlaying;            // @0x5C40 (23616)
        bool                 mbStartRecording;         // @0x5C41 (23617)
        bool                 mbStopRecording;          // @0x5C42 (23618)
        bool                 mbMarkActionReplay;       // @0x5C43 (23619)
        bool                 mbStartActionReplay;      // @0x5C44 (23620)
        bool                 mbAutoStart;              // @0x5C45 (23621) -- Enable auto-start

        // @0x6264 (25188): the ctor stamps a contained-interface vtable here
        // (off_820CE890). FLAG: the owning interface type / its vtable are not
        // reconstructed; the slot is left null in the ctor.
        void*                mpTailInterfaceVTable;    // @0x6264
    };
}
