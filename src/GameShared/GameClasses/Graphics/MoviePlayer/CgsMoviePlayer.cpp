#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "GameShared/GameClasses/System/FileSystem/CgsPatchManager.h"
#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"
#include <cctype>
#include <cstring>
#include <new>

namespace CgsFileSystem
{
    extern PatchManager* gpPatchManager;
}

namespace CgsGraphics
{
    namespace
    {
        const u32 KU_MOVIE_BUFFER_SIZE = 0x100000;

        void LogMovieMemoryStatus(const char* lpcFreeMemoryLabel)
        {
            MEMORYSTATUS lStatus;
            GlobalMemoryStatus(&lStatus);
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "XBox 360 memory status: \n";
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lStatus.dwTotalPhys >> 20)
                                          << " total MB of physical memory.\n";
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lStatus.dwAvailPhys >> 20)
                                          << lpcFreeMemoryLabel;
        }
    }
    // ARTIST 0x827DD390. The stream, allocator and resource records initialize themselves.
    MoviePlayer::MoviePlayer()
        : mRwVideoRendererResource(0), mRwVideoRenderableOperationsResource(0)
    {
    }

    // ARTIST 0x827EA178
    void MoviePlayer::Construct()
    {
        mfRectLeft = 0.0f;
        mfRectTop = 0.0f;
        mpRwAllocator = 0;
        mpIm2dRenderBuffer = 0;
        mpRwVideoRenderer = 0;
        mfRectRight = 1.0f;
        mpRwVideoRenderableOperations = 0;
        mfRectBottom = 1.0f;
        mePlayerState = E_RW_MOVIE_PLAYER_NULL;
        macMovieFileName[0] = 0;
        mbIsLooped = false;
        miCrossfadeInFrames = 0;
        miCrossfadeOutFrames = 0;
        mbDecodingFrame = false;
        miVideoRender = CgsDev::PerfMonCpu::AddMonitor("Video", CgsDev::E_PMP_22, false, 25.0f, true);
    }

    // ARTIST 0x827FAE08
    void MoviePlayer::PrepareResources(const char*)
    {
        CGS_ASSERT(mePlayerState == E_RW_MOVIE_PLAYER_NULL,
                   "mePlayerState == E_RW_MOVIE_PLAYER_NULL");

        mAllocator.Construct(mpRwAllocator);
        vp6::SetAllocator(&mAllocator);

        const rw::ResourceDescriptor lRendererDescriptor = MovieVideoRenderer::GetResourceDescriptor();
        mRwVideoRendererResource =
            mpRwAllocator->DoAllocate(lRendererDescriptor, "CgsGraphics::MovieVideoDecoder object");
        void* lpRendererMemory = mRwVideoRendererResource.m_baseResources[0];
        mpRwVideoRenderer = lpRendererMemory ? new (lpRendererMemory) MovieVideoRenderer() : 0;
        mpRwVideoRenderer->SetParentMoviePlayer(this);
        mpRwVideoRenderer->Init(&mAllocator, 1280, 720);

        mMovieStream.Construct(0, 0);
        mpMoviePlayerBuffer = mpRwAllocator->AllocateMemoryResource(KU_MOVIE_BUFFER_SIZE, 128, 0);
        char* lpChunkMemory = static_cast<char*>(
            mpRwAllocator->AllocateMemoryResource(KU_MOVIE_BUFFER_SIZE, 128, 0));
        mChunkBuffer.miBufferSize = KU_MOVIE_BUFFER_SIZE;
        mChunkBuffer.mpcBuffer = lpChunkMemory;
        VP6_VPInitLibrary();
    }

    void MoviePlayer::ResetChunkSet()
    {
        mChunkBuffer.muCurrentChunk = 0;
        mChunkBuffer.muCurrentChunkRead = 0;
        mChunkBuffer.muChunkBufferPos = 0;
        std::memset(mChunkBuffer.mpcBuffer, 0, mChunkBuffer.miBufferSize);
    }

    // ARTIST 0x827FE928
    bool MoviePlayer::Prepare(rw::IResourceAllocator* lpAllocator, const char* lpMovieFileName,
                              const char* lpcLanguageCode)
    {
        switch (mePlayerState)
        {
        case E_RW_MOVIE_PLAYER_NULL:
        {
            CGS_ASSERT(lpAllocator, "lpResourceAllocator");
            mpRwAllocator = lpAllocator;
            PrepareResources(lpcLanguageCode);

            macMovieFileName[0] = 0;
            CGS_ASSERT(lpMovieFileName, "lpMovieFileName");
            CgsCore::StrCpy(macMovieFileName, sizeof(macMovieFileName), lpMovieFileName);
            for (u32 i = 0; i < std::strlen(macMovieFileName); ++i)
                macMovieFileName[i] = static_cast<char>(std::toupper(macMovieFileName[i]));

            char lacPatchedName[256];
            CgsFileSystem::gpPatchManager->RemapFilename(macMovieFileName, lacPatchedName,
                                                        sizeof(lacPatchedName));
            mMovieStream.Open(lacPatchedName, mpMoviePlayerBuffer, KU_MOVIE_BUFFER_SIZE, 16,
                              0, 0, false, -1);
            ResetChunkSet();
        }
        // Fall through to poll the stream's opening state.
        case E_RW_MOVIE_PLAYER_OPENING_FILE:
            mePlayerState = E_RW_MOVIE_PLAYER_OPENING_FILE;
            if (mMovieStream.GetStatus() != CgsFileSystem::StreamDeviceDiskRead::E_STATUS_OPEN)
                return false;

            miNumVideos = 0;
            miNumAudios = 0;
            // Fall through to read the stream headers.
        case E_RW_MOVIE_PLAYER_LOADING_VIDEO_STREAMS:
            mePlayerState = E_RW_MOVIE_PLAYER_LOADING_VIDEO_STREAMS;
            if (!ReadChunkSet(1))
                return false;
            if (std::strncmp(mChunkBuffer.GetChunkId(0), "MVhd", 4) == 0)
            {
                AddVideoStream(0);
                ResetChunkSet();
                return false;
            }
            if (std::strncmp(mChunkBuffer.GetChunkId(0), "S", 1) == 0)
            {
                ++miNumAudios;
                ResetChunkSet();
                return false;
            }
            // Fall through to read the first complete set.
        case E_RW_MOVIE_PLAYER_LOADING_FIRST_BUFFER:
            if (!ReadChunkSet(miNumAudios + miNumVideos))
                return false;

            // Fall through to start the decoders.
        case E_RW_MOVIE_PLAYER_INIT_DECODERS:
        {
            mePlayerState = E_RW_MOVIE_PLAYER_INIT_DECODERS;
            LogMovieMemoryStatus(" free MB of physical memory.\n");

            s32 liTotalHeight = 0;
            for (s32 liVideoIndex = 0; liVideoIndex < miNumVideos; ++liVideoIndex)
            {
                const VP6ChunkHeader& lrHeader = maVP6Headers[liVideoIndex];
                CGS_ASSERT(lrHeader.mWidth == maVP6Headers[0].mWidth,
                           "maVP6Headers[liVideoIndex].mWidth == maVP6Headers[0].mWidth");
                CGS_ASSERT(lrHeader.mHeight % 16 == 0,
                           "(maVP6Headers[liVideoIndex].mHeight % 16) == 0");
                CGS_ASSERT(lrHeader.mTotalFrames == maVP6Headers[0].mTotalFrames,
                           "maVP6Headers[liVideoIndex].mTotalFrames == maVP6Headers[0].mTotalFrames");
                VP6_StartDecoder(&maVP6Decoders[liVideoIndex], lrHeader.mWidth, lrHeader.mHeight,
                                 true, CgsSystem::JobManager(), 63);
                VP6_SetPbParam(maVP6Decoders[liVideoIndex], PBC_SET_POSTPROC, 0);
                liTotalHeight += lrHeader.mHeight;
            }
            CGS_ASSERT(liTotalHeight == 720, "liTotalHeight == 720");

            miCurrentFrame = 0;
            muStartTimeInCycles = 0;
            mbReadyToRender = false;
            StartDecodes();
            WaitForDecode();
            ResetChunkSet();
            LogMovieMemoryStatus("  free MB of physical memory.\n");
        }
        // Fall through to publish the prepared state.
        case E_RW_MOVIE_PLAYER_PREPARED:
            mePlayerState = E_RW_MOVIE_PLAYER_PREPARED;
            return true;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                      CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lMessage << "Unhandled state " << static_cast<s32>(mePlayerState) << "\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return false;
        }
    }

    // ARTIST 0x827EF200
    void MoviePlayer::ReleaseResources()
    {
        mePlayerState = E_RW_MOVIE_PLAYER_NULL;
        mpRwAllocator->FreeMemoryResource(mpMoviePlayerBuffer);
        mpRwAllocator->FreeMemoryResource(mChunkBuffer.mpcBuffer);

        vp6::SetAllocator(0);
        if (mpRwVideoRenderer)
        {
            mpRwAllocator->Free(mRwVideoRendererResource);
            mpRwVideoRenderer = 0;
        }
    }

    // ARTIST 0x827F7F38
    bool MoviePlayer::Release()
    {
        switch (mePlayerState)
        {
        case E_RW_MOVIE_PLAYER_NULL:
            return true;

        case E_RW_MOVIE_PLAYER_CONSTRUCTED:
        case E_RW_MOVIE_PLAYER_OPENING_FILE:
        case E_RW_MOVIE_PLAYER_LOADING_VIDEO_STREAMS:
            if (mMovieStream.GetStatus() != CgsFileSystem::StreamDeviceDiskRead::E_STATUS_CLOSED)
                mMovieStream.Close();
            ReleaseResources();
            return true;

        case E_RW_MOVIE_PLAYER_PLAYING:
            Stop();
            // Fall through to decoder teardown.
        case E_RW_MOVIE_PLAYER_INIT_DECODERS:
        case E_RW_MOVIE_PLAYER_PREPARED:
        case E_RW_MOVIE_PLAYER_STOPPED:
            for (s32 liVideo = 0; liVideo < miNumVideos; ++liVideo)
                VP6_StopDecoder(&maVP6Decoders[liVideo]);
            mMovieStream.Close();
            mePlayerState = E_RW_MOVIE_PLAYER_CLOSING_FILE;
            // Fall through to poll for closure.
        case E_RW_MOVIE_PLAYER_CLOSING_FILE:
            if (mMovieStream.GetStatus() != CgsFileSystem::StreamDeviceDiskRead::E_STATUS_CLOSED)
                return false;
            ReleaseResources();
            mpRwAllocator = 0;
            CGS_ASSERT(mePlayerState == E_RW_MOVIE_PLAYER_NULL,
                       "mePlayerState == E_RW_MOVIE_PLAYER_NULL");
            return true;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                      CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lMessage << "Unhandled state " << static_cast<s32>(mePlayerState) << "\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return false;
        }
    }

    // ARTIST 0x827EEFB0
    void MoviePlayer::Update()
    {
        if (miCurrentFrame >= maVP6Headers[0].mTotalFrames)
        {
            miCurrentFrame = maVP6Headers[0].mTotalFrames;
            mePlayerState = E_RW_MOVIE_PLAYER_STOPPED;
            return;
        }
        if (mePlayerState != E_RW_MOVIE_PLAYER_PLAYING)
            return;

        const u64 luNow = CgsSystem::GetSystemTimerBaseTime();
        s32 liNumFramesToDecode = 1;
        if (!mbReadyToRender)
        {
            muStartTimeInCycles = luNow;
        }
        else
        {
            const u64 luNumerator = static_cast<u64>(maVP6Headers[0].mFpsNumerator);
            const s32 liDenominator = maVP6Headers[0].mFpsDenominator;
            const u64 luDenominator = static_cast<u64>(liDenominator);
            const u64 luScaledCycles = ((luNow - muStartTimeInCycles) * luNumerator) / luDenominator;
            const u64 luElapsedFrames = luScaledCycles / CgsSystem::GetSystemTimerFrequency();
            const u32 luFramesDue = static_cast<u32>(luElapsedFrames) - static_cast<u32>(miCurrentFrame) + 1;
            liNumFramesToDecode = CgsNumeric::Max(static_cast<s32>(luFramesDue), 0);

            if (liNumFramesToDecode > 4)
            {
                // ARTIST multiplies the negative offset in u64 and divides it unsigned.
                const u64 luFrequency = CgsSystem::GetSystemTimerFrequency();
                const u64 luCycleOffset = luFrequency * (miCurrentFrame - 1) * (-liDenominator);
                muStartTimeInCycles = luNow + luCycleOffset / luNumerator;

                const u64 luAdjustedCycles = ((luNow - muStartTimeInCycles) * luNumerator) / luDenominator;
                const u64 luAdjustedFrames = luAdjustedCycles / CgsSystem::GetSystemTimerFrequency();
                liNumFramesToDecode = static_cast<s32>(static_cast<u32>(luAdjustedFrames) -
                                                       static_cast<u32>(miCurrentFrame) + 1);
                CGS_ASSERT(liNumFramesToDecode == 0, "liNumFramesToDecode == 0");
            }
        }

        const s32 liTotalFrames = maVP6Headers[0].mTotalFrames;
        if (miCurrentFrame + liNumFramesToDecode > liTotalFrames)
            liNumFramesToDecode = liTotalFrames - miCurrentFrame;

        mbDroppingFrames = true;
        while (liNumFramesToDecode > 0)
        {
            if (!ReadChunkSet(miNumAudios + miNumVideos))
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "Stalling to wait for stream\n";
                break;
            }

            StartDecodes();
            if (liNumFramesToDecode > 1)
                WaitForDecode();
            --liNumFramesToDecode;
            mbReadyToRender = true;
            mbDroppingFrames = false;
        }
    }

    // ARTIST 0x827FF110
    void MoviePlayer::Render(Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        mpIm2dRenderBuffer = lpIm2dRenderBuffer;
        if (mePlayerState != E_RW_MOVIE_PLAYER_PLAYING)
            return;

        CGS_ASSERT(mpIm2dRenderBuffer, "mpIm2dRenderBuffer");
        CgsDev::PerfMonCpu::StartMonitor(miVideoRender);
        if (mbReadyToRender)
        {
            s32 liRow = 0;
            for (s32 liIndex = miNumVideos - 1; liIndex >= 0; --liIndex)
            {
                rw::movie::VideoRenderable lRenderable;
                const YUV_BUFFER_CONFIG lYUVConfig = maYUVConfigs[liIndex];
                const VP6ChunkHeader& lrHeader = maVP6Headers[liIndex];
                const u32 luWidth = lrHeader.mWidth;
                const u32 luHeight = lrHeader.mHeight;

                lRenderable.SetWidth(luWidth);
                lRenderable.SetHeight(luHeight);
                lRenderable.SetFrameNumber(miCurrentFrame);
                lRenderable.SetNumBuffersUsed(3);
                lRenderable.SetFormat(rw::movie::VideoRenderable::VIDEOFORMAT_YV12);
                lRenderable.SetDropFrameFlag(false);
                lRenderable.SetReadyToRender(true);
                lRenderable.SetStride(lYUVConfig.YStride, 0);
                lRenderable.SetStride(lYUVConfig.UVStride, 1);
                lRenderable.SetStride(lYUVConfig.UVStride, 2);
                lRenderable.SetData(reinterpret_cast<u8*>(lYUVConfig.YBuffer), 0);
                lRenderable.SetData(reinterpret_cast<u8*>(lYUVConfig.UBuffer), 1);
                lRenderable.SetData(reinterpret_cast<u8*>(lYUVConfig.VBuffer), 2);
                lRenderable.SetSize(luHeight * lYUVConfig.YStride, 0);
                const u32 luChromaSize = (luHeight * lYUVConfig.UVStride) >> 2;
                lRenderable.SetSize(luChromaSize, 1);
                lRenderable.SetSize(luChromaSize, 2);

                mpRwVideoRenderer->Render(liRow, &lRenderable, liIndex == 0);
                liRow += lrHeader.mHeight;
            }

            if (!mbDroppingFrames)
                WaitForDecode();
        }
        CgsDev::PerfMonCpu::StopMonitor(miVideoRender);
    }

    // ARTIST 0x827EA448
    void MoviePlayer::SetRectangle(f32 lfLeft, f32 lfTop, f32 lfRight, f32 lfBottom)
    {
        mfRectLeft = lfLeft;
        mfRectTop = lfTop;
        mfRectRight = lfRight;
        mfRectBottom = lfBottom;
    }

    // ARTIST 0x827EA460
    void MoviePlayer::SetCrossfade(s32 liCrossfadeInFrames, s32 liCrossfadeOutFrames)
    {
        miCrossfadeInFrames = liCrossfadeInFrames;
        miCrossfadeOutFrames = liCrossfadeOutFrames;
    }

    // ARTIST 0x827EA470
    void MoviePlayer::Play()
    {
        if (mePlayerState == E_RW_MOVIE_PLAYER_PREPARED)
            mePlayerState = E_RW_MOVIE_PLAYER_PLAYING;
    }

    // ARTIST 0x827EA488
    void MoviePlayer::Stop()
    {
        const s32 liTotalFrames = maVP6Headers[0].mTotalFrames;
        mePlayerState = E_RW_MOVIE_PLAYER_STOPPED;
        miCurrentFrame = liTotalFrames;
    }
}

