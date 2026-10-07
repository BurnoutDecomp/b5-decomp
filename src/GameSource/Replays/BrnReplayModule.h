#pragma once

// ARTIST ReplayModule 827E03D0 / 82656E20 / 82660E78. Original named
// state and real contained streams/jobs; native pointers widen by their types.
#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"
#include "GameSource/Replays/BrnReplayShared.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayReels.h"
#include "GameSource/Replays/BrnReplayDebugComponent.h"
#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"
#include "GameSource/Replays/Stream/BrnReplayReadStream.h"
#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"
#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileHandle.h"
#include "SDKs/EATech/eajobs/job.h"

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }
namespace CgsMemory { class LinearMalloc; }
namespace BrnReplays
{
    enum EStreamState
    {
        E_STREAM_STATE_IDLE=0, E_STREAM_STATE_RECORDING=1,
        E_STREAM_STATE_PLAYING=2, E_STREAM_STATE_RESTORING=3, E_STREAM_STATE_COUNT=4
    };
    enum EStreamStage
    {
        E_STREAM_STAGE_CLOSED=0, E_STREAM_STAGE_OPENING=1, E_STREAM_STAGE_OPEN=2,
        E_STREAM_STAGE_HEADER=3, E_STREAM_STAGE_CLOSING=4, E_STREAM_STAGE_COUNT=5
    };
    namespace ReplayIO { struct RequestInterface; struct InputBuffer_PreSim; struct OutputBuffer_PreSim; struct StatusInterface; }
    struct MemBuffer;
    class ReplayModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        static const s32 KI_NUM_SERIALISERS=E_ID_COUNT;
        static const s32 KI_REPLAY_LINEAR_BANK=48;
        static const s32 KI_NUM_SERIALISE_JOBS=4;
        enum EPrepareStage { E_PREPARESTAGE_START=0, E_PREPARESTAGE_MANAGER=1, E_PREPARESTAGE_DONE=2 };
        enum EReleaseStage { E_RELEASESTAGE_START=0, E_RELEASESTAGE_MANAGER=1, E_RELEASESTAGE_DONE=2 };
        enum EMode { E_MODE_INGAME=0, E_MODE_PAUSEMENU=1, E_MODE_COUNT=2 };

