#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>
#include <windows.h>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// ARTIST streaming stages. Native event sizes and pointers replace the console
// record strides; resource payload bytes retain the converted native bundle ABI.
namespace CgsResource {

void BundleLoaderModule::ProcessBundleLoadRequests(
    const BundleLoaderIO::InputBuffer_Update* lpInput, BundleLoaderIO::OutputBuffer*)
{
    // ARTIST828E2A10..828E2BAC; the export is missing, so constants and
    // case-sensitive string tests are read from the instructions and image.
    const auto* lpLoads = lpInput->GetLoadBundleRequestQueue();
    for (s32 li = 0; li < lpLoads->GetLength(); ++li) {
        const auto& lrRequest = lpLoads->GetEvent(li);
        s32 liPriority = 0;
        if (std::strstr(lrRequest.macFileName, "TRK_"))
            liPriority = std::strstr(lrRequest.macFileName, "Prop") ? 1000 : 2000;
        else if (std::strstr(lrRequest.macFileName, "GuiApt\\SaveLoadComponent.bundle"))
            liPriority = 2500;
        if (!mLoadRequestQueue.Push(&lrRequest, liPriority)
            && (CgsDev::Message::gxMessageFilterFlags & 1))
            *CgsDev::Log::gpDebugPrint << "Unable to add entry. Queue is full.\n";
    }
    const auto* lpUnloads = lpInput->GetUnloadBundleRequestQueue();
    for (s32 li = 0; li < lpUnloads->GetLength(); ++li)
        if (!mUnloadRequestQueue.Push(&lpUnloads->GetEvent(li), 0)
            && (CgsDev::Message::gxMessageFilterFlags & 1))
            *CgsDev::Log::gpDebugPrint << "Unable to add entry. Queue is full.\n";
}

void BundleLoaderModule::PostLoadFinishedEvent(BundleLoaderIO::OutputBuffer* lpOutput,
    const Events::LoadBundleRequest* lpRequest, Events::LoadBundleResponse::EResult leResult)
{
    // ARTIST828EC3C8-450: preserve destination/event/name/pool; clear the
    // request-only live-update flag, and publish the supplied result.
    CGS_ASSERT(lpOutput, "lpOutput");
    Events::LoadBundleResponse lResponse = {};
    lResponse.mpUser = lpRequest->mpUser;
    lResponse.miEventId = lpRequest->miEventId;
    lResponse.SetFileName(lpRequest->macFileName);
    lResponse.miPoolId = lpRequest->miPoolId;
    lResponse.mbLiveUpdateReplace = false;
    lResponse.meResult = leResult;
    lpOutput->GetLoadBundleResponseQueue()->AddEvent(lResponse);
}

bool BundleLoaderModule::CheckForLoads(BundleLoaderIO::OutputBuffer* lpOutput) // 828FB758
{
    const s32 liNextStream = 1 - miCurrentStream;
    if (mabStreamBuffersUsed[liNextStream] || mLoadRequestQueue.GetLength() <= 0)
        return false;
    Events::LoadBundleRequest lRequest;
    while (mLoadRequestQueue.Peek(&lRequest)) {
        // Native correction: ARTIST828FB934 compares an untagged CRC with
        // IDs tagged by ProcessBundleEntryList (828FB114), so its resident
        // fast path misses normal bundles. Use the same ID as insertion and
        // unload. Replacement requests must still read and replace the data.
        if (lRequest.mbLiveUpdateReplace)
            break;
        ID lId;
        lId.SetHash(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(lRequest.macFileName)))
                    | 0x8000000000000000ull);
        s32 liLoaded = 0;
        while (liLoaded < miMaxLoadedBundles) {
            const auto& lrLoaded = mpLoadedBundles[liLoaded];
            if (lrLoaded.miPoolId == lRequest.miPoolId && lrLoaded.mResourceId == lId)
                break;
            ++liLoaded;
        }
        if (liLoaded == miMaxLoadedBundles)
            break;
        ++mpLoadedBundles[liLoaded].miRefCount;
        PostLoadFinishedEvent(lpOutput, &lRequest, Events::LoadBundleResponse::E_RESULT_SUCCESS);
        mLoadRequestQueue.Pop(&lRequest);
        if (mLoadRequestQueue.GetLength() <= 0)
            return false;
    }
    // Avoid opening the active bundle twice while it is still being loaded.
    if (meStreamStage != STREAMSTAGE_IDLE && _stricmp(lRequest.macFileName, mLoadRequest.macFileName) == 0)
        return false;
    Events::OpenReadStreamRequest lOpen = {};
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
    maStreams[liNextStream] = nullptr;
    return true;
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
            const auto& lrLoaded = mpLoadedBundles[liSlot];
            if (lrLoaded.miPoolId == lRequest.miPoolId && lrLoaded.mResourceId == lId)
                break;
            ++liSlot;
        }
        if (liSlot == miMaxLoadedBundles) {
            CGS_ASSERT(false, "Could not find bundle in pool to unload\n");
            continue;
        }
        auto& lrLoaded = mpLoadedBundles[liSlot];
        if (lrLoaded.miRefCount == 1) {
            Events::UnloadResourceListRequest lUnload = {};
            lUnload.miEventId = liSlot;
            lUnload.miPoolId = lRequest.miPoolId;
            lUnload.mListId = lId;
            lpOutput->GetPoolSendQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lUnload), 20, sizeof(lUnload));
            --miNumLoadedBundles;
            lrLoaded.miPoolId = -1;
        }
        --lrLoaded.miRefCount;
        Events::UnloadBundleResponse lResponse = {};
        lResponse.mpUser = lRequest.mpUser;
        lResponse.miEventId = lRequest.miEventId;
        lResponse.SetFileName(lRequest.macFileName);
        lResponse.miPoolId = lRequest.miPoolId;
        lResponse.mbLiveUpdateReplace = false;
        lpOutput->GetUnloadBundleResponseQueue()->AddEvent(lResponse);
    }
    return true;
}