// FOLDED FROM CgsMoviePlayer_wBT_01.cpp
// CgsGraphics::MoviePlayer -- the console's chunked movie read and VP6 decode pipeline.
//
// A movie file is a sequence of EA chunks (an 8-byte { tag, size } header plus payload). Each frame's
// chunk set holds one video chunk per stream ("MV0K" key / "MV0F" inter frames), then the audio
// chunks. ReadChunkSet streams a set into mChunkBuffer, StartDecodes queues every video chunk on its
// stream's VP6 decoder through the job system, and WaitForDecode collects one completion per stream
// (DecodeCallback posts sDecodeSemaphore), takes the pictures and empties the buffer for the next set.
//
// The PC playback decodes through FFmpeg (CgsMoviePlayer.cpp) and does not drive these.

#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // gxMessageFilterFlags / gpDebugPrint
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStreamBase
#include "eathread/eathread_semaphore.h"                        // EA::Thread::Semaphore

#include <cstring>

// EA::Thread::kTimeoutNone: block without a timeout.
static const u32 KU_TIMEOUT_NONE = 0xFFFFFFFFu;

// Posted once per finished stream decode.
static EA::Thread::Semaphore sDecodeSemaphore;

// Completion callback of every stream decode.
static int DecodeCallback(s64 /*FrameNumber*/, char* /*YBufferStart*/, int /*YWidth*/, int /*YHeight*/,
                          void* /*Context*/, int /*Param0*/, int /*Param1*/)
{
    return sDecodeSemaphore.Post(1);
}

