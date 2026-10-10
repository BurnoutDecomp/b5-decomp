#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (Im2dRenderBuffer)
#include "pc/gcm/renderengine/device.h"                              // renderengine::gDevice
#include "pc/gcm/renderengine/texture.h"                             // renderengine::Texture
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>   // std::memcpy (strip-band recombine upload)

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// Engine clock (same source the loading screen / flow states pace from; defined in CgsTimeUtils.cpp).
namespace CgsSystem
{
    u32 GetSystemTimerBaseTime();
    u32 GetSystemTimerFrequency();
}

namespace CgsGraphics
{
    namespace
    {
        const s32 KI_D3DFMT_A8R8G8B8 = 21;   // D3DFORMAT for the BGRA frame texture
        const u32 KU_PRIMITIVE_TRIANGLE_STRIP = 6u;   // renderengine::PrimitiveType (font/loading-screen path)
    }

    void MoviePlayer::Construct()
    {
        mePlayerState = E_RW_MOVIE_PLAYER_NULL;  // ARTIST 827EA190/827EA1D8: store zero.
        mcMovieFileName[0] = 0;
        mbIsOkToPlay = false;
        mbIsLooped = false;
        mbFinished = false;

        mfRectLeft = 0.0f;
        mfRectTop = 0.0f;
        mfRectRight = 1.0f;   // ARTIST 827EA1C4: unit-screen rectangle
        mfRectBottom = 1.0f;
        miCrossfadeInFrames = 0;
        miCrossfadeOutFrames = 0;

        muStartTick = 0;
        muPauseTick = 0;
        mfFramePtsSec = 0.0;
        mfFrameRate = 0.0;
        mfDurationSec = 0.0;
        mfLastElapsedSec = 0.0;

        mpFormatCtx = 0;
        mpVideoCtx = 0;
        mpSws = 0;
        mpFrame = 0;
        mpPacket = 0;
        miVideoStream = -1;
        mfTimeBaseSec = 0.0;
        mbHaveFrame = false;
        mbEof = false;

        mpAllocator = nullptr;
        mRwVideoRendererResource = rw::Resource();
        mpRwVideoRenderer = nullptr;
        mpIm2dRenderBuffer = nullptr;
        mbReadyToRender = false;
        for (auto& lpTexture : mapNativeTextureOwnersPC) lpTexture = nullptr;
        mbPausedPC = false;
        miCurrentFrame = 0;
        muTexWidth = 0;
        muTexHeight = 0;

        miVerticalStrips = 1;
        miCurrentStrip = 0;
        miStripsDecoded = 0;
        mpStripStaging = 0;
        muStripStagingBytes = 0;
        mpStripCtx[0] = 0;
        mpStripCtx[1] = 0;
        mpStripCtx[2] = 0;
        muPacketRoute = 0;

        mbDecodingFrame = false;   // no decodes queued (console Construct)
    }

    bool MoviePlayer::SetMovieFile(const char* lpMovieFileName, bool lbPreload)
    {
        if (lpMovieFileName == 0)
        {
            return false;
        }
        u32 li = 0;
        for (; li < 255u && lpMovieFileName[li] != 0; ++li)
        {
            mcMovieFileName[li] = lpMovieFileName[li];
        }
        mcMovieFileName[li] = 0;
        mbIsOkToPlay = false;
        // The native preload cannot allocate renderer resources before the
        // original Prepare call supplies their real owner.
        return lbPreload && mpAllocator != nullptr ? PrepareResources() : true;
    }

    bool MoviePlayer::Prepare(rw::IResourceAllocator* lpAllocator, const char* lpcMovieFileName,
                              const char* lpcLanguageCode)
    {
        ReleaseResources();
        mpAllocator = lpAllocator;
        SetMovieFile(lpcMovieFileName, false);
        (void)lpcLanguageCode;   // [follow-on] select the localized sound stream for this language
        return PrepareResources();
    }

