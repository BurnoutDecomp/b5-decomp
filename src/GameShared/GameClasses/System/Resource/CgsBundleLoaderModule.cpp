#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/core/stdc/stdc.h"
#include <cstring>
#include <xtl.h>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // _miStreamEntryCopy timing

#define CGS_ARTIST_BUNDLE_LOADER_FILE \
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsBundleLoaderModule.cpp"

// ARTIST streaming stages. Native event sizes and pointers replace the console
// record strides; resource payload bytes retain the converted native bundle ABI.
namespace CgsResource {
void BundleLoaderModule::ProcessReceiverQueue(
    const BundleLoaderIO::InputBuffer_Update*, BundleLoaderIO::OutputBuffer*) // 828E2888
{
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liTag = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent) {
        if (liTag == 16) {
            const Events::OpenReadStreamResponse* lpResponse = reinterpret_cast<const Events::OpenReadStreamResponse*>(lpEvent);
            maStreams[lpResponse->GetEventId()] = lpResponse->GetStream();
        } else if (liTag == 18) {
            const Events::CloseReadStreamResponse* lpResponse = reinterpret_cast<const Events::CloseReadStreamResponse*>(lpEvent);
            mabStreamBuffersUsed[lpResponse->GetEventId()] = false;
        } else {


































































































































































































































































































































































            CGS_FAIL_STREAM("Unexpected event received\n");
        }
        const CgsModule::Event* lpNext = 0;
        liTag = mReceiverQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    mReceiverQueue.Clear();
}
// These remaining stages are still explicit activation gates. The resource
// module continues using its existing synchronous intake until the full
// allocation/failure/IO/job lifetime is connected and tested.
void BundleLoaderModule::ProcessPoolResponses(BundleLoaderIO::OutputBuffer*)
{
    // ARTIST828EC148. Partial-fixup replies during DATA are intentionally
    // ignored; the final reply publishes the bundle's retained handle.
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liTag = mPoolReceiveQueueCache.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent) {
        if (liTag == 17) {
            mAllocationResponse = *reinterpret_cast<const Events::AllocateResourceListResponse*>(lpEvent);
            if (MoveToFirstResource()) {
                if (mCurrentBundle.IsCompressed()) {
                    const u32 luSize = mAllocationResponse.mpEntries[miCurrentResource].GetUncompressedSize(miCurrentMemoryType);
                    void* lpDest = mAllocationResponse.mpResources[miCurrentResource].m_baseResources[miCurrentMemoryType];
                    mDecompressionJobInterface.BeginStream();
                    mDecompressionJobInterface.CreateEntry(lpDest, luSize);
                }
                meStreamStage = STREAMSTAGE_STREAMDATA;
            } else {
                meStreamStage = STREAMSTAGE_CLOSESTREAM;
            }
        } else if (liTag == 19 && meStreamStage != STREAMSTAGE_STREAMDATA) {
            meStreamStage = STREAMSTAGE_LOADDONE;
            if (!mCurrentLoad.mbIsLiveUpdate) {
                const Events::FixUpAndResolveResourceListResponse* lpResponse = reinterpret_cast<const Events::FixUpAndResolveResourceListResponse*>(lpEvent);
                mCurrentLoad.mHandle = lpResponse->mListHandle;
                s32 liSlot = 0;
                while (liSlot < miMaxLoadedBundles && mpLoadedBundles[liSlot].miPoolId >= 0)
                    ++liSlot;
                if (liSlot < miMaxLoadedBundles)
                    mpLoadedBundles[liSlot] = mCurrentLoad;
                else



































































































































                    CGS_FAIL_STREAM("Could not store loaded bundle info - out of bundles. Should have caught this during load\n");
                ++miNumLoadedBundles;
            }
        }
        const CgsModule::Event* lpNext = 0;
        liTag = mPoolReceiveQueueCache.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    mPoolReceiveQueueCache.Clear();
}
void BundleLoaderModule::ProcessBundleHeader() // 828D7A90
{
    std::memcpy(&mCurrentBundle, mpcHeaderBuffer, sizeof(mCurrentBundle));
    if (mpcDebugDataBuffer)
        *mpcDebugDataBuffer = 0;
    // ARTIST 0x828D7AE4 / CgsBundleLoaderModule.cpp:599: the bundle platform is X360 (2).






























    CGS_ASSERT_STREAM(mCurrentBundle.muPlatform == BundleV2::KU_PLATFORM, "Invalid platform");







    CGS_ASSERT_STREAM(mCurrentBundle.muVersion == BundleV2::KU_VERSION, "Bundle version is out of date\n");


    CGS_ASSERT_STREAM(mCurrentBundle.muResourceEntriesCount <= static_cast<u32>(miMaxResourcesPerBundle), "Bundle contains more resources than maximum defined in init options\n");
    CGS_ASSERT_STREAM(mCurrentBundle.mauResourceDataOffset[0] <= static_cast<u32>(miBundleHeaderBufferSize), "Bundle header is larger than buffer size provided\n");
}
void BundleLoaderModule::ProcessBundleEntryList(void* lpOutputBuffer) // 828FAF58
{






































    CGS_ASSERT(lpOutputBuffer, "lpOutput");
    char lacName[256];
    CGS_ASSERT(std::strlen(mLoadRequest.macFileName) < sizeof(lacName), "String too long");
    std::strncpy(lacName, mLoadRequest.macFileName, sizeof(lacName));
    for (char* lp = lacName; *lp; ++lp)
        if (*lp >= 'a' && *lp <= 'z') *lp -= 'a' - 'A';
    mAllocationRequest = Events::AllocateResourceListRequest();
    mAllocationRequest.miEventId = mLoadRequest.miEventId;
    mAllocationRequest.miPoolId = mLoadRequest.miPoolId;
    mAllocationRequest.mpEntries = reinterpret_cast<const BundleV2::ResourceEntry*>(mpcHeaderBuffer);
    mAllocationRequest.mpcDebugData = mpcDebugDataBuffer;
    mAllocationRequest.miNumEntries = mCurrentBundle.muResourceEntriesCount;
    mAllocationRequest.mpNeeds = mpNeeds;
    mAllocationRequest.mpResources = mpResources;
    mAllocationRequest.mbLiveUpdateReplace = mLoadRequest.mbLiveUpdateReplace;
    mAllocationRequest.mbAllowFailiure = mLoadRequest.mbAllowFailiure;
    mAllocationRequest.mbCompressedBundle = mCurrentBundle.IsCompressed();
    const char* lpcListName = mLoadRequest.mbLiveUpdateReplace ? "__LIVE_UPDATE_LIST__" : lacName;
    mAllocationRequest.mListId.SetHash(
        static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(lpcListName))) | 0x8000000000000000ull);
    mCurrentLoad.miPoolId = mLoadRequest.miPoolId;
    mCurrentLoad.mResourceId = mAllocationRequest.mListId;
    mCurrentLoad.miRefCount = 1;
    mCurrentLoad.mbIsLiveUpdate = mAllocationRequest.mbLiveUpdateReplace;
    BundleLoaderIO::OutputBuffer* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
    lpOutput->GetPoolSendQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&mAllocationRequest), 16, sizeof(mAllocationRequest));
    mbStreamJobStarted = false;
    mbOnLastStreamJob = false;
    miNextFixUpRequestIndex = 0;
}









