namespace CgsGraphics
{

    void MoviePlayer::AddVideoStream(u32 luChunkIndex)
    {
        CGS_ASSERT(strncmp(mChunkBuffer.GetChunkId(luChunkIndex), "MVhd", 4) == 0,
                   "strncmp(mChunkBuffer.GetChunkId(luChunkIndex),\"MVhd\",4) == 0");
        CGS_ASSERT(miNumVideos < KI_MAX_VIDEOS, "miNumVideos < KI_MAX_VIDEOS");

        VP6ChunkHeader& lHeader = maVP6Headers[miNumVideos];
        memcpy(&lHeader, mChunkBuffer.GetChunk(luChunkIndex), sizeof(VP6ChunkHeader));

        lHeader.mLength = ByteSwap(lHeader.mLength);
        lHeader.mWidth = ByteSwap(lHeader.mWidth);
        lHeader.mHeight = ByteSwap(lHeader.mHeight);
        lHeader.mTotalFrames = ByteSwap(lHeader.mTotalFrames);
        lHeader.mSuggestedBufferSize = ByteSwap(lHeader.mSuggestedBufferSize);
        lHeader.mFpsNumerator = ByteSwap(lHeader.mFpsNumerator);
        lHeader.mFpsDenominator = ByteSwap(lHeader.mFpsDenominator);
        lHeader.mFlags = ByteSwap(lHeader.mFlags);

        ++miNumVideos;
    }