    // Open the container + video stream for mcMovieFileName. [PC: FFmpeg stands in for the X360's
    // StreamDeviceDiskRead + EA-chunk parse + On2 VP6 decoder setup.]
    bool MoviePlayer::PrepareResources()
    {
        ReleaseResources();
        mbFinished = false;
        mbEof = false;
        mbHaveFrame = false;
        mePlayerState = E_RW_MOVIE_PLAYER_CONSTRUCTED;
        mbReadyToRender = false;
        mbPausedPC = false;

        if (mcMovieFileName[0] == 0)
        {
            CgsDev::Log::WriteToLog("[Movie] PrepareResources: empty filename\n");
            return false;
        }
        if (avformat_open_input(&mpFormatCtx, mcMovieFileName, 0, 0) < 0)
        {
            mpFormatCtx = 0;
            CgsDev::Log::WriteToLog("[Movie] avformat_open_input FAILED: ");
            CgsDev::Log::WriteToLog(mcMovieFileName);
            CgsDev::Log::WriteToLog("\n");
            return false;
        }
        if (avformat_find_stream_info(mpFormatCtx, 0) < 0)
        {
            ReleaseResources();
            CgsDev::Log::WriteToLog("[Movie] avformat_find_stream_info FAILED\n");
            return false;
        }

        // const AVCodec*: the FFmpeg submodule is now the Xenia fork (libavformat 59+ / FFmpeg 5.0+),
        // where av_find_best_stream / avcodec_alloc_context3 / avcodec_open2 all take a const AVCodec*
        // (the "Replace FFmpeg submodule with Xenia fork" change; this call site predates it).
        const AVCodec* lpCodec = 0;
        miVideoStream = av_find_best_stream(mpFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &lpCodec, 0);
        if (miVideoStream < 0 || lpCodec == 0)
        {
            ReleaseResources();
            CgsDev::Log::WriteToLog("[Movie] av_find_best_stream FAILED (no video stream)\n");
            return false;
        }

        AVStream* lpStream = mpFormatCtx->streams[miVideoStream];
        mpVideoCtx = avcodec_alloc_context3(lpCodec);
        if (mpVideoCtx == 0)
        {
            ReleaseResources();
            return false;
        }
        avcodec_parameters_to_context(mpVideoCtx, lpStream->codecpar);
        if (avcodec_open2(mpVideoCtx, lpCodec, 0) < 0)
        {
            ReleaseResources();
            return false;
        }

        mfTimeBaseSec = av_q2d(lpStream->time_base);
        mfFrameRate = av_q2d(lpStream->avg_frame_rate);
        mfDurationSec = (mpFormatCtx->duration > 0) ? (static_cast<f64>(mpFormatCtx->duration) / static_cast<f64>(AV_TIME_BASE)) : 0.0;

        // X360 VP6 UI/boot videos are authored as three vertically-stacked 1280x240 strips
        // per 1280x720 display frame (the VP6 stream carries 3x the container frame count;
        // 3 consecutive decoded strips = one display frame). The console's video path
        // recombined them; the PC decoder emits them separately, so detect the layout and
        // recombine below. Signature: the coded frame is 1280 wide and 240 tall.
        miVerticalStrips = (mpVideoCtx->width == 1280 && mpVideoCtx->height == 240) ? 3 : 1;
        miCurrentStrip   = 0;
        miStripsDecoded  = 0;
        muPacketRoute    = 0;
        mpStripCtx[0]    = mpVideoCtx;   // band 0 decodes through the primary context
        mpStripCtx[1]    = 0;
        mpStripCtx[2]    = 0;
        if (miVerticalStrips != 1)
        {
            // Each interleaved band is its own VP6 prediction chain -> give bands 1..N-1 their own
            // decoder contexts (band 0 reuses mpVideoCtx). Packet i%N feeds mpStripCtx[i%N].
            bool lbStripOk = true;
            for (s32 liBand = 1; liBand < miVerticalStrips; ++liBand)
            {
                AVCodecContext* lpCtx = avcodec_alloc_context3(lpCodec);
                if (lpCtx == 0 ||
                    avcodec_parameters_to_context(lpCtx, lpStream->codecpar) < 0 ||
                    avcodec_open2(lpCtx, lpCodec, 0) < 0)
                {
                    if (lpCtx != 0) avcodec_free_context(&lpCtx);
                    lbStripOk = false;
                    break;
                }
                mpStripCtx[liBand] = lpCtx;
            }
            if (!lbStripOk)
            {
                ReleaseResources();
                CgsDev::Log::WriteToLog("[Movie] 3-strip decoder alloc FAILED\n");
                return false;
            }
            CgsDev::Log::WriteToLog("[Movie] X360 3-strip VP6 detected (1280x240 -> 1280x720, 3 decoders).\n");
        }

        mpFrame = av_frame_alloc();
        mpPacket = av_packet_alloc();
        if (mpFrame == 0 || mpPacket == 0)
        {
            ReleaseResources();
            return false;
        }

        if (!EnsureTexture(static_cast<u32>(mpVideoCtx->width),
                           static_cast<u32>(mpVideoCtx->height * miVerticalStrips)))
        {
            ReleaseResources();
            return false;
        }
        mbIsOkToPlay = true;
        mePlayerState = E_RW_MOVIE_PLAYER_PREPARED;
        CgsDev::Log::WriteToLog("[Movie] prepared ");
        CgsDev::Log::WriteToLog(mcMovieFileName);
        CgsDev::Log::WriteToLog("\n");
        return true;
    }