void BundleLoaderModule::PostLoadFinishedEvent(BundleLoaderIO::OutputBuffer* lpOutput,
    const Events::LoadBundleRequest* lpLoadRequest, Events::LoadBundleResponse::EResult leResult) // 828EC3C8
{
    CGS_ASSERT_AT(lpOutput, "lpOutput", CGS_ARTIST_BUNDLE_LOADER_FILE, 728);

    Events::LoadBundleResponse lResponse;   // DWARF cpp:705 (ARTIST +25)
    lResponse.Construct(lpLoadRequest->GetUser(), lpLoadRequest->GetEventId(), lpLoadRequest->GetFileName(),
                        lpLoadRequest->GetPoolId(), leResult);
    lpOutput->GetLoadBundleResponseQueue()->AddEvent(lResponse);
}
void BundleLoaderModule::UpdateStream(BundleLoaderIO::OutputBuffer* lpOutput) // 82906B30
{

    if (_miStreamUpdate > -1)
        CgsDev::PerfMonCpu::StartMonitor(_miStreamUpdate);
    switch (meStreamStage) {
    case STREAMSTAGE_IDLE: {
        meStreamStage = STREAMSTAGE_IDLE;
        if (!StreamIdleFunc(lpOutput)) break;
        miCurrentStream = 1 - miCurrentStream;
        LARGE_INTEGER lTime;
        QueryPerformanceCounter(&lTime);
        muLoadStartTime = static_cast<u64>(lTime.QuadPart);
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Loading " << mLoadRequest.macFileName
                                     << " on stream " << miCurrentStream << "\n";
    }
        // fall through: ready stages advance within the same update.
    case STREAMSTAGE_STREAMHEADER:
        meStreamStage = STREAMSTAGE_STREAMHEADER;
        if (!StreamHeaderFunc()) break;
        // fall through
    case STREAMSTAGE_STREAMDEBUGDATA:
        meStreamStage = STREAMSTAGE_STREAMDEBUGDATA;
        if (!StreamDebugDataFunc()) break;
        // fall through
    case STREAMSTAGE_STREAMENTRYLIST:
        meStreamStage = STREAMSTAGE_STREAMENTRYLIST;
        if (!StreamEntryListFunc(lpOutput)) break;
        // fall through
    case STREAMSTAGE_WAITFORALLOCATE:
        meStreamStage = STREAMSTAGE_WAITFORALLOCATE;
        break;
    case STREAMSTAGE_STREAMDATA: {
        meStreamStage = STREAMSTAGE_STREAMDATA;
        if (!mCurrentBundle.IsCompressed()) {
            if (!StreamDataFunc(lpOutput)) break;
        } else {




















































































            CGS_ASSERT_STREAM_AT(mCurrentBundle.IsMainMemOptimised() && mCurrentBundle.IsGraphicsMemOptimised(),
                "Compressed data must be main and graphics mem optmised to use with decompression job\n",
                CGS_ARTIST_BUNDLE_LOADER_FILE, 857);
            if (!StreamCompressedDataAsJobFunc(lpOutput)) break;
        }



    }
        // fall through
    case STREAMSTAGE_CLOSESTREAM:
        meStreamStage = STREAMSTAGE_CLOSESTREAM;
        if (mQueuedLoads.GetLength() == 0) CheckForLoads(lpOutput);
        StreamClose(lpOutput);
        // fall through
    case STREAMSTAGE_STREAMDONE:
        meStreamStage = STREAMSTAGE_STREAMDONE;
        if (mQueuedLoads.GetLength() == 0) CheckForLoads(lpOutput);
        if (mAllocationResponse.miPoolId >= 0) {
            if (!StreamDoneFunc(lpOutput)) break;
        } else {
            meStreamStage = STREAMSTAGE_LOADDONE;
            break;
        }
        // fall through
    case STREAMSTAGE_FIXUP:
        meStreamStage = STREAMSTAGE_FIXUP;
        if (mQueuedLoads.GetLength() == 0) CheckForLoads(lpOutput);
        break;
    case STREAMSTAGE_LOADDONE:
        if (mLoadRequest.mbLiveUpdateReplace)
            mLoadRequest.SetPoolId(mAllocationResponse.miPoolId);   // DWARF: BundleLoaderEvent::SetPoolId
        PostLoadFinishedEvent(lpOutput, &mLoadRequest, mAllocationResponse.mbFailed
            ? Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY : Events::LoadBundleResponse::E_RESULT_SUCCESS);
        meStreamStage = STREAMSTAGE_IDLE;
        break;
    case STREAMSTAGE_COUNT:

























































        CGS_FAIL_AT("Should never happen.", CGS_ARTIST_BUNDLE_LOADER_FILE, 949);
        break;
    default:
        break;
    }
    if (_miStreamUpdate > -1)
        CgsDev::PerfMonCpu::StopMonitor(_miStreamUpdate);
}
bool BundleLoaderModule::StreamIdleFunc(void* lpOutputBuffer)
{


















    CGS_ASSERT_AT(lpOutputBuffer, "lpOutputBuffer", CGS_ARTIST_BUNDLE_LOADER_FILE, 977);
    BundleLoaderIO::OutputBuffer* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
    mLoadRequestQueue.Tick(1);
    mUnloadRequestQueue.Tick(1);
    if (CheckForUnloads(lpOutput))
        return false;
    // ARTIST @0x82900F4C tests the queue length before popping (DWARF local lNextLoad, cpp:968).
    if (mQueuedLoads.GetLength() > 0)
    {
        RunningLoad lNextLoad;
        mQueuedLoads.Pop(&lNextLoad);
        mLoadRequest = lNextLoad.mLoadRequest;
        return true;
    }
    return CheckForLoads(lpOutput);
}
bool BundleLoaderModule::StreamHeaderFunc() // 829042A0
{
    // DWARF (DecFIGS CgsSystemUnity CU) lists no stream local: ARTIST re-reads the stream's device
    // for the Read (lwzx @0x829042E0) after the validity test (@0x829042BC).
    if (!maStreams[miCurrentStream].IsValid())
        return false;
    miHeaderPos += maStreams[miCurrentStream].Read(sizeof(BundleV2) - miHeaderPos, mpcHeaderBuffer + miHeaderPos);
    if (miHeaderPos != sizeof(BundleV2))
        return false;






























    CGS_ASSERT_STREAM(std::memcmp(mpcHeaderBuffer, "bnd2", 4) == 0, mLoadRequest.GetFileName() << " was an invalid bundle\n");
    ProcessBundleHeader();
    return true;
}










