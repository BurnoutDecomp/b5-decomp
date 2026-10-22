#pragma once

// BrnReplays::ReplayModule -- the replay record/playback module that owns the
// write/read streams, the per-serialiser book-keeping, and the action-replay
// control flags. DWARF/assert home path: GameSource/Replays/BrnReplayModuleIO.h
// (the IO buffers are homed in BrnReplayModuleIO.h; the module's own bodies live
// in this ReplayModule TU).
//
// The native C++ constructor is ARTIST 0x827E03D0. It constructs the base locks,
// marks the variable event queue unconstructed, initializes native locks/jobs, and
// stamps the embedded DebugComponent vtable. The logical virtual Construct is the
// separate ARTIST body at 0x82656E20; it initializes ReplayModule state and calls
// DebugComponent::Construct. Prepare @0x82652768 then calls base Prepare and Register
// before the original allocator sequence.
//
// The debug overlay uses named accessors. Streams are embedded values in the original
// declaration: WriteStream begins at ARTIST +0x880 and ReadStream at +0x8E0. Comments
// below cite console addresses and source names; the host uses native pointer widths.
// Unnamed dwords/bytes are typed only at widths proven by ARTIST and have no inferred role.
//
// ARTIST debug-overlay anchors:
//   +0x230 meState; +0x234 meStreamStage; +0x84C mapSerialisers[11]
//   +0x880 WriteStream; +0x8E0 ReadStream; +0x5C3F..+0x5C45 action flags
//   +0x3F80 DiskReadStream; +0x980 GPUDiskWriteStream; +0x6264 DebugComponent

#include "types.hpp"
#include "GameSource/Replays/BrnReplayShared.h"
#include "GameSource/Replays/BrnReplayReels.h"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // CgsModule::ModuleSingleBuffered (base; vtable + the two RWMutexes via its DataBuffers)
#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"   // BrnReplays::GPUDiskWriteStream (embedded @ +0x980)
#include "GameSource/Replays/Stream/BrnReplayReadStream.h"           // BrnReplays::ReadStream
#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"          // BrnReplays::WriteStream
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h" // CgsMemory::DataStreamCommandPoster (embedded @ +0x5A80)
#include "eathread/eathread_futex.h"                                 // EA::Thread::Futex (CRITICAL_SECTION-backed; its ctor does RtlInitializeCriticalSection)
#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"       // BrnReplays::DiskReadStream (embedded @ +0x3F80)
#include "GameSource/Replays/BrnReplayDebugComponent.h"              // BrnReplays::DebugComponent (embedded @ +0x6264)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // mGameEventCache
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // mGameDataReceiverQueue
#include "GameShared/GameClasses/System/FileSystem/CgsFileHandle.h"  // CgsFileSystem::FileHandle (mHeaderFile)
#include "SharedClasses/BrnSharedConstants.h"                         // BrnUpdateSet (UpdateRestoring_PostSim)

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }   // Prepare's argument
namespace BrnReplays { namespace ReplayIO { struct InputBuffer_PostSim; struct OutputBuffer_PostSim; } }
namespace CgsMemory { class LinearMalloc; }                               // mpLinearMalloc

namespace BrnReplays
{
    // The live per-serialiser book-keeping objects the overlay snapshots each frame
    // are BaseSerialiser instances (PreUpdateRecord @0x8264BD08).
    class BaseSerialiser;
    struct MemBuffer; // DecFIGS ReplayModule::mpSecondaryStreamBuffer pointee

    // DWARF: BrnReplayShared.h -- the replay stream-state machine the overlay labels.
    enum EStreamState
    {
        E_STREAM_STATE_IDLE      = 0,
        E_STREAM_STATE_RECORDING = 1,
        E_STREAM_STATE_PLAYING   = 2,
        E_STREAM_STATE_RESTORING = 3,
        E_STREAM_STATE_COUNT     = 4,
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

        enum EPrepareStage
        {
            E_PREPARE_STAGE_START   = 0,
            E_PREPARE_STAGE_MANAGER = 1,
            E_PREPARE_STAGE_DONE    = 2,
        };

        enum EReleaseStage
        {
            E_RELEASE_STAGE_START   = 0,
            E_RELEASE_STAGE_MANAGER = 1,
            E_RELEASE_STAGE_DONE    = 2,
        };

        enum EMode
        {
            E_MODE_INGAME    = 0,
            E_MODE_PAUSEMENU = 1,
            E_MODE_COUNT     = 2,
        };

        ReplayModule();
        void Construct() override;

        // BrnReplayModule.h.
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;
        typedef CgsModule::EventReceiverQueue<1024, 16> GameDataReceiverQueue;

        // Vtable slot 3 : tear down the debug component and the game-event cache,
        // then the module base.
        void Destruct() override;

        // . Drain the game-data receiver queue for the header file's open response,
        // then report whether the replay files are open: a primary stream (read or write) open
        // and the header file open. Clears the queue once they are.
        bool WaitForOpenReplayFiles();