        ReplayModule();
        void Construct() override;
        virtual bool Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList);
        virtual void Update_PreSim(const ReplayIO::InputBuffer_PreSim* lpInput,
                           ReplayIO::OutputBuffer_PreSim* lpOutput, BrnUpdateSet leUpdateSet);
        void Update_Dispatch();
        void StoreSerialisers(const ReplayIO::RequestInterface& lrRequestInterface);
        void WaitForSerialiseJobs();

        EStreamState GetState() const { return meState; }
        EStreamStage GetStreamStage() const { return meStreamStage; }
        BaseSerialiser* GetSerialiser(s32 liId) const { return mapSerialisers[liId]; }
        WriteStream* GetWriteStream() { return &mWriteStream; }
        ReadStream* GetReadStream() { return &mReadStream; }
        s32 GetWriteIndex() const { return mWriteStream.miCurrentWriteIndex; }
        s32 GetWriteStalls() const { return mWriteStream.miStallCount; }
        s32 GetReadBlockStart() const { return mReadStream.miCurrentFrunk; }
        void RequestStartPlaying() { mbStartPlaying=true; }
        void RequestStopPlaying() { mbStopPlaying=true; }
        void RequestStartRecording() { mbStartRecording=true; }
        void RequestStopRecording() { mbStopRecording=true; }
        void RequestMarkActionReplay() { mbMarkActionReplay=true; }
        void RequestStartActionReplay() { mbStartActionReplay=true; }
        bool* GetAutoStartFlagPtr() { return &mbAutoStart; }

    private:
        void LockSerialisers();
        void UnlockSerialisers();
        void ClearSerialisers(BaseSerialiser::EMode leMode, bool lbAllowStreaming);
        void RenderDebugHUD();
        void SetStatusInterface(ReplayIO::StatusInterface* lpStatus);
        void UpdateRecording_PreSim(const ReplayIO::InputBuffer_PreSim*, ReplayIO::OutputBuffer_PreSim*, BrnUpdateSet);
        void UpdatePlaying_PreSim(const ReplayIO::InputBuffer_PreSim*, ReplayIO::OutputBuffer_PreSim*, BrnUpdateSet);
        void BeginRestoring(const ReplayIO::InputBuffer_PreSim*, ReplayIO::OutputBuffer_PreSim*, BrnUpdateSet);
        void StopRecording(const ReplayIO::InputBuffer_PreSim*, ReplayIO::OutputBuffer_PreSim*, BrnUpdateSet);
        void StopPlaying(const ReplayIO::InputBuffer_PreSim*, ReplayIO::OutputBuffer_PreSim*, BrnUpdateSet);
        bool WaitForOpenReplayFiles();
        void CloseReplayFiles(ReplayIO::OutputBuffer_PreSim* lpOutput);
        bool WaitForCloseReplayFiles();

        // Canonical objects in original field order. No duplicated contained
        // relocator Job/stream locks or console-sized job placeholders.
        EPrepareStage mePrepareStage;                 // ARTIST +228
        EReleaseStage meReleaseStage;                 // +22C
        EStreamState meState;                         // +230
        EStreamStage meStreamStage;                   // +234
        EMode meMode;                                 // +238
        CgsModule::VariableEventQueue<1536,16> mGameEventCache; // +23C
        BaseSerialiser* mapSerialisers[KI_NUM_SERIALISERS]; // +84C
        CgsMemory::LinearMalloc* mpLinearMalloc;       // +878
        WriteStream mWriteStream;                     // +880
        ReadStream mReadStream;                       // +8E0
        char* mpcHeaderBuffer;                        // +8F8
        char* mpcPrimaryStreamBuffer;                 // +8FC
        char* mpcHeaderStreamBuffer;                  // +908
        s32 miHeaderStreamPos;                       // +90C
        MemBuffer* mpSecondaryStreamBuffer;           // +910
        s32 miPlaybackPreRoll;                       // +914, ARTIST-only warmup count
        s64 miCurrentFrame;                          // +918, original ld/std
        f32 mfCurrentTime;                           // +920
        s32 miKeyFrameInterval;                      // +924
        f32 mfStartTime;                             // +928
        f32 mfEndTime;                               // +92C
        CgsFileSystem::FileHandle mHeaderFile;         // +930
        GPUDiskWriteStream mGpuWriteStream;            // +980
        DiskReadStream mDiskReadStream;               // +3F80
        CgsModule::EventReceiverQueue<1024,16> mGameDataReceiverQueue; // +4518
        EA::Jobs::Job maSerialiseJobs[KI_NUM_SERIALISE_JOBS]; // +4940
        EA::Jobs::Job mSerialiseJob;                   // +5680
        bool mbSerialiseActive;                      // +59D0
        CgsMemory::DataStreamCommandPoster mCommandPoster; // +5A80
        bool mbPosterActive;                         // +5B00
        bool mbReadyToPlay;                          // +5C30
        bool mbPlaybackDataReady;                    // +5C31
        bool mbStreamDataReady;                      // +5C32
        bool mbJobThreadDisabled;                    // +5C38, original EnableJobThread gate
        bool mbPlayNext;                             // +5C3A, inferred from stop-playing path
        bool mbPauseOnKeyFrame;                      // +5C3B, ARTIST-only
        bool mbPaused;                               // +5C3C
        bool mbPlaybackEnded;                        // +5C3D
        bool mbWaitingForRecordPause;                // +5C3E
        bool mbStartPlaying,mbStopPlaying,mbStartRecording,mbStopRecording;
        bool mbMarkActionReplay,mbStartActionReplay,mbAutoStart; // +5C3F..45
        Reel maReels[6];                             // original six 257-byte reels
        s32 miCurrentRecordReel;                     // +6250
        s32 miCurrentPlayReel;                       // +6254
        bool mbStopPlayingRequestReceived;           // +6258
        bool mbStopRecordingRequestReceived;         // +6259, inferred role
        bool mbFinishRecordingRequested;             // +625A, inferred role
        f32 mfDebugHudAlpha;                         // +625C
        s32 miLastRecordFrame;                       // +6260
        DebugComponent mDebugComponent;              // +6264
    };
}