// @ 0x829043A0 -- read the bundle's debug data (when it has any and a buffer was given) up to the
// resource entries. The DWARF (DecFIGS CgsSystemUnity CU) puts liRequiredBufferSize (cpp:1030) in
// the outer block and liDebugDataOffset (1036) / liResourceEntriesOffset (1052) in the inner one;
// at ARTIST's +25 they land on 1055 / 1061 / 1077. ARTIST compares them signed (cmpw @0x829043DC,
// 0x829043E8, 0x82904460) and re-reads the entries offset for the second read (@0x8290443C).
bool BundleLoaderModule::StreamDebugDataFunc() // 829043A0
{
    if (mCurrentBundle.ContainsDebugData() && mpcDebugDataBuffer)
    {
        int32_t liRequiredBufferSize = mCurrentBundle.muResourceEntriesOffset - mCurrentBundle.muDebugDataOffset;
        if (miDebugBufferSize >= liRequiredBufferSize)
        {



            int32_t liDebugDataOffset = mCurrentBundle.muDebugDataOffset;
            if (miHeaderPos < liDebugDataOffset)
            {
                miHeaderPos += maStreams[miCurrentStream].Read(liDebugDataOffset - miHeaderPos, 0);
                if (miHeaderPos < liDebugDataOffset)
                    return false;
            }









            int32_t liResourceEntriesOffset = mCurrentBundle.muResourceEntriesOffset;
            miHeaderPos += maStreams[miCurrentStream].Read(liResourceEntriesOffset - miHeaderPos, mpcDebugDataBuffer + (miHeaderPos - liDebugDataOffset));
            if (miHeaderPos < liResourceEntriesOffset)
                return false;
        }
    }
    return true;
}

