bool BundleLoaderModule::MoveToFirstResource() // 828D7DA8
{
    if (mAllocationResponse.mbFailed)
        return false;
    miCurrentResourcePosition = 0;
    miCurrentResource = 0;
    miCurrentMemoryType = 0;
    // Native guard for an empty resource list: the original dereferences entry0.
    if (mAllocationResponse.miNumEntries == 0)
        return false;
    if (mAllocationResponse.mpEntries[0].GetDiskSize(0) && mAllocationResponse.mpNeeds[0])
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

void BundleLoaderModule::ProcessBundleHeader() // 828D7A90
{
    std::memcpy(&mCurrentBundle, mpcHeaderBuffer, sizeof(mCurrentBundle));
    if (mpcDebugDataBuffer)
        *mpcDebugDataBuffer = 0;
    // FLAG PC-platform leaf: the existing asset pipeline emits native x64
    // platform4 bundles. Their header and entry table retain the BND2 format.
    CGS_ASSERT(mCurrentBundle.muPlatform == BundleV2::KU_PLATFORM, "Invalid platform");
    CGS_ASSERT(mCurrentBundle.muVersion == BundleV2::KU_VERSION, "Bundle version is out of date\n");
    CGS_ASSERT(mCurrentBundle.muResourceEntriesCount <= static_cast<u32>(miMaxResourcesPerBundle),
               "Bundle contains more resources than maximum defined in init options\n");
    CGS_ASSERT(mCurrentBundle.mauResourceDataOffset[0] <= static_cast<u32>(miBundleHeaderBufferSize),
               "Bundle header is larger than buffer size provided\n");
}

bool BundleLoaderModule::StreamHeaderFunc() // 829042A0
{
    CgsFileSystem::ReadStream& lrStream = maStreams[miCurrentStream];
    if (!lrStream.IsValid())
        return false;
    miHeaderPos += lrStream.Read(sizeof(BundleV2) - miHeaderPos, mpcHeaderBuffer + miHeaderPos);
    if (miHeaderPos != sizeof(BundleV2))
        return false;
    CGS_ASSERT(std::memcmp(mpcHeaderBuffer, "bnd2", 4) == 0, "Invalid bundle\n");
    ProcessBundleHeader();
    return true;
}

bool BundleLoaderModule::StreamDebugDataFunc() // 829043A0
{
    if (!mCurrentBundle.ContainsDebugData() || !mpcDebugDataBuffer)
        return true;
    const u32 luStart = mCurrentBundle.muDebugDataOffset;
    const u32 luEnd = mCurrentBundle.muResourceEntriesOffset;
    if (static_cast<u32>(miDebugBufferSize) < luEnd - luStart)
        return true;
    CgsFileSystem::ReadStream& lrStream = maStreams[miCurrentStream];
    if (static_cast<u32>(miHeaderPos) < luStart) {
        miHeaderPos += lrStream.Read(luStart - miHeaderPos, nullptr);
        if (static_cast<u32>(miHeaderPos) < luStart)
            return false;
    }
    miHeaderPos += lrStream.Read(luEnd - miHeaderPos, mpcDebugDataBuffer + miHeaderPos - luStart);
    return static_cast<u32>(miHeaderPos) >= luEnd;
}

bool BundleLoaderModule::StreamEntryListFunc(void* lpOutputBuffer) // 82904478
{
    const u32 luStart = mCurrentBundle.muResourceEntriesOffset;
    const u32 luEnd = mCurrentBundle.mauResourceDataOffset[0];
    CgsFileSystem::ReadStream& lrStream = maStreams[miCurrentStream];
    if (static_cast<u32>(miHeaderPos) < luStart) {
        miHeaderPos += lrStream.Read(luStart - miHeaderPos, nullptr);
        if (static_cast<u32>(miHeaderPos) < luStart)
            return false;
    }
    CGS_ASSERT(luEnd - luStart <= static_cast<u32>(miBundleHeaderBufferSize),
               "Bundle entry list will not fit in header buffer\n");
    miHeaderPos += lrStream.Read(luEnd - miHeaderPos, mpcHeaderBuffer + miHeaderPos - luStart);
    if (static_cast<u32>(miHeaderPos) < luEnd)
        return false;
    ProcessBundleEntryList(lpOutputBuffer);
    return true;
}

void BundleLoaderModule::ProcessBundleEntryList(void* lpOutputBuffer) // 828FAF58
{
    CGS_ASSERT(lpOutputBuffer, "lpOutput");
    char lacName[256];
    CGS_ASSERT(std::strlen(mLoadRequest.macFileName) < sizeof(lacName), "String too long");
    std::strncpy(lacName, mLoadRequest.macFileName, sizeof(lacName));
    for (char* lp = lacName; *lp; ++lp)
        if (*lp >= 'a' && *lp <= 'z') *lp -= 'a' - 'A';
    mAllocationRequest = {};
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
    auto* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
    lpOutput->GetPoolSendQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&mAllocationRequest), 16, sizeof(mAllocationRequest));
    mbStreamJobStarted = false;
    mbOnLastStreamJob = false;
    miNextFixUpRequestIndex = 0;
}