    // Stream chunks into the buffer until luNumChunks are complete: each chunk's 8-byte header
    // first, then the rest of the size it announces. A short read leaves the partial state for the
    // next call and reports false.
    bool MoviePlayer::ReadChunkSet(u32 luNumChunks)
    {
        ChunkBuffer& lBuffer = mChunkBuffer;

        while (lBuffer.muCurrentChunk < luNumChunks)
        {
            if (lBuffer.muCurrentChunkRead == 0)
                lBuffer.mauChunkOffsets[lBuffer.muCurrentChunk] = lBuffer.muChunkBufferPos;

            if (lBuffer.muCurrentChunkRead < sizeof(EacChunkDef))
            {
                const u32 luWanted = sizeof(EacChunkDef) - lBuffer.muCurrentChunkRead;
                const u32 luRead = mMovieStream.Read(luWanted, lBuffer.mpcBuffer + lBuffer.muChunkBufferPos);
                lBuffer.muCurrentChunkRead += luRead;
                lBuffer.muChunkBufferPos += luRead;
                if (luRead < luWanted)
                    return false;
                if (lBuffer.muCurrentChunkRead < sizeof(EacChunkDef))
                    continue;
            }

            const u32 luChunkSize = ByteSwap(lBuffer.GetChunkDef(lBuffer.muCurrentChunk)->muSize);
            const u32 luWanted = luChunkSize - lBuffer.muCurrentChunkRead;
            const u32 luRead = mMovieStream.Read(luWanted, lBuffer.mpcBuffer + lBuffer.muChunkBufferPos);
            lBuffer.muCurrentChunkRead += luRead;
            lBuffer.muChunkBufferPos += luRead;
            if (luRead < luWanted)
                return false;

            if (lBuffer.muCurrentChunkRead == luChunkSize)
            {
                lBuffer.muCurrentChunkRead = 0;
                ++lBuffer.muCurrentChunk;
            }
        }

        return true;
    }