// @ 0x82904478 -- read up to the entry list, then the entry list itself into the header buffer.
// The DWARF (DecFIGS CgsSystemUnity CU) names the two int32_t locals liResourceEntriesOffset
// (DecFIGS cpp:1088) and liResourceDataOffset (cpp:1104); ARTIST's lines are DecFIGS + 25 here
// (its streamed assert sits on 1132, li r5,0x46C @0x829045B8), so they land on 1113 and 1129.
// ARTIST compares them signed (cmpw @0x82904494 / 0x829044E4 / 0x82904628), streams the
// bundle name through BundleLoaderEvent::GetFileName (DWARF CgsResourceIOEvents.h:74) and times
// only the entry-list read with _miStreamEntryCopy (registered as "    Entry MemCpy" by
// ResourceModule::Construct @0x82906474), guarded as `> -1` (cmpwi -1 / ble @0x829045D4).
bool BundleLoaderModule::StreamEntryListFunc(void* lpOutputBuffer) // 82904478
{

    int32_t liResourceEntriesOffset = mCurrentBundle.muResourceEntriesOffset;
    if (miHeaderPos < liResourceEntriesOffset)
    {
        miHeaderPos += maStreams[miCurrentStream].Read(liResourceEntriesOffset - miHeaderPos, 0);
        if (miHeaderPos < liResourceEntriesOffset)
            return false;
    }









    int32_t liResourceDataOffset = mCurrentBundle.mauResourceDataOffset[0];


    CGS_ASSERT_STREAM_AT(liResourceDataOffset - liResourceEntriesOffset <= miBundleHeaderBufferSize,
        "Bundle " << mLoadRequest.GetFileName() << " entry list will not fit in header buffer - need "
        << (liResourceDataOffset - liResourceEntriesOffset) << " bytes\n", CGS_ARTIST_BUNDLE_LOADER_FILE, 1132);
    if (_miStreamEntryCopy > -1)
        CgsDev::PerfMonCpu::StartMonitor(_miStreamEntryCopy);
    miHeaderPos += maStreams[miCurrentStream].Read(liResourceDataOffset - miHeaderPos, mpcHeaderBuffer + miHeaderPos - liResourceEntriesOffset);
    if (_miStreamEntryCopy > -1)
        CgsDev::PerfMonCpu::StopMonitor(_miStreamEntryCopy);
    if (miHeaderPos < liResourceDataOffset)
        return false;
    ProcessBundleEntryList(lpOutputBuffer);
    return true;
}
bool BundleLoaderModule::StreamCompressedDataAsJobFunc(void* lpOutputBuffer)
{
    // ARTIST82900FA0..829013F4. Copy at most one 512-KiB batch into the
    // secondary buffer, whose lifetime extends until its inflate job completes.
    BundleLoaderIO::OutputBuffer* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
    CgsFileSystem::ReadStream& lrStream = maStreams[miCurrentStream];
    if ((lrStream.IsBufferComplete() || (mbStreamJobStarted && mbOnLastStreamJob))
        && mQueuedLoads.GetLength() == 0)
        CheckForLoads(lpOutput);

    if (mbStreamJobStarted)
    {
        if (!mDecompressionJobInterface.WaitForFlushJobs(false))
            return false;
        mbStreamJobStarted = false;
        if (mbOnLastStreamJob)
        {
            mDecompressionJobInterface.EndStream();
            return true;
        }
    }
    SendPartialFixupRequest(lpOutput);
    u32 luCopied = 0;
    while (lrStream.GetAmountOfDataInBuffer() != 0)
    {
        const u64 luBegin = lrStream.Tell();
        u64 luPosition = luBegin;
        lrStream.StartAsyncRead(&mpDecompressionStreamStart, &muDecompressionStreamSize);
        u32 luAvailable = muDecompressionStreamSize;
        while (luAvailable != 0 && luCopied < KU_SECONDARY_STREAM_BUFFER_SIZE)
        {
            const BundleV2::ResourceEntry& lrEntry = mAllocationResponse.mpEntries[miCurrentResource];
            const u64 luDesired = static_cast<u64>(lrEntry.mauDiskOffset[miCurrentMemoryType])
                                + mCurrentBundle.mauResourceDataOffset[miCurrentMemoryType];
            if (luDesired > luPosition)
            {
                const u32 luSkip = static_cast<u32>((luDesired - luPosition < luAvailable)
                    ? luDesired - luPosition : luAvailable);
                luPosition += luSkip;
                luAvailable -= luSkip;
                continue;
            }
            const u32 luRemaining = lrEntry.GetDiskSize(miCurrentMemoryType) - miCurrentResourcePosition;
            u32 luAmount = luRemaining < luAvailable ? luRemaining : luAvailable;
            if (luAmount > KU_SECONDARY_STREAM_BUFFER_SIZE - luCopied)
                luAmount = KU_SECONDARY_STREAM_BUFFER_SIZE - luCopied;
            void* lpCopy = mpcSecondaryStreamBuffer + luCopied;
            std::memcpy(lpCopy, static_cast<const char*>(mpDecompressionStreamStart)
                + static_cast<size_t>(luPosition - luBegin), luAmount);
            mDecompressionJobInterface.AppendToEntry(lpCopy, luAmount);
            luCopied = (luCopied + luAmount + 15) & ~15u;
            if (luAmount == luRemaining)
            {
                mDecompressionJobInterface.FinishEntry();
                if (!MoveToNextResource())
                {
                    mbOnLastStreamJob = true;
                    // ARTIST8290130C skips the final cursor increment: these
                    // copied bytes remain in the ring until the stream closes.
                    break;
                }
                const BundleV2::ResourceEntry& lrNext = mAllocationResponse.mpEntries[miCurrentResource];
                mDecompressionJobInterface.CreateEntry(
                    mAllocationResponse.mpResources[miCurrentResource].m_baseResources[miCurrentMemoryType],
                    lrNext.GetUncompressedSize(miCurrentMemoryType));
            }
            else
                miCurrentResourcePosition += luAmount;
            luPosition += luAmount;
            luAvailable -= luAmount;
        }
        lrStream.StopAsyncRead(static_cast<u32>(luPosition - luBegin));
        mbStreamJobStarted = mDecompressionJobInterface.RunFlushJobs();
        if (mbStreamJobStarted)
            break;









































































































































































































































































































        CGS_ASSERT_STREAM(!mbOnLastStreamJob, "Should never finish the last resource without any data to stream!");
    }
    return false;
}
bool BundleLoaderModule::StreamDoneFunc(void* lpOutputBuffer) // 828FB178
{
    if (mAllocationResponse.mbFailed) {
        meStreamStage = STREAMSTAGE_LOADDONE;
        return false;
    }



















    CGS_ASSERT(lpOutputBuffer, "lpOutput");
    Events::FixUpAndResolveResourceListRequest lRequest = Events::FixUpAndResolveResourceListRequest();
    lRequest.miPoolId = mAllocationResponse.miPoolId;
    lRequest.mListId = mAllocationResponse.mListId;
    lRequest.miFirstIndex = miNextFixUpRequestIndex;
    lRequest.miCount = mAllocationResponse.miNumEntries - miNextFixUpRequestIndex;
    lRequest.mbFinalFixup = true;
    lRequest.mbFixUpDependencies = mAllocationRequest.mbLiveUpdateReplace;
    static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer)->GetPoolSendQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 18, sizeof(lRequest));
    return true;
}
bool BundleLoaderModule::StreamClose(void* lpOutputBuffer) // 828FB260
{












    // DWARF: lCloseStreamRequest (cpp:1544) is built before the output-buffer check; ARTIST stores it
    // @0x828FB27C..0x828FB29C, ahead of the null test of the buffer.
    Events::CloseReadStreamRequest lCloseStreamRequest;
    lCloseStreamRequest.Construct(&mReceiverQueue, miCurrentStream, maStreams[miCurrentStream]);
    CGS_ASSERT_AT(lpOutputBuffer, "lpOutputBuffer", CGS_ARTIST_BUNDLE_LOADER_FILE, 1574);
    const bool lbAdded = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer)->GetStreamRequestQueue()->CloseReadStream(
        lCloseStreamRequest);
    maStreams[miCurrentStream] = 0;
    return lbAdded;
}
bool BundleLoaderModule::CheckForUnloads(BundleLoaderIO::OutputBuffer* lpOutput) // 828FB308
{
    if (mUnloadRequestQueue.GetLength() <= 0)
        return false;
    Events::UnloadBundleRequest lRequest;
    while (mUnloadRequestQueue.Pop(&lRequest)) {
        ID lId;
        lId.SetHash(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(lRequest.macFileName)))
                    | 0x8000000000000000ull);
        s32 liSlot = 0;
        while (liSlot < miMaxLoadedBundles) {
            const LoadedBundleData& lrLoaded = mpLoadedBundles[liSlot];
            if (lrLoaded.miPoolId == lRequest.miPoolId && lrLoaded.mResourceId == lId)
                break;
            ++liSlot;
        }
        if (liSlot == miMaxLoadedBundles) {




























































































































































            CGS_FAIL_STREAM("Could not find bundle in pool to unload\n");
            continue;
        }
        LoadedBundleData& lrLoaded = mpLoadedBundles[liSlot];
        if (lrLoaded.miRefCount == 1) {
            Events::UnloadResourceListRequest lUnload = Events::UnloadResourceListRequest();
            lUnload.miEventId = liSlot;
            lUnload.miPoolId = lRequest.miPoolId;
            lUnload.mListId = lId;
            lpOutput->GetPoolSendQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lUnload), 20, sizeof(lUnload));
            --miNumLoadedBundles;
            lrLoaded.miPoolId = -1;
        }
        --lrLoaded.miRefCount;
        Events::UnloadBundleResponse lResponse = Events::UnloadBundleResponse();
        lResponse.mpUser = lRequest.mpUser;
        lResponse.miEventId = lRequest.miEventId;
        lResponse.SetFileName(lRequest.macFileName);
        lResponse.miPoolId = lRequest.miPoolId;
        lResponse.mbLiveUpdateReplace = false;
        lpOutput->GetUnloadBundleResponseQueue()->AddEvent(lResponse);
    }
    return true;
}
void BundleLoaderModule::SendPartialFixupRequest(void* lpOutputBuffer) // 828FBFC0
{
    if (mAllocationResponse.mbFailed || miMaxPartialFixups <= 0)
        return;
    const s32 liReady = miCurrentMemoryType > 0 ? mAllocationResponse.miNumEntries : miCurrentResource;
    const s32 liAvailable = liReady - miNextFixUpRequestIndex;
    const s32 liCount = liAvailable < miMaxPartialFixups ? liAvailable : miMaxPartialFixups;
    if (liCount <= 0)
        return;




















































































































































































    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    Events::FixUpAndResolveResourceListRequest lRequest = Events::FixUpAndResolveResourceListRequest();
    lRequest.miPoolId = mAllocationResponse.miPoolId;
    lRequest.mListId = mAllocationResponse.mListId;
    lRequest.miFirstIndex = miNextFixUpRequestIndex;
    lRequest.miCount = liCount;
    lRequest.mbFinalFixup = false;
    lRequest.mbFixUpDependencies = mAllocationRequest.mbLiveUpdateReplace;
    static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer)->GetPoolSendQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 18, sizeof(lRequest));
    miNextFixUpRequestIndex += liCount;
}

    s32 BundleLoaderModule::_miUpdatePerfMon = -1;
    s32 BundleLoaderModule::_miStreamUpdate = -1;
    s32 BundleLoaderModule::_miStreamEntryCopy = -1;
    s32 BundleLoaderModule::_miStreamDataCopy = -1;
    s32 BundleLoaderModule::_miTraceStreamUpdate_StreamHeader = -1;
    s32 BundleLoaderModule::_miTraceStreamUpdate_StreamEntries = -1;
    s32 BundleLoaderModule::_miTraceStreamUpdate_StreamData = -1;