bool BundleLoaderModule::StreamDataFunc() // 82904648
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
            lrStream.Read(luDiskOffset - luPosition, nullptr);
            continue;
        }
        const u32 luRemaining = lrEntry.GetDiskSize(miCurrentMemoryType) - miCurrentResourcePosition;
        auto* lpDestination = static_cast<u8*>(
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

void BundleLoaderModule::ProcessReceiverQueue() // 828E2888
{
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liTag = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent) {
        if (liTag == 16) {
            const auto* lpResponse = reinterpret_cast<const Events::OpenReadStreamResponse*>(lpEvent);
            maStreams[lpResponse->GetEventId()] = lpResponse->GetStream();
        } else if (liTag == 18) {
            const auto* lpResponse = reinterpret_cast<const Events::CloseReadStreamResponse*>(lpEvent);
            mabStreamBuffersUsed[lpResponse->GetEventId()] = false;
        } else {
            CGS_ASSERT(false, "Unexpected event received\n");
        }
        const CgsModule::Event* lpNext = nullptr;
        liTag = mReceiverQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    mReceiverQueue.Clear();
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
    Events::FixUpAndResolveResourceListRequest lRequest = {};
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

bool BundleLoaderModule::StreamDoneFunc(void* lpOutputBuffer) // 828FB178
{
    if (mAllocationResponse.mbFailed) {
        meStreamStage = STREAMSTAGE_LOADDONE;
        return false;
    }
    CGS_ASSERT(lpOutputBuffer, "lpOutput");
    Events::FixUpAndResolveResourceListRequest lRequest = {};
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
    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    Events::CloseReadStreamRequest lRequest;
    lRequest.Construct(&mReceiverQueue, miCurrentStream, maStreams[miCurrentStream]);
    const bool lbAdded = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer)->GetStreamRequestQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 18, sizeof(lRequest));
    maStreams[miCurrentStream] = nullptr;
    return lbAdded;
}

