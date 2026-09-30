#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Containers/CgsPriorityQueue.h"
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"
#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"
#include "GameShared/Jobs/DecompressionJob/DecompressionJobInterface.h"

namespace CgsResource {
class PoolModule;
struct RunningLoad { Events::LoadBundleRequest mLoadRequest; };

// DecFIGS CgsBundleLoaderModule.h:62; ARTIST ProcessPoolResponses 828EC148.
// This is a loaded-bundle record, not a file-stream slot.
struct LoadedBundleData {
    s32 miPoolId;
    ID mResourceId;
    ResourceHandle mHandle;
    s16 miRefCount;
    bool mbIsLiveUpdate;
};

class BundleLoaderModule : public CgsModule::ModuleSingleBuffered {
public:
    enum EStage { E_STAGE_START=0, E_STAGE_RUNNING=1, E_STAGE_DONE=2 };
    enum EStreamStage {
        STREAMSTAGE_IDLE=0, STREAMSTAGE_STREAMHEADER, STREAMSTAGE_STREAMDEBUGDATA,
        STREAMSTAGE_STREAMENTRYLIST, STREAMSTAGE_WAITFORALLOCATE, STREAMSTAGE_STREAMDATA,
        STREAMSTAGE_CLOSESTREAM, STREAMSTAGE_STREAMDONE, STREAMSTAGE_FIXUP,
        STREAMSTAGE_LOADDONE, STREAMSTAGE_COUNT
    };
    struct InitOptions {
        s32 miBundleHeaderBufferSize;
        s32 miDebugDataBufferSize;
        s32 miStreamBufferSize;
        s32 miMaxBundles;
        s32 miMaxResourcesPerBundle;
        bool mbForceUpperCaseFileNames;
        bool mbForcePS3HardDriveUpperCaseGame;
    };

    BundleLoaderModule() = default;
    static const u32 KU_SECONDARY_STREAM_BUFFER_SIZE = 0x80000;
    void Construct();
    bool Prepare();
    bool Release();
    void Destruct();
    bool Update(void* lpInputBuffer, void* lpOutputBuffer);
    void ProcessReceiverQueue();
    void ProcessPoolResponses();
    void RecordPostUpdateEvents(const BundleLoaderIO::InputBuffer_Record* lpInput);
    void ProcessBundleLoadRequests(const BundleLoaderIO::InputBuffer_Update* lpInput,
                                   BundleLoaderIO::OutputBuffer* lpOutput);
    void PostLoadFinishedEvent(BundleLoaderIO::OutputBuffer* lpOutput,
                              const Events::LoadBundleRequest* lpRequest,
                              Events::LoadBundleResponse::EResult leResult);
    bool CheckForLoads(BundleLoaderIO::OutputBuffer* lpOutput);
    bool CheckForUnloads(BundleLoaderIO::OutputBuffer* lpOutput);

    // Existing synchronous intake remains until ResourceModule's full stream,
    // failure and emergency-stall handoffs are connected.
    void EnqueueLoadRequest(const Events::LoadBundleRequest& lrRequest);
    void ProcessLoadRequests(PoolModule* lpPoolModule, FTypeResolver lpfnResolveType);
    void EnqueueUnloadRequest(const Events::UnloadBundleRequest& lrRequest);
    void ProcessUnloadRequests(PoolModule* lpPoolModule);

    bool MoveToFirstResource();
    bool MoveToNextResource();
    void ProcessBundleHeader();
    void ProcessBundleEntryList(void* lpOutputBuffer);
    void SendPartialFixupRequest(void* lpOutputBuffer);
    bool StreamHeaderFunc();
    bool StreamDebugDataFunc();
    bool StreamEntryListFunc(void* lpOutputBuffer);
    bool StreamDataFunc();
    bool StreamDoneFunc(void* lpOutputBuffer);
    bool StreamClose(void* lpOutputBuffer);
    bool StreamCompressedDataAsJobFunc(void* lpOutputBuffer);
    bool StreamIdleFunc(void* lpOutputBuffer);
    void UpdateStream(void* lpOutputBuffer);

private:
    // Named ARTIST fields with DecFIGS declaration shape; pointers and embedded
    // events use the host ABI. Compression/job members follow their lifecycle.
    EStage mePrepareStage;
    EStage meReleaseStage;
    EStreamStage meStreamStage;
    s32 miBundleHeaderBufferSize;
    s32 miDebugBufferSize;
    s32 miStreamBufferSize;
    s32 miMaxBundles;
    s32 miMaxResourcesPerBundle;
    bool mbForceUpperCaseFileNames;
    char* mpcHeaderBuffer;
    char* mpcDebugDataBuffer;
    bool mabStreamBuffersUsed[2];
    char* mapcStreamBuffers[2];
    char* mpcSecondaryStreamBuffer;
    bool* mpNeeds;
    SmallResource* mpResources;
    s32 miHeaderPos;
    s32 miCurrentResource;
    s32 miCurrentMemoryType;
    s32 miCurrentResourcePosition;
    FifoQueue<RunningLoad,4> mQueuedLoads;
    Events::LoadBundleRequest mLoadRequest;
    Events::UnloadBundleRequest mUnloadRequest;
    Events::AllocateResourceListRequest mAllocationRequest;
    Events::AllocateResourceListResponse mAllocationResponse;
    BundleV2 mCurrentBundle;
    LoadedBundleData mCurrentLoad;
    CgsFileSystem::ReadStream maStreams[2];
    s32 miCurrentStream;
    CgsModule::EventReceiverQueue<128,16> mReceiverQueue;
    CgsContainers::PriorityQueue<Events::LoadBundleRequest,128> mLoadRequestQueue;
    CgsContainers::PriorityQueue<Events::UnloadBundleRequest,128> mUnloadRequestQueue;
    LoadedBundleData* mpLoadedBundles;
    s32 miNumLoadedBundles;
    s32 miMaxLoadedBundles;
    DecompressionJobInterface mDecompressionJobInterface;
    void* mpDecompressionStreamStart;
    u32 muDecompressionStreamSize;
    bool mbStreamJobStarted;
    bool mbOnLastStreamJob;
    BundleLoaderIO::InputBuffer_Record::PoolReceiveQueue mPoolReceiveQueueCache;
    s32 miNextFixUpRequestIndex;
    s32 miMaxPartialFixups;
    u64 muLoadStartTime;
    u64 muTimerTrace;

    CgsModule::EventQueue<Events::LoadBundleRequest,256> mLoadRequests;
    CgsModule::EventQueue<Events::UnloadBundleRequest,256> mUnloadRequests;
};
}
