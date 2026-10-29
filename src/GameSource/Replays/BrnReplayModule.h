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
#include "GameSource/Replays/BrnReplayBaseSerialiser.h" // BaseSerialiser::EMode and module-owned fields
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // CgsModule::ModuleSingleBuffered (base; vtable + the two RWMutexes via its DataBuffers)
#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"   // BrnReplays::GPUDiskWriteStream (embedded @ +0x980)
#include "GameSource/Replays/Stream/BrnReplayReadStream.h"           // BrnReplays::ReadStream
#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"          // BrnReplays::WriteStream
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h" // native producer @ +0x5A00
#include "SDKs/EATech/eajobs/job.h"                                // canonical native-width EA::Jobs::Job embeds
#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"       // BrnReplays::DiskReadStream (embedded @ +0x3F80)
#include "GameSource/Replays/BrnReplayDebugComponent.h"              // BrnReplays::DebugComponent (embedded @ +0x6264)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // mGameEventCache
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // mGameDataReceiverQueue
#include "GameShared/GameClasses/System/FileSystem/CgsFileHandle.h"  // CgsFileSystem::FileHandle (mHeaderFile)
#include "SharedClasses/BrnSharedConstants.h"                         // BrnUpdateSet (UpdateRestoring_PostSim)

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }   // Prepare's argument
namespace BrnReplays { namespace ReplayIO { struct InputBuffer_PreSim; struct OutputBuffer_PreSim; struct InputBuffer_PostSim; struct OutputBuffer_PostSim; struct StatusInterface; } }
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
        E_STREAM_STAGE_HEADER  = 3,
        E_STREAM_STAGE_CLOSING = 4,
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
        void Construct() ;

        // ARTIST 0x82660E78; the public virtual pre-sim entry owns IO locks and dispatch.
        virtual void Update_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                           ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                           BrnUpdateSet lUpdateSet);

        // ARTIST 8265F618: the complete post-sim update owns the IO buffer locks,
        // request adoption, GUI input events and state-specific post-sim dispatch.
        virtual void Update_PostSim(const ReplayIO::InputBuffer_PostSim* lpInputBuffer,
                                    ReplayIO::OutputBuffer_PostSim* lpOutputBuffer,
                                    BrnUpdateSet lUpdateSet);

        // BrnReplayModule.h.
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;
        typedef CgsModule::EventReceiverQueue<1024, 16> GameDataReceiverQueue;

        // Vtable slot 3 : tear down the debug component and the game-event cache,
        // then the module base.
        void Destruct() ;

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
        // Narrow declaration-shape repair required by the sealed Replay8 review:
        // Update_PostSim remains public virtual; its two state helpers are private.
        void UpdateRecording_PostSim(const ReplayIO::InputBuffer_PostSim* lpInputBuffer,
                                     ReplayIO::OutputBuffer_PostSim* lpOutputBuffer,
                                     BrnUpdateSet lUpdateSet);
        void UpdatePlaying_PostSim(const ReplayIO::InputBuffer_PostSim* lpInputBuffer,
                                   ReplayIO::OutputBuffer_PostSim* lpOutputBuffer,
                                   BrnUpdateSet lUpdateSet);

        // ARTIST 0x82660430 / 0x826608A8; called only by Update_PreSim.
        void UpdateRecording_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                                    ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                    BrnUpdateSet lUpdateSet);
        void UpdatePlaying_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                                  ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                  BrnUpdateSet lUpdateSet);
        void BeginRestoring(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                            ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                            BrnUpdateSet lUpdateSet);
        void StopRecording(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                          ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                          BrnUpdateSet lUpdateSet);
        void StopPlaying(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                        ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                        BrnUpdateSet lUpdateSet);
        void SetStatusInterface(ReplayIO::StatusInterface* lpStatusInterface);
        void ClearSerialisers(BaseSerialiser::EMode leMode, bool lbAllowStreaming);
        void CloseReplayFiles(ReplayIO::OutputBuffer_PreSim* lpOutputBuffer);
        bool WaitForCloseReplayFiles();

        // ARTIST calls this from Update_PreSim, but its body is absent from retained exports.
        void RenderDebugHUD();

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
        // Native contained objects follow. Some dormant native members still
        // require recovery: the documented offsets are ARTIST evidence, and
        // are not an assertion that this entire current declaration matches.
        // No storage is added merely to fill the remaining native spans.

        // @0x980 (2432): the embedded GPU disk write stream (Update_Dispatch dispatches it).
        GPUDiskWriteStream   mGpuWriteStream;          // @0x980

        // Native constructor 827E0418..0434 reaches GPU stream+0x2190 (Relocator::mJob)
        // and GPU stream+0x3180 (its lock). GPUDiskWriteStream's native members own both.

        // @0x3F80 (16256): the primary disk read stream ; its inlined ctor is the
        // console's second RtlInitializeCriticalSection (the stream's mutex is at offset 0).
        // WaitForOpenReplayFiles reads its status (+0x588) and pending-op count (+0x58C).
        DiskReadStream       mPrimaryDiskReadStream;   // @0x3F80
        // @0x4518: the game-data receiver queue  the file opens reply into.
        GameDataReceiverQueue mGameDataReceiverQueue;  // @0x4518

        // Four original Job subobjects: constructor 827E0440..0460 walks slots 0..3.
        // Their submission roles are not assigned a new policy by this source recovery.
        static const s32     KI_NUM_SERIALISE_JOBS = 4;
        EA::Jobs::Job        maJobs4940[KI_NUM_SERIALISE_JOBS]; // X360 +0x4940, stride0x350

        // Original job waited by WaitForSerialiseJobs @8264B4DC (X360 +0x5680).
        EA::Jobs::Job        mSerialiseJob;

        bool                 mbSerialiseActive;        // @0x59D0
        // ARTIST8264B4E0..B4F0 identifies the actual producer at+5A00:
        // its poster is+80 and its streaming flag+100. Both belong to
        // SimpleDataStreamProducer; its End preserves that call/store order.
        CgsMemory::SimpleDataStreamProducer mSerialiseStream; // native +0x5A00

        // DecFIGS names the three readiness flags in this order; ARTIST 8264B57C..B598
        // reads them at+5C32/+5C30/+5C31 and writes the playback-ready result at+5C31.
        bool                 mbReadyToPlay;             // ARTIST +0x5C30
        bool                 mbPlaybackDataReady;       // ARTIST +0x5C31
        bool                 mbStreamDataReady;         // ARTIST +0x5C32

        // ARTIST GUI event 526 copies these payload bytes without masking. Their
        // widths/owners are proven; no new semantic role is assigned here.
        u8                   mUnidentified5C38;
        u8                   mUnidentified5C39;

        // ARTIST also accesses the byte at +0x5C3A. Its width is attested; its role is not.
        bool                 mUnidentified5C3A;

        // ARTIST accesses four byte slots at +0x5C3B..+0x5C3E. Keep unidentified
        // entries typed by their observed byte width without assigning behavior.
        bool                 mUnidentified5C3B;
        u8                   mUnidentified5C3C;
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
        bool                 mUnidentified6259;          // GUI event 531; reset each PostSim
        bool                 mUnidentified625A;          // GUI event 532; reset each PostSim
        f32                  mfDebugHudAlpha;

        // ARTIST Construct stores zero here; Update_PostSim stores an event dword.
        // This member records only the proven width and ownership, not an inferred role.
        u32                  mUnidentified6260;
        DebugComponent       mDebugComponent;            // ARTIST +0x6264
    };
}