    void MoviePlayer::ReleaseResources()
    {
        if (mpSws != 0)
        {
            sws_freeContext(mpSws);
            mpSws = 0;
        }
        if (mpFrame != 0)
        {
            av_frame_free(&mpFrame);
        }
        if (mpPacket != 0)
        {
            av_packet_free(&mpPacket);
        }
        // Free the extra band decoders first (mpStripCtx[0] aliases mpVideoCtx, freed below).
        for (s32 liB = 1; liB < 3; ++liB)
        {
            if (mpStripCtx[liB] != 0)
            {
                avcodec_free_context(&mpStripCtx[liB]);
            }
        }
        mpStripCtx[0] = 0;
        mpStripCtx[1] = 0;
        mpStripCtx[2] = 0;
        muPacketRoute = 0;
        if (mpVideoCtx != 0)
        {
            avcodec_free_context(&mpVideoCtx);
        }
        if (mpFormatCtx != 0)
        {
            avformat_close_input(&mpFormatCtx);
        }
        if (mpRwVideoRenderer != nullptr)
        {
            // ARTIST 827EF29C..2C0 frees the renderer resource, not its
            // separately allocated texture/state resources in the movie arena.
            mpAllocator->DoFree(mRwVideoRendererResource);
            mRwVideoRendererResource = rw::Resource();
            mpRwVideoRenderer = nullptr;
        }
        delete[] mpStripStaging;
        mpStripStaging = 0;
        muStripStagingBytes = 0;
        muTexWidth = 0;
        muTexHeight = 0;
        miVideoStream = -1;
        mbHaveFrame = false;
        mbReadyToRender = false;
        mePlayerState = E_RW_MOVIE_PLAYER_NULL;
    }

    bool MoviePlayer::Release()
    {
        // ARTIST 827F7FC8..8034 stops playback and closes before resource
        // release. FFmpeg's native close is synchronous; no fictitious frame
        // delay or disk completion is inserted into the engine state machine.
        if (mePlayerState == E_PLAYING)
            Stop();
        if (mePlayerState != E_RW_MOVIE_PLAYER_NULL)
            mePlayerState = E_RW_MOVIE_PLAYER_CLOSING_FILE;
        ReleaseResources();
        mpAllocator = nullptr; // ARTIST 827F8044: close releases the allocator association.
        mbIsOkToPlay = false;
        return true;
    }

    void MoviePlayer::RetireArenaTexturesPC()
    {
        // FLAG PC-platform leaf: COM image allocations are outside the arena.
        // Retire them at that owner's real disposal/reconstruction boundary;
        // the wrapper/state memory is released with the enclosing arena.
        for (auto& lpTexture : mapNativeTextureOwnersPC)
        {
            if (lpTexture != nullptr)
                renderengine::Texture::Destroy(lpTexture);
            lpTexture = nullptr;
        }
    }

    void MoviePlayer::Destruct()
    {
        Release();
    }

    void MoviePlayer::Play()
    {
        if (!mbIsOkToPlay && !PrepareResources())
        {
            return;   // nothing to play
        }
        muStartTick = CgsSystem::GetSystemTimerBaseTime();
        mfLastElapsedSec = 0.0;
        mbFinished = false;
        mePlayerState = E_PLAYING;
        mbPausedPC = false;
    }

