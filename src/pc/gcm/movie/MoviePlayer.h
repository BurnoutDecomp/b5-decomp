#pragma once

#include "types.hpp"

struct IDirect3DTexture9;
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace CgsGraphics
{
    // PC movie player backed by FFmpeg (libavformat/libavcodec/libswscale). [PC DIVERGENCE]
    // The X360 CgsGraphics::MoviePlayer drives the On2 VP6 SDK (xPB_INST) over an EA-chunk container;
    // on PC the codec is treated as the SDK dependency it is -- FFmpeg demuxes + decodes (VP6 for the
    // original EA movies, H.264 for MP4), libswscale converts the frame to BGRA, and we present it as a
    // full-screen quad through the D3D9 device. Audio is a follow-on (libswresample -> the sound system).
    class MoviePlayer
    {
    public:
        MoviePlayer();

        void Construct();
        bool Open(const char* lpcPath);   // open any FFmpeg-readable container (MP4, EA, ...) for video
        void Play();                      // begin playback (start the wall clock)
        void Update();                    // decode/advance to the frame due at the current time
        void Render();                    // present the current frame full-screen (device + the movie texture)
        bool IsFinished() const { return mbFinished; }
        bool IsPlaying()  const { return mbPlaying; }
        void Stop();
        void Close();
        void Destruct() { Close(); }

    private:
        bool EnsureTexture(u32 luWidth, u32 luHeight);
        bool DecodeFrame();               // pull one decoded video frame into mpFrame (+ mfFramePtsSec)
        void UploadFrame();               // sws_scale mpFrame -> the BGRA texture

        AVFormatContext* mpFormatCtx;
        AVCodecContext*  mpVideoCtx;
        SwsContext*      mpSws;
        AVFrame*         mpFrame;
        AVPacket*        mpPacket;
        s32              miVideoStream;
        f64              mfTimeBaseSec;    // the video stream's time_base, in seconds

        IDirect3DTexture9* mpTexture;      // current frame, D3DFMT_A8R8G8B8 (BGRA in memory)
        u32  muTexWidth;
        u32  muTexHeight;

        u64  muStartTick;                  // playback start (CgsSystem timer ticks)
        f64  mfFramePtsSec;                // PTS (seconds) of the frame currently held in mpFrame
        bool mbHaveFrame;                  // mpFrame holds a decoded, not-yet-shown frame
        bool mbPlaying;
        bool mbFinished;
        bool mbEof;                        // demux hit EOF; the decoder is being flushed
    };

    // The movie currently owning the screen (set by the marketing/intro flow state; BrnRendererModule
    // ::Render draws it each frame while non-null). Option B bridge, like gBrnLoadingScreenShouldShow.
    extern MoviePlayer* gpActiveMoviePlayer;
}
