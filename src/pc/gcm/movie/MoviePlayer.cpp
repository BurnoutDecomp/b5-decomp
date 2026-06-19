#include "pc/gcm/movie/MoviePlayer.h"
#include "pc/gcm/renderengine/device.h"   // gDevice, gDisplayWidth/Height
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <d3d9.h>

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

    MoviePlayer::MoviePlayer()
    {
        Construct();
    }

    void MoviePlayer::Construct()
    {
        mpFormatCtx = 0;
        mpVideoCtx = 0;
        mpSws = 0;
        mpFrame = 0;
        mpPacket = 0;
        miVideoStream = -1; 
        mfTimeBaseSec = 0.0;
        mpTexture = 0;
        muTexWidth = 0;
        muTexHeight = 0;
        muStartTick = 0; 
        mfFramePtsSec = 0.0;
        mbHaveFrame = false;
        mbPlaying = false;
        mbFinished = false;
        mbEof = false;
    }

    bool MoviePlayer::Open(const char* lpcPath)
    {
        Close();

        mbFinished = false;
        mbEof = false;
        mbHaveFrame = false;
        mbPlaying = false;

        if (avformat_open_input(&mpFormatCtx, lpcPath, 0, 0) < 0) 
        { 
            mpFormatCtx = 0; 
            return false; 
        }
        if (avformat_find_stream_info(mpFormatCtx, 0) < 0) 
        { 
            Close(); 
            return false; 
        }

        const AVCodec* lpCodec = 0;
        miVideoStream = av_find_best_stream(mpFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &lpCodec, 0);
        if (miVideoStream < 0 || lpCodec == 0) 
        { 
            Close(); 
            return false; 
        }

        AVStream* lpStream = mpFormatCtx->streams[miVideoStream];
        mpVideoCtx = avcodec_alloc_context3(lpCodec);
        if (mpVideoCtx == 0) 
        { 
            Close(); 
            return false; 
        }

        avcodec_parameters_to_context(mpVideoCtx, lpStream->codecpar);
        if (avcodec_open2(mpVideoCtx, lpCodec, 0) < 0) 
        { 
            Close(); 
            return false; 
        }

        mfTimeBaseSec = av_q2d(lpStream->time_base);
        mpFrame  = av_frame_alloc();
        mpPacket = av_packet_alloc();
        if (mpFrame == 0 || mpPacket == 0)
        {
            Close();
            return false;
        }

        CgsDev::Log::WriteToLog("[Movie] opened ");
        CgsDev::Log::WriteToLog(lpcPath);
        CgsDev::Log::WriteToLog("\n");
        return true;
    }

    void MoviePlayer::Play()
    {
        muStartTick = CgsSystem::GetSystemTimerBaseTime();
        mbPlaying = true;
    }

    bool MoviePlayer::EnsureTexture(u32 luWidth, u32 luHeight)
    {
        if (mpTexture != 0 && muTexWidth == luWidth && muTexHeight == luHeight)
            return true;
        
        if (renderengine::gDevice == 0)
            return false;

        if (mpTexture != 0) 
        {
            mpTexture->Release(); 
            mpTexture = 0; 
        }

        if (FAILED(renderengine::gDevice->CreateTexture(luWidth, luHeight, 1, 0, D3DFMT_A8R8G8B8,
                                                        D3DPOOL_MANAGED, &mpTexture, 0)))
        {
            mpTexture = 0;
            return false;
        }

        muTexWidth = luWidth; muTexHeight = luHeight;

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

    // Pull the next decoded video frame, reading + sending packets as needed; flush at EOF. Sets
    // mbFinished + returns false when the stream is fully drained.
    bool MoviePlayer::DecodeFrame()
    {
        for (;;)
        {
            int liRecv = avcodec_receive_frame(mpVideoCtx, mpFrame);
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

            // decoder wants more input
            if (mbEof) 
            { 
                avcodec_send_packet(mpVideoCtx, 0); 
                continue; 
            }
            // already draining: flush again
            int liRead = av_read_frame(mpFormatCtx, mpPacket);
            if (liRead < 0) 
            { 
                mbEof = true; 
                avcodec_send_packet(mpVideoCtx, 0); 
                continue; 
            }
            if (mpPacket->stream_index == miVideoStream)
                avcodec_send_packet(mpVideoCtx, mpPacket);
            av_packet_unref(mpPacket);
        }
    }

    void MoviePlayer::UploadFrame()
    {
        if (!EnsureTexture(static_cast<u32>(mpVideoCtx->width), static_cast<u32>(mpVideoCtx->height)))
            return;
        
        D3DLOCKED_RECT lLocked;
        if (FAILED(mpTexture->LockRect(0, &lLocked, 0, 0)))
            return;

        u8* lpDst[1] = { static_cast<u8*>(lLocked.pBits) };
        int liStride[1] = { lLocked.Pitch };

        sws_scale(mpSws, mpFrame->data, mpFrame->linesize, 0, mpVideoCtx->height, lpDst, liStride);
        mpTexture->UnlockRect(0);
    }

    void MoviePlayer::Update()
    {
        if (!mbPlaying || mbFinished || mpVideoCtx == 0)
            return;

        const u32 luFreq = CgsSystem::GetSystemTimerFrequency();
        const u32 luNow  = CgsSystem::GetSystemTimerBaseTime();
        const f64 lfElapsed = (luFreq != 0) ? (static_cast<f64>(luNow - muStartTick) / static_cast<f64>(luFreq)) : 0.0;

        // Show the latest frame whose PTS is due: decode + upload frames with PTS <= elapsed, holding
        // the first one that is still in the future (presented on a later Update).
        for (;;)
        {
            if (!mbHaveFrame)
            {
                if (!DecodeFrame())
                    break;   // EOF / drained -> mbFinished set
                mbHaveFrame = true;
            }
            if (mfFramePtsSec > lfElapsed)
                break;       // this frame is for later; keep it
            UploadFrame();
            mbHaveFrame = false;
        }
    }

    void MoviePlayer::Render()
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == 0 || mpTexture == 0)
            return;

        // Full-screen quad in the engine's 1280x720 logical space, scaled to the back buffer (matching
        // the Im2d screen-space convention). Pre-transformed (XYZRHW), opaque, textured.
        const f32 lfSx = static_cast<f32>(renderengine::gDisplayWidth)  / 1280.0f;
        const f32 lfSy = static_cast<f32>(renderengine::gDisplayHeight) / 720.0f;

        struct ScreenVtx 
        {
            float x, y, z, rhw;
            DWORD color;
            float u, v;
        };

        const DWORD luWhite = 0xFFFFFFFFu;
        ScreenVtx laQuad[4] = {
            {    0.0f,           0.0f,          0.0f, 1.0f, luWhite, 0.0f, 0.0f },
            { 1280.0f * lfSx,    0.0f,          0.0f, 1.0f, luWhite, 1.0f, 0.0f },
            {    0.0f,         720.0f * lfSy,   0.0f, 1.0f, luWhite, 0.0f, 1.0f },
            { 1280.0f * lfSx,  720.0f * lfSy,   0.0f, 1.0f, luWhite, 1.0f, 1.0f },
        };

        lpDevice->SetTexture(0, mpTexture);
        lpDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);   // opaque (ignore frame alpha)
        lpDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        lpDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        lpDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, laQuad, sizeof(ScreenVtx));
    }

    void MoviePlayer::Stop()
    {
        mbPlaying = false;
        mbFinished = true;
    }

    void MoviePlayer::Close()
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

        if (mpTexture != 0)
        {
            mpTexture->Release();
            mpTexture = 0;
        }
        
        muTexWidth = 0;
        muTexHeight = 0;
        miVideoStream = -1;

        mbHaveFrame = false;
        mbPlaying = false;   // mbFinished left as-is for the caller to read
    }
}