    void MoviePlayer::Pause()
    {
        if (mePlayerState == E_PLAYING)
        {
            muPauseTick = CgsSystem::GetSystemTimerBaseTime();
            mbPausedPC = true;
        }
    }

    void MoviePlayer::Unpause()
    {
        if (mbPausedPC)
        {
            // Shift the start tick forward by the paused span so elapsed resumes where it stopped.
            muStartTick += (CgsSystem::GetSystemTimerBaseTime() - muPauseTick);
            mePlayerState = E_PLAYING;
            mbPausedPC = false;
        }
    }

    void MoviePlayer::Stop()
    {
        mePlayerState = E_STOPPED;
        miCurrentFrame = static_cast<s32>(mfDurationSec * mfFrameRate);
        mbFinished = true;
    }

    void MoviePlayer::SetRectangle(float fLeft, float fTop, float fRight, float fBottom)
    {
        mfRectLeft = fLeft;
        mfRectTop = fTop;
        mfRectRight = fRight;
        mfRectBottom = fBottom;
    }

    void MoviePlayer::SetCrossfade(int32_t liCrossfadeInFrames, int32_t liCrossfadeOutFrames)
    {
        miCrossfadeInFrames = liCrossfadeInFrames;
        miCrossfadeOutFrames = liCrossfadeOutFrames;
    }

    // 3-strip VP6: N interleaved INDEPENDENT prediction chains (packet i belongs to band i%N).
    // Output the next strip in (display,band) order from its OWN decoder context; feed packets in
    // file order routed by muPacketRoute%N so each band's P-frames reference the previous same-band
    // frame. A shared decoder would mispredict across bands -> macroblock garbage.
    bool MoviePlayer::DecodeStripFrame()
    {
        const s32 liN = miVerticalStrips;
        const s32 liBand = static_cast<s32>(miStripsDecoded % liN);
        AVCodecContext* lpCtx = mpStripCtx[liBand];
        for (;;)
        {
            const int liRecv = avcodec_receive_frame(lpCtx, mpFrame);
            if (liRecv == 0)
            {
                miCurrentStrip = liBand;
                // The container rate is already the authored display-frame rate. Each
                // packet yields the N decoded bands for that display frame, so divide
                // only the decoded-strip index, never the stream's frame rate.
                const s64 liDisplayIndex = miStripsDecoded / liN;
                const f64 lfFps = (mfFrameRate > 1.0) ? mfFrameRate : 30.0;
                mfFramePtsSec = static_cast<f64>(liDisplayIndex) / lfFps;
                ++miStripsDecoded;
                return true;
            }
            if (liRecv == AVERROR_EOF || liRecv != AVERROR(EAGAIN))
            {
                mbFinished = true;
                return false;
            }
            // This band's decoder wants more input.
            if (mbEof)
            {
                avcodec_send_packet(lpCtx, 0);   // keep flushing this band
                continue;
            }
            const int liRead = av_read_frame(mpFormatCtx, mpPacket);
            if (liRead < 0)
            {
                mbEof = true;
                for (s32 liB = 0; liB < liN; ++liB)
                {
                    avcodec_send_packet(mpStripCtx[liB], 0);   // begin draining every band
                }
                continue;
            }
            if (mpPacket->stream_index == miVideoStream)
            {
                avcodec_send_packet(mpStripCtx[muPacketRoute % static_cast<u32>(liN)], mpPacket);
                ++muPacketRoute;
            }
            av_packet_unref(mpPacket);
        }
    }

    // Pull the next decoded video frame, reading + sending packets as needed; flush at EOF. Sets
    // mbFinished + returns false when the stream is fully drained. [PC FFmpeg decode loop.]
    bool MoviePlayer::DecodeFrame()
    {
        if (miVerticalStrips != 1)
        {
            return DecodeStripFrame();
        }
        for (;;)
        {
            const int liRecv = avcodec_receive_frame(mpVideoCtx, mpFrame);
            if (liRecv == 0)
            {
                s64 lts = mpFrame->best_effort_timestamp;
                if (lts == AV_NOPTS_VALUE) lts = mpFrame->pts;
                if (lts != AV_NOPTS_VALUE) mfFramePtsSec = static_cast<f64>(lts) * mfTimeBaseSec;
                return true;
            }
            if (liRecv == AVERROR_EOF)
            {
                mbFinished = true;
                return false;
            }
            if (liRecv != AVERROR(EAGAIN))
            {
                mbFinished = true;
                return false;
            }

            // The decoder wants more input.
            if (mbEof)
            {
                avcodec_send_packet(mpVideoCtx, 0);   // keep flushing
                continue;
            }
            const int liRead = av_read_frame(mpFormatCtx, mpPacket);
            if (liRead < 0)
            {
                mbEof = true;
                avcodec_send_packet(mpVideoCtx, 0);   // begin draining
                continue;
            }
            if (mpPacket->stream_index == miVideoStream)
            {
                avcodec_send_packet(mpVideoCtx, mpPacket);
            }
            av_packet_unref(mpPacket);
        }
    }

