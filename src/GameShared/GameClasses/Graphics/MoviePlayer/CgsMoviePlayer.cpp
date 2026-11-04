#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

namespace CgsGraphics
{
    // ARTIST 0x827DD390. The stream, allocator and resource records initialize themselves.
    MoviePlayer::MoviePlayer()
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

    // ARTIST 0x827EF200
    void MoviePlayer::ReleaseResources()
    {
        rw::Resource lStreamResource;
        lStreamResource.m_baseResources[0] = mpMoviePlayerBuffer;
        mePlayerState = E_RW_MOVIE_PLAYER_NULL;
        mpRwAllocator->Free(lStreamResource);

        rw::Resource lChunkResource;
        lChunkResource.m_baseResources[0] = mChunkBuffer.mpcBuffer;
        mpRwAllocator->Free(lChunkResource);

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