void BundleLoaderModule::ProcessBundleLoadRequests(
    const BundleLoaderIO::InputBuffer_Update* lpInput, BundleLoaderIO::OutputBuffer*)
{
    // ARTIST828E2A10..828E2BAC; the export is missing, so constants and
    // case-sensitive string tests are read from the instructions and image.
    const BundleLoaderIO::InputBuffer_Update::LoadBundleRequestQueue* lpLoads = lpInput->GetLoadBundleRequestQueue();
    for (s32 li = 0; li < lpLoads->GetLength(); ++li) {
        const Events::LoadBundleRequest& lrRequest = lpLoads->GetEvent(li);
        s32 liPriority = 0;
        if (std::strstr(lrRequest.macFileName, "TRK_"))
            liPriority = std::strstr(lrRequest.macFileName, "Prop") ? 1000 : 2000;
        else if (std::strstr(lrRequest.macFileName, "GuiApt\\SaveLoadComponent.bundle"))
            liPriority = 2500;
        if (!mLoadRequestQueue.Push(&lrRequest, liPriority)
            && (CgsDev::Message::gxMessageFilterFlags & 1))
            *CgsDev::Log::gpDebugPrint << "Unable to add entry. Queue is full.\n";
    }
    const BundleLoaderIO::InputBuffer_Update::UnloadBundleRequestQueue* lpUnloads = lpInput->GetUnloadBundleRequestQueue();
    for (s32 li = 0; li < lpUnloads->GetLength(); ++li)
        if (!mUnloadRequestQueue.Push(&lpUnloads->GetEvent(li), 0)
            && (CgsDev::Message::gxMessageFilterFlags & 1))
            *CgsDev::Log::gpDebugPrint << "Unable to add entry. Queue is full.\n";
}



