    bool MoviePlayer::EnsureTexture(u32 luWidth, u32 luHeight)
    {
        if (mpRwVideoRenderer != nullptr)
            return muTexWidth == luWidth && muTexHeight == luHeight;
        CGS_ASSERT(mpAllocator != nullptr, "mpAllocator");
        rw::ResourceDescriptor lDescriptor = MovieVideoRenderer::GetResourceDescriptor();
        mRwVideoRendererResource = mpAllocator->DoAllocate(lDescriptor, nullptr);
        if (mRwVideoRendererResource.m_baseResources[0] == nullptr)
            return false;
        mpRwVideoRenderer = MovieVideoRenderer::Initialize(mRwVideoRendererResource);
        mpRwVideoRenderer->SetParentMoviePlayer(this);
        mpRwVideoRenderer->Init(mpAllocator, luWidth, luHeight);
        bool lbComplete = true;
        for (u32 luSlot = 0; luSlot < MovieVideoRenderer::KU_NUM_TEXTURES; ++luSlot)
        {
            renderengine::Texture* lpTexture = mpRwVideoRenderer->maTextureInfoTypes[luSlot].mpTexture;
            mapNativeTextureOwnersPC[luSlot] = lpTexture;
            lbComplete &= lpTexture != nullptr && lpTexture->mpD3DTexture != nullptr
                && mpRwVideoRenderer->maTextureInfoTypes[luSlot].mpTextureState != nullptr;
        }
        muTexWidth = luWidth;
        muTexHeight = luHeight;
        mpSws = sws_getContext(mpVideoCtx->width, mpVideoCtx->height, mpVideoCtx->pix_fmt,
            mpVideoCtx->width, mpVideoCtx->height, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        return lbComplete && mpSws != nullptr;
    }

    void MoviePlayer::UploadFrame()
    {
        // The texture spans all N strip bands (N==1 for normal video). Repeated texture
        // Lock/Unlock cycles do NOT reliably preserve the other bands between writes, so
        // accumulate the N strips in a persistent CPU BGRA buffer (sws each strip into its
        // band) and publish the recombined frame to the GPU texture in ONE lock on the last
        // strip. For N==1 this is one strip + one upload (unchanged behaviour).
        const u32 luStripW    = static_cast<u32>(mpVideoCtx->width);
        const u32 luStripH    = static_cast<u32>(mpVideoCtx->height);
        const u32 luTexHeight = luStripH * static_cast<u32>(miVerticalStrips);
        if (!EnsureTexture(luStripW, luTexHeight))
        {
            return;
        }

        const u32 luBufPitch = luStripW * 4u;
        const u32 luBufBytes = luBufPitch * luTexHeight;
        if (mpStripStaging == 0 || muStripStagingBytes < luBufBytes)
        {
            delete[] mpStripStaging;
            mpStripStaging      = new u8[luBufBytes];
            muStripStagingBytes = luBufBytes;
        }

        const u32 luBandOffset = static_cast<u32>(miCurrentStrip) * luStripH * luBufPitch;
        u8* lpDst[1] = { mpStripStaging + luBandOffset };
        int liStride[1] = { static_cast<int>(luBufPitch) };
        sws_scale(mpSws, mpFrame->data, mpFrame->linesize, 0, mpVideoCtx->height, lpDst, liStride);

        // Publish only once the display frame is complete (all N strips uploaded).
        if (miCurrentStrip != miVerticalStrips - 1)
        {
            return;
        }

        // Completed CPU codec output is published through the renderer's selected
        // original texture slot during Render, not into the previous dispatch bank.
        mbReadyToRender = true;
        miCurrentFrame = miVerticalStrips > 1
            ? static_cast<s32>(miStripsDecoded / miVerticalStrips - 1)
            : static_cast<s32>(mfFramePtsSec * mfFrameRate);
    }

    void MoviePlayer::Update()
    {
        if (mePlayerState != E_PLAYING || mbPausedPC || mbFinished || mpVideoCtx == 0)
        {
            return;
        }

        const u32 luFreq = CgsSystem::GetSystemTimerFrequency();
        const u32 luNow = CgsSystem::GetSystemTimerBaseTime();
        const f64 lfElapsed = (luFreq != 0) ? (static_cast<f64>(luNow - muStartTick) / static_cast<f64>(luFreq)) : 0.0;
        mfLastElapsedSec = lfElapsed;

        // Present the latest frame whose PTS is due: decode + upload frames with PTS <= elapsed, holding
        // the first one still in the future (shown on a later Update).
        for (;;)
        {
            if (!mbHaveFrame)
            {
                if (!DecodeFrame())
                {
                    break;   // EOF / drained -> mbFinished set
                }
                mbHaveFrame = true;
            }
            if (mfFramePtsSec > lfElapsed)
            {
                break;       // this frame is for later; keep it
            }
            UploadFrame();
            mbHaveFrame = false;
        }
    }

    // Fade alpha [0,1] for the current elapsed time: ramps up over the crossfade-in span and down over
    // the crossfade-out span (frame counts -> seconds via the stream frame rate). 1.0 when no fade set.
    f32 MoviePlayer::ComputeCrossfadeAlpha(f64 lfElapsedSec) const
    {
        f32 lfAlpha = 1.0f;
        const f64 lfFps = (mfFrameRate > 1.0) ? mfFrameRate : 30.0;

        if (miCrossfadeInFrames > 0)
        {
            const f64 lfInDur = static_cast<f64>(miCrossfadeInFrames) / lfFps;
            if (lfInDur > 0.0 && lfElapsedSec < lfInDur)
            {
                lfAlpha = static_cast<f32>(lfElapsedSec / lfInDur);
            }
        }
        if (miCrossfadeOutFrames > 0 && mfDurationSec > 0.0)
        {
            const f64 lfOutDur = static_cast<f64>(miCrossfadeOutFrames) / lfFps;
            const f64 lfRemain = mfDurationSec - lfElapsedSec;
            if (lfOutDur > 0.0 && lfRemain < lfOutDur)
            {
                const f32 lfOut = static_cast<f32>(lfRemain / lfOutDur);
                if (lfOut < lfAlpha) lfAlpha = lfOut;
            }
        }
        if (lfAlpha < 0.0f) lfAlpha = 0.0f;
        if (lfAlpha > 1.0f) lfAlpha = 1.0f;
        return lfAlpha;
    }

    // ARTIST 827FF110: latch the supplied buffer, then gate on PLAYING and the
    // actual decoded-frame readiness before invoking the real movie renderer.
    void MoviePlayer::Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        mpIm2dRenderBuffer = lpIm2dRenderBuffer;
        if (mePlayerState != E_PLAYING || !mbReadyToRender)
            return;
        CGS_ASSERT(mpIm2dRenderBuffer != nullptr, "mpIm2dRenderBuffer");
        rw::movie::VideoRenderable lFrame;
        lFrame.SetData(mpStripStaging, 0);
        lFrame.SetSize(muTexWidth * muTexHeight * 4u, 0);
        lFrame.SetStride(muTexWidth * 4u, 0);
        lFrame.SetWidth(muTexWidth);
        lFrame.SetHeight(muTexHeight);
        lFrame.SetFormat(rw::movie::VideoRenderable::VIDEOFORMAT_ARGB32);
        lFrame.SetFrameNumber(miCurrentFrame);
        lFrame.SetNumBuffersUsed(1);
        lFrame.SetReadyToRender(true);
        // FLAG PC-platform leaf: the decoder has already combined the original
        // three YUV strips into one native BGRA display frame. The owner still
        // receives the real supplied payload and advances its original four slots.
        mpRwVideoRenderer->Render(0, &lFrame, true);
    }
}
