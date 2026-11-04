#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
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