    // Queue the chunk set's video chunks (one per stream, in stream order) on the stream decoders;
    // the audio chunks that follow are only logged here.
    void MoviePlayer::StartDecodes()
    {
        ChunkBuffer& lBuffer = mChunkBuffer;

        for (s32 liIndex = 0; liIndex < miNumVideos; ++liIndex)
        {
            CGS_ASSERT(lBuffer.GetChunkId(liIndex)[0] == 'M', "lBuffer.GetChunkId(liIndex)[0] == 'M'");

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const u32 luSize = ByteSwap(lBuffer.GetChunkDef(liIndex)->muSize);
                *CgsDev::Log::gpDebugPrint << "Video chunk " << liIndex << " size: " << luSize << "\n";
            }

            const u32 luDataSize = ByteSwap(lBuffer.GetChunkDef(liIndex)->muSize) - sizeof(EacChunkDef);
            VP6_DecodeFrameToYUV_JOB(maVP6Decoders[liIndex], static_cast<char*>(lBuffer.GetChunkData(liIndex)),
                                     luDataSize, maVP6Headers[liIndex].mWidth, maVP6Headers[liIndex].mHeight,
                                     miCurrentFrame, DecodeCallback, 0, 0, 0);
        }

        for (s32 liIndex = 0; liIndex < miNumAudios; ++liIndex)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const u32 luSize = ByteSwap(lBuffer.GetChunkDef(miNumVideos + liIndex)->muSize);
                *CgsDev::Log::gpDebugPrint << "Audio chunk " << liIndex << " size: " << luSize << "\n";
            }
        }

        mbDecodingFrame = true;
    }

    void MoviePlayer::WaitForDecode()
    {
        if (mbDecodingFrame)
        {
            for (s32 liIndex = 0; liIndex < miNumVideos; ++liIndex)
                sDecodeSemaphore.Wait(&KU_TIMEOUT_NONE);

            for (s32 liIndex = 0; liIndex < miNumVideos; ++liIndex)
                VP6_GetYUVConfig(maVP6Decoders[liIndex], &maYUVConfigs[liIndex]);

            ++miCurrentFrame;

            ResetChunkSet();
            mbDecodingFrame = false;
            return;
        }

        mbDecodingFrame = false;
    }
}
