#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (Im2dRenderBuffer)
#include "pc/gcm/renderengine/device.h"                              // renderengine::gDevice
#include "pc/gcm/renderengine/texture.h"                             // renderengine::Texture
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

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
    MoviePlayer* gpActiveMoviePlayer = 0;

    namespace
    {
        const s32 KI_D3DFMT_A8R8G8B8 = 21;   // D3DFORMAT for the BGRA frame texture
        const u32 KU_PRIMITIVE_TRIANGLE_STRIP = 6u;   // renderengine::PrimitiveType (font/loading-screen path)
    }

    void MoviePlayer::Construct()
    {
        mePlayerState = E_STOPPED;
        mcMovieFileName[0] = 0;
        mbIsOkToPlay = false;
        mbIsLooped = false;
        mbFinished = false;

        mfRectLeft = 0.0f;
        mfRectTop = 0.0f;
        mfRectRight = 1280.0f;   // full logical screen
        mfRectBottom = 720.0f;
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

        mpFrameTexture = 0;
        muTexWidth = 0;
        muTexHeight = 0;
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
        return lbPreload ? PrepareResources() : true;
    }

    bool MoviePlayer::Prepare(const char* lpcLanguageCode)
    {
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
        mePlayerState = E_STOPPED;

        if (mcMovieFileName[0] == 0)
        {
            return false;
        }
        if (avformat_open_input(&mpFormatCtx, mcMovieFileName, 0, 0) < 0)
        {
            mpFormatCtx = 0;
            return false;
        }
        if (avformat_find_stream_info(mpFormatCtx, 0) < 0)
        {
            ReleaseResources();
            return false;
        }

        const AVCodec* lpCodec = 0;
        miVideoStream = av_find_best_stream(mpFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &lpCodec, 0);
        if (miVideoStream < 0 || lpCodec == 0)
        {
            ReleaseResources();
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

        mpFrame = av_frame_alloc();
        mpPacket = av_packet_alloc();
        if (mpFrame == 0 || mpPacket == 0)
        {
            ReleaseResources();
            return false;
        }

        mbIsOkToPlay = true;
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
        if (mpVideoCtx != 0)
        {
            avcodec_free_context(&mpVideoCtx);
        }
        if (mpFormatCtx != 0)
        {
            avformat_close_input(&mpFormatCtx);
        }
        if (mpFrameTexture != 0)
        {
            renderengine::Texture::Destroy(mpFrameTexture);
            delete mpFrameTexture;
            mpFrameTexture = 0;
        }
        muTexWidth = 0;
        muTexHeight = 0;
        miVideoStream = -1;
        mbHaveFrame = false;
    }

    bool MoviePlayer::Release()
    {
        ReleaseResources();
        mePlayerState = E_STOPPED;
        mbIsOkToPlay = false;
        return true;
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
    }

    void MoviePlayer::Pause()
    {
        if (mePlayerState == E_PLAYING)
        {
            muPauseTick = CgsSystem::GetSystemTimerBaseTime();
            mePlayerState = E_PAUSED;
        }
    }

    void MoviePlayer::Unpause()
    {
        if (mePlayerState == E_PAUSED)
        {
            // Shift the start tick forward by the paused span so elapsed resumes where it stopped.
            muStartTick += (CgsSystem::GetSystemTimerBaseTime() - muPauseTick);
            mePlayerState = E_PLAYING;
        }
    }

    void MoviePlayer::Stop()
    {
        mePlayerState = E_STOPPED;
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

    // Pull the next decoded video frame, reading + sending packets as needed; flush at EOF. Sets
    // mbFinished + returns false when the stream is fully drained. [PC FFmpeg decode loop.]
    bool MoviePlayer::DecodeFrame()
    {
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
        if (mpFrameTexture != 0 && muTexWidth == luWidth && muTexHeight == luHeight)
        {
            return true;
        }
        if (renderengine::gDevice == 0)
        {
            return false;
        }
        if (mpFrameTexture != 0)
        {
            renderengine::Texture::Destroy(mpFrameTexture);
            delete mpFrameTexture;
            mpFrameTexture = 0;
        }

        renderengine::Texture* lpTexture = new renderengine::Texture();   // value-init: POD fields zeroed
        renderengine::Texture::Parameters lParams = {};
        lParams.miFormat = KI_D3DFMT_A8R8G8B8;
        lParams.muWidth = luWidth;
        lParams.muHeight = luHeight;
        lParams.muDepth = 1u;
        lParams.muNumLevels = 1u;
        renderengine::Texture::Create(lpTexture, &lParams, 0);            // empty (uploaded each frame)
        if (lpTexture->mpD3DTexture == 0)
        {
            delete lpTexture;
            return false;
        }
        mpFrameTexture = lpTexture;
        muTexWidth = luWidth;
        muTexHeight = luHeight;

        if (mpSws != 0)
        {
            sws_freeContext(mpSws);
            mpSws = 0;
        }
        mpSws = sws_getContext(mpVideoCtx->width, mpVideoCtx->height, mpVideoCtx->pix_fmt,
                               static_cast<int>(luWidth), static_cast<int>(luHeight),
                               AV_PIX_FMT_BGRA, SWS_BILINEAR, 0, 0, 0);
        return mpSws != 0;
    }

    void MoviePlayer::UploadFrame()
    {
        if (!EnsureTexture(static_cast<u32>(mpVideoCtx->width), static_cast<u32>(mpVideoCtx->height)))
        {
            return;
        }
        renderengine::Texture::LockInfo lLock;
        renderengine::Texture::Lock(mpFrameTexture, 0, 0, 0, &lLock);
        if (lLock.mpBits == 0)
        {
            return;
        }
        u8* lpDst[1] = { static_cast<u8*>(lLock.mpBits) };
        int liStride[1] = { static_cast<int>(lLock.muPitch) };
        sws_scale(mpSws, mpFrame->data, mpFrame->linesize, 0, mpVideoCtx->height, lpDst, liStride);
        renderengine::Texture::Unlock(mpFrameTexture, &lLock);
    }

    void MoviePlayer::Update()
    {
        if (mePlayerState != E_PLAYING || mbFinished || mpVideoCtx == 0)
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

    // Present the held frame as a textured quad through the immediate-mode renderer -- the same Im2d path
    // the loading screen + bitmap font draw through (RenderStart/RenderEnd a 4-vertex triangle strip),
    // replacing the standalone D3D fullscreen quad. The faithful X360 Render builds a VideoRenderable.
    void MoviePlayer::Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        if (lpIm2dRenderBuffer == 0 || mpFrameTexture == 0 || mpFrameTexture->mpD3DTexture == 0)
        {
            return;
        }

        const f32 lfAlpha = ComputeCrossfadeAlpha(mfLastElapsedSec);
        const RGBA8 lColour = { 255u, 255u, 255u, static_cast<u8>(lfAlpha * 255.0f) };

        lpIm2dRenderBuffer->BeginRendering();
        lpIm2dRenderBuffer->SetTexture(mpFrameTexture);
        Basic2dColouredTexturedVertex* lpVtx = lpIm2dRenderBuffer->RenderStart(4u);
        if (lpVtx != 0)
        {
            // Triangle strip: TL, TR, BL, BR.
            lpVtx[0].mv2Pos.x = mfRectLeft;  lpVtx[0].mv2Pos.y = mfRectTop;
            lpVtx[0].mv2Tex0UV.x = 0.0f;     lpVtx[0].mv2Tex0UV.y = 0.0f;  lpVtx[0].mv4Colour = lColour;
            lpVtx[1].mv2Pos.x = mfRectRight; lpVtx[1].mv2Pos.y = mfRectTop;
            lpVtx[1].mv2Tex0UV.x = 1.0f;     lpVtx[1].mv2Tex0UV.y = 0.0f;  lpVtx[1].mv4Colour = lColour;
            lpVtx[2].mv2Pos.x = mfRectLeft;  lpVtx[2].mv2Pos.y = mfRectBottom;
            lpVtx[2].mv2Tex0UV.x = 0.0f;     lpVtx[2].mv2Tex0UV.y = 1.0f;  lpVtx[2].mv4Colour = lColour;
            lpVtx[3].mv2Pos.x = mfRectRight; lpVtx[3].mv2Pos.y = mfRectBottom;
            lpVtx[3].mv2Tex0UV.x = 1.0f;     lpVtx[3].mv2Tex0UV.y = 1.0f;  lpVtx[3].mv4Colour = lColour;
            lpIm2dRenderBuffer->RenderEnd(static_cast<renderengine::PrimitiveType>(KU_PRIMITIVE_TRIANGLE_STRIP),
                                          lpVtx, 4u);
        }
        lpIm2dRenderBuffer->EndRendering();
    }
}