bool BundleLoaderModule::CheckForLoads(BundleLoaderIO::OutputBuffer* lpOutput) // 828FB758
{
    const s32 liNextStream = 1 - miCurrentStream;
    if (mabStreamBuffersUsed[liNextStream] || mLoadRequestQueue.GetLength() <= 0)
        return false;
    Events::LoadBundleRequest lRequest;
    while (mLoadRequestQueue.Peek(&lRequest)) {
        // ARTIST828FB8F8..934: uppercase the lookup text, then compare the
        // full resident ID against the untagged, zero-extended32-bit CRC.
        // The native fast path has no live-update or stream-stage exclusion.
        char lacLookupName[256];
        std::strncpy(lacLookupName, lRequest.macFileName, sizeof(lacLookupName));
        rw::core::stdc::StringToUpperCase(lacLookupName, lacLookupName);
        ID lId;
        lId.SetHash(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(lacLookupName))));
        s32 liLoaded = 0;
        while (liLoaded < miMaxLoadedBundles) {
            const LoadedBundleData& lrLoaded = mpLoadedBundles[liLoaded];
            if (lrLoaded.miPoolId == lRequest.miPoolId && lrLoaded.mResourceId == lId)
                break;
            ++liLoaded;
        }
        if (liLoaded >= miMaxLoadedBundles)
            break;
        ++mpLoadedBundles[liLoaded].miRefCount;
        PostLoadFinishedEvent(lpOutput, &lRequest, Events::LoadBundleResponse::E_RESULT_SUCCESS);
        mLoadRequestQueue.Pop(&lRequest);
        if (mLoadRequestQueue.GetLength() <= 0)
            return false;
    }
    Events::OpenReadStreamRequest lOpen = Events::OpenReadStreamRequest();
    lOpen.Construct(&mReceiverQueue, liNextStream);
    char lacName[256];
    std::strncpy(lacName, lRequest.macFileName, sizeof(lacName));
    if (mbForceUpperCaseFileNames) {
        char* lpBegin = std::strchr(lacName, ':');
        if (!lpBegin) lpBegin = lacName;
        for (char* lp = lpBegin; *lp; ++lp)
            if (*lp >= 'a' && *lp <= 'z') *lp -= 'a' - 'A';
    }
    lOpen.SetFileName(lacName);
    lOpen.SetBuffer(mapcStreamBuffers[liNextStream]);
    lOpen.SetBufferSize(miStreamBufferSize);
    lOpen.SetNumBlocks(miStreamBufferSize / 0x100000);
    lOpen.SetNormalPriority(25);
    lOpen.SetHighPriority(25);
    lOpen.SetUseHDCache(lRequest.mbUseHDCache);
    // ARTIST828FBE10: only the nonresident open path suppresses a request
    // whose name matches the active bundle, preserving the queued request.
    if (meStreamStage != STREAMSTAGE_IDLE &&
        rw::core::stdc::StringNoCaseCompare(lRequest.macFileName, mLoadRequest.macFileName) == 0)
        return false;
    mLoadRequestQueue.Pop(&lRequest);
    if (meStreamStage == STREAMSTAGE_IDLE)
        mLoadRequest = lRequest;
    else {
        RunningLoad lRunning = {lRequest};
        mQueuedLoads.Push(&lRunning);
    }
    CGS_ASSERT(!mabStreamBuffersUsed[liNextStream], "Attempting to open stream with buffer that's already in use\n");
    mabStreamBuffersUsed[liNextStream] = true;
    lpOutput->GetStreamRequestQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lOpen), 16, sizeof(lOpen));
    miHeaderPos = 0;
    maStreams[liNextStream] = 0;
    return true;
}