        // . Once every live serialiser has restored its data: go idle, post the
        // LeaveReplay game event, and switch every serialiser back to idle.
        void UpdateRestoring_PostSim(const ReplayIO::InputBuffer_PostSim* lpInputBuffer,
                                     ReplayIO::OutputBuffer_PostSim* lpOutputBuffer,
                                     BrnUpdateSet lUpdateSet);

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
        WriteStream* GetWriteStream()          { return &mWriteStream; }
        ReadStream*  GetReadStream()           { return &mReadStream; }
        s32          GetWriteIndex() const     { return mWriteStream.miCurrentWriteIndex; }
        s32          GetWriteStalls() const    { return mWriteStream.miStallCount; }
        s32          GetReadBlockStart() const { return mReadStream.miCurrentFrunk; }

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
        // ARTIST 0x8264B8B8 / 0x8264B910. Walk all eleven serialiser slots;
        // present entries receive their original BaseSerialiser lock operation.
        void LockSerialisers();
        void UnlockSerialisers();

        // ARTIST and DecFIGS identify these members by name. The host keeps native
        // pointer widths; no code reaches them through console-offset arithmetic.
        EPrepareStage        mePrepareStage;
        EReleaseStage        meReleaseStage;
        EStreamState         meState;
        EStreamStage         meStreamStage;
        EMode                meMode;
        GameEventQueue       mGameEventCache;
        BaseSerialiser*      mapSerialisers[KI_NUM_SERIALISERS];

        // ReplayModule::Prepare owns the bank-48 allocator. ARTIST calls WriteStream
        // at +0x880 and ReadStream at +0x8E0; both are embedded values.
        CgsMemory::LinearMalloc* mpLinearMalloc;
        WriteStream          mWriteStream;
        ReadStream           mReadStream;

        // DecFIGS member names; ARTIST access widths and consumers pin the frame counter.
        char*                mpcHeaderBuffer;          // +0x8F8, file buffer (0x20000)
        char*                mpcPrimaryStreamBuffer;   // +0x8FC, linear allocation (0x80000)
        char*                mpcHeaderStreamBuffer;    // +0x908, linear allocation (0x4000)
        MemBuffer*           mpSecondaryStreamBuffer;  // +0x910, memory-resource result
        s32                  miHeaderStreamPos;        // +0x914
        s64                  miCurrentFrame;           // +0x918, ARTIST loads/stores as a qword
        f32                  mfCurrentTime;            // +0x920
        s32                  miKeyFrameInterval;       // +0x924
        f32                  mfStartTime;              // +0x928
        f32                  mfEndTime;                // +0x92C

        CgsFileSystem::FileHandle mHeaderFile;
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

        // @0x3B00 (15104): an RtlInitializeCriticalSection'd lock (the committed
        // EA::Thread::Futex ctor performs that init).
        EA::Thread::Futex    mLockA;                   // @0x3B00

        // @0x3F80 (16256): the primary disk read stream ; its inlined ctor is the
        // console's second RtlInitializeCriticalSection (the stream's mutex is at offset 0).
        // WaitForOpenReplayFiles reads its status (+0x588) and pending-op count (+0x58C).
        DiskReadStream       mPrimaryDiskReadStream;   // @0x3F80
        // @0x4518: the game-data receiver queue  the file opens reply into.
        GameDataReceiverQueue mGameDataReceiverQueue;  // @0x4518

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

        // ARTIST accesses four byte slots at +0x5C3B..+0x5C3E. Keep unidentified
        // entries typed by their observed byte width without assigning behavior.
        bool                 mUnidentified5C3B;
        bool                 mUnidentified5C3C;
        bool                 mUnidentified5C3D;
        bool                 mUnidentified5C3E;

        bool                 mbStartPlaying;            // ARTIST +0x5C3F
        bool                 mbStopPlaying;             // ARTIST +0x5C40
        bool                 mbStartRecording;          // ARTIST +0x5C41
        bool                 mbStopRecording;           // ARTIST +0x5C42
        bool                 mbMarkActionReplay;        // ARTIST +0x5C43
        bool                 mbStartActionReplay;       // ARTIST +0x5C44
        bool                 mbAutoStart;               // ARTIST +0x5C45
        bool                 mbPlayNext;                // DecFIGS ReplayModule member

        Reel                 maReels[6];                 // ARTIST initializes six 0x101-byte records
        s32                  miCurrentRecordReel;
        s32                  miCurrentPlayReel;
        bool                 mbStopPlayingRequestReceived;
        f32                  mfDebugHudAlpha;

        // ARTIST Construct stores zero here; Update_PostSim stores an event dword.
        // This member records only the proven width and ownership, not an inferred role.
        u32                  mUnidentified6260;
        DebugComponent       mDebugComponent;            // ARTIST +0x6264
    };
}