// These remaining stages are still explicit activation gates. The resource
// module continues using its existing synchronous intake until the full
// allocation/failure/IO/job lifetime is connected and tested.
void BundleLoaderModule::ProcessPoolResponses()
{
    // ARTIST828EC148. Partial-fixup replies during DATA are intentionally
    // ignored; the final reply publishes the bundle's retained handle.
    const CgsModule::Event* lpEvent = nullptr;
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
                const auto* lpResponse = reinterpret_cast<const Events::FixUpAndResolveResourceListResponse*>(lpEvent);
                mCurrentLoad.mHandle = lpResponse->mListHandle;
                s32 liSlot = 0;
                while (liSlot < miMaxLoadedBundles && mpLoadedBundles[liSlot].miPoolId >= 0)
                    ++liSlot;
                if (liSlot < miMaxLoadedBundles)
                    mpLoadedBundles[liSlot] = mCurrentLoad;
                else
                    CGS_ASSERT(false, "Could not store loaded bundle info - out of bundles. Should have caught this during load\n");
                ++miNumLoadedBundles;
            }
        }
        const CgsModule::Event* lpNext = nullptr;
        liTag = mPoolReceiveQueueCache.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    mPoolReceiveQueueCache.Clear();
}

// Inlined in ResourceModule::Update82907948: read-lock the record buffer and
// append its native events to the loader's cache for the following update.
void BundleLoaderModule::RecordPostUpdateEvents(const BundleLoaderIO::InputBuffer_Record* lpInput)
{
    auto* lpLock = const_cast<BundleLoaderIO::InputBuffer_Record*>(lpInput);
    lpLock->LockForRead();
    const auto* lpQueue = lpInput->GetPoolReceiveQueue();
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liTag = lpQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent) {
        mPoolReceiveQueueCache.AddEvent(lpEvent, liTag, liSize);
        const CgsModule::Event* lpNext = nullptr;
        liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    lpLock->UnlockForRead();
}
bool BundleLoaderModule::StreamCompressedDataAsJobFunc(void* lpOutputBuffer)
{
    // ARTIST82900FA0..829013F4. Copy at most one 512-KiB batch into the
    // secondary buffer, whose lifetime extends until its inflate job completes.
    auto* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
    auto& lrStream = maStreams[miCurrentStream];
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
            const auto& lrEntry = mAllocationResponse.mpEntries[miCurrentResource];
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
                const auto& lrNext = mAllocationResponse.mpEntries[miCurrentResource];
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
        CGS_ASSERT(!mbOnLastStreamJob,
            "Should never finish the last resource without any data to stream!");
    }
    return false;
}
bool BundleLoaderModule::StreamIdleFunc(void* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    auto* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
    mLoadRequestQueue.Tick(1);
    mUnloadRequestQueue.Tick(1);
    if (CheckForUnloads(lpOutput))
        return false;
    RunningLoad lLoad;
    if (!mQueuedLoads.Pop(&lLoad))
        return CheckForLoads(lpOutput);
    mLoadRequest = lLoad.mLoadRequest;
    return true;
}
void BundleLoaderModule::UpdateStream(void* lpOutputBuffer) // 82906B30
{
    auto* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
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
        bool lbFinished;
        if (mCurrentBundle.IsCompressed()) {
            CGS_ASSERT(mCurrentBundle.IsMainMemOptimised() && mCurrentBundle.IsGraphicsMemOptimised(),
                       "Compressed data must be main and graphics mem optmised to use with decompression job\n");
            lbFinished = StreamCompressedDataAsJobFunc(lpOutput);
        } else {
            lbFinished = StreamDataFunc();
        }
        if (!lbFinished) break;
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
        if (mAllocationResponse.miPoolId < 0) {
            meStreamStage = STREAMSTAGE_LOADDONE;
            break;
        }
        if (!StreamDoneFunc(lpOutput)) break;
        // fall through
    case STREAMSTAGE_FIXUP:
        meStreamStage = STREAMSTAGE_FIXUP;
        if (mQueuedLoads.GetLength() == 0) CheckForLoads(lpOutput);
        break;
    case STREAMSTAGE_LOADDONE:
        if (mLoadRequest.mbLiveUpdateReplace)
            mLoadRequest.miPoolId = mAllocationResponse.miPoolId;
        PostLoadFinishedEvent(lpOutput, &mLoadRequest, mAllocationResponse.mbFailed
            ? Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY : Events::LoadBundleResponse::E_RESULT_SUCCESS);
        meStreamStage = STREAMSTAGE_IDLE;
        break;
    case STREAMSTAGE_COUNT:
        CGS_ASSERT(false, "Should never happen.");
        break;
    default:
        break;
    }
}
}