bool BundleLoaderModule::MoveToFirstResource() // 828D7DA8
{
    if (mAllocationResponse.mbFailed)
        return false;
    miCurrentResourcePosition = 0;
    miCurrentResource = 0;
    miCurrentMemoryType = 0;
    // DWARF local liResourceMemSize (int32_t, cpp:1647): ARTIST tests it signed (cmpwi @0x828D7DFC)
    // and re-reads miCurrentResource for the needs flag after the call (@0x828D7E08).
    int32_t liResourceMemSize =
        mAllocationResponse.mpEntries[miCurrentResource].GetDiskSize(miCurrentMemoryType);
    if (liResourceMemSize != 0 && mAllocationResponse.mpNeeds[miCurrentResource])
        return true;
    return MoveToNextResource();
}


bool BundleLoaderModule::MoveToNextResource() // 828D7CE0
{
    s32 liEntry = miCurrentResource;
    s32 liMemoryType = miCurrentMemoryType;
    for (;;) {
        if (++liEntry >= mAllocationResponse.miNumEntries) {
            ++liMemoryType;
            liEntry = 0;
            if (liMemoryType >= static_cast<s32>(BundleV2::E_MEMTYPE_NUMTYPES))
                return false;
        }
        if (mAllocationResponse.mpEntries[liEntry].GetDiskSize(liMemoryType)
            && mAllocationResponse.mpNeeds[liEntry]) {
            miCurrentResource = liEntry;
            miCurrentMemoryType = liMemoryType;
            miCurrentResourcePosition = 0;
            return true;
        }
    }
}


bool BundleLoaderModule::StreamDataFunc(BundleLoaderIO::OutputBuffer* /*lpOutput*/) // 82904648
{
    CgsFileSystem::ReadStream& lrStream = maStreams[miCurrentStream];
    while (lrStream.GetAmountOfDataInBuffer() > 0) {
        const BundleV2::ResourceEntry& lrEntry = mAllocationResponse.mpEntries[miCurrentResource];
        // 82904688-C8 is missing from the decompiler's expression tree:
        // the desired offset is entry.diskOffset[type] + bundle.dataOffset[type].
        const u32 luDiskOffset = lrEntry.mauDiskOffset[miCurrentMemoryType]
                               + mCurrentBundle.mauResourceDataOffset[miCurrentMemoryType];
        const u32 luPosition = static_cast<u32>(lrStream.Tell());
        if (luDiskOffset > luPosition) {
            lrStream.Read(luDiskOffset - luPosition, 0);
            continue;
        }
        const u32 luRemaining = lrEntry.GetDiskSize(miCurrentMemoryType) - miCurrentResourcePosition;
        u8* lpDestination = static_cast<u8*>(
            mAllocationResponse.mpResources[miCurrentResource].m_baseResources[miCurrentMemoryType])
            + miCurrentResourcePosition;
        const u32 luRead = lrStream.Read(luRemaining, lpDestination);
        if (luRead != luRemaining)
            miCurrentResourcePosition += luRead;
        else if (!MoveToNextResource())
            return true;
    }
    return false;
}


// Inlined in ResourceModule::Update82907948: read-lock the record buffer and
// append its native events to the loader's cache for the following update.
void BundleLoaderModule::RecordPostUpdateEvents(const BundleLoaderIO::InputBuffer_Record* lpInput)
{
    BundleLoaderIO::InputBuffer_Record* lpLock = const_cast<BundleLoaderIO::InputBuffer_Record*>(lpInput);
    lpLock->LockForRead();
    const BundleLoaderIO::InputBuffer_Record::PoolReceiveQueue* lpQueue = lpInput->GetPoolReceiveQueue();
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liTag = lpQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent) {
        mPoolReceiveQueueCache.AddEvent(lpEvent, liTag, liSize);
        const CgsModule::Event* lpNext = 0;
        liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    lpLock->UnlockForRead();
}
}

#undef CGS_ARTIST_BUNDLE_LOADER_FILE
