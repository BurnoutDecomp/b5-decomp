#ifndef CGS_MOVIE_PLAYER_H
#define CGS_MOVIE_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer
#include "eathread/eathread_futex.h"                                          // EA::Thread::Futex (CRITICAL_SECTION-backed lock)

// FFmpeg backend (PC decode substitution) -- forward-declared so this header stays light.
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace renderengine { class Texture; }

namespace CgsGraphics
{
    // CgsGraphics::MoviePlayer -- the full-screen boot/front-end movie player. Interface + state machine
    // reconstructed from the X360 ARTIST build (the EA-chunk + On2 VP6 architecture; DecFIGS
    // GameShared/.../Graphics/MoviePlayer/CgsMoviePlayer.{h,cpp} for the method shape, and the DWARF
    // dossier for the public surface). Driven SetMovieFile -> Prepare -> Play -> (Update per game
    // tick / Render per frame) -> Release.
    //
    // [PC DECODE SUBSTITUTION] The X360 player streams an EA-chunk container through StreamDeviceDiskRead
    // and decodes VP6 frames with the On2 VP6 SDK (xPB_INST), presenting YUV_BUFFER_CONFIG planes via
    // rw::movie::VideoRenderable. Neither the On2 SDK nor the EA-chunk codec is a buildable PC library, so
    // FFmpeg (libavformat ea/mov demux + libavcodec vp6/h264 decode + libswscale YUV->BGRA) stands in for
    // the demux+decode step (the members below the marker). Everything above the marker -- the player
    // interface, state machine, frame timing, crossfade, and the Im2d render path -- is faithful game code.
    class MoviePlayer
    {
    public:
        // X360 @0x827DD390: initializes the per-player lock (RtlInitializeCriticalSection on
        // console; mLock's Futex ctor on PC), installs the vtable, and zeroes the playback +
        // decode-stream state. The state-zeroing is shared with Construct(), which the
        // constructor delegates to.
        MoviePlayer();

        void Construct();

        // SetMovieFile records the file name (DWARF-attested signature); Prepare opens + readies the stream for
        // it (the X360 PrepareResources, which on console allocated chunk buffers + spun the decode job).
        // Release/Destruct tear the stream down. lbPreload prepares immediately instead of lazily at Play.
        bool SetMovieFile(const char* lpMovieFileName, bool lbPreload = false);
        bool Prepare(const char* lpcLanguageCode = 0);
        bool Release();
        void Destruct();

        enum PlayerStateType
        {
            E_STOPPED,
            E_PLAYING,
            E_PAUSED
        };

        PlayerStateType GetPlayerState() const { return mePlayerState; }
        bool            IsFinished() const     { return mbFinished; }

        // Sub-rectangle to present into, in the engine's logical 1280x720 screen space (the Im2d
        // convention). Defaults to the full screen.
        void SetRectangle(float fLeft, float fTop, float fRight, float fBottom);
        // Fade the quad in over the first liCrossfadeInFrames and out over the last liCrossfadeOutFrames
        // (0 = no fade). Frame->time uses the stream's frame rate.
        void SetCrossfade(int32_t liCrossfadeInFrames, int32_t liCrossfadeOutFrames);

        void Play();
        void Pause();
        void Unpause();
        void Stop();

        void Update();                                                  // advance to the frame due now
        void Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer); // present the held frame via Im2d

    private:
        bool PrepareResources();                  // open the stream (FFmpeg) for mcMovieFileName
        void ReleaseResources();                  // free the stream + frame texture
        bool DecodeFrame();                       // [PC] pull one decoded frame into mpFrame (+ mfFramePtsSec)
        bool EnsureTexture(u32 luWidth, u32 luHeight);
        void UploadFrame();                       // [PC] sws_scale mpFrame -> the renderengine texture
        f32  ComputeCrossfadeAlpha(f64 lfElapsedSec) const;

        // ---- faithful player state ------------------------------------------------------
        // Per-player lock. The X360 ctor initializes a Win32 CRITICAL_SECTION at this slot
        // (RtlInitializeCriticalSection); the committed EA::Thread::Futex is that exact
        // CRITICAL_SECTION-backed mutex, reused BY NAME, so its ctor performs the init.
        EA::Thread::Futex mLock;

        PlayerStateType mePlayerState;
        char            mcMovieFileName[256];
        bool            mbIsOkToPlay;             // stream prepared, ready to Play
        bool            mbIsLooped;
        bool            mbFinished;

        f32   mfRectLeft, mfRectTop, mfRectRight, mfRectBottom;   // logical 1280x720 px
        s32   miCrossfadeInFrames, miCrossfadeOutFrames;

        u64   muStartTick;                        // playback start (CgsSystem timer ticks)
        u64   muPauseTick;                        // tick captured at Pause (E_PAUSED)
        f64   mfFramePtsSec;                      // PTS (s) of the frame currently held in mpFrame
        f64   mfFrameRate;                        // stream avg fps (crossfade frame->sec); 0 => unknown
        f64   mfDurationSec;                      // total duration (crossfade-out anchor); 0 => unknown
        f64   mfLastElapsedSec;                   // elapsed at the last Update (Render reads it for crossfade)

        // ---- [PC DECODE SUBSTITUTION] FFmpeg backend ------------------------------------
        AVFormatContext* mpFormatCtx;
        AVCodecContext*  mpVideoCtx;
        SwsContext*      mpSws;
        AVFrame*         mpFrame;
        AVPacket*        mpPacket;
        s32              miVideoStream;
        f64              mfTimeBaseSec;           // the video stream's time_base, in seconds
        bool             mbHaveFrame;             // mpFrame holds a decoded, not-yet-shown frame
        bool             mbEof;                   // demux hit EOF; the decoder is being flushed

        // ---- frame texture (drawn through Im2d, the faithful render path) ---------------
        renderengine::Texture* mpFrameTexture;    // current frame, D3DFMT_A8R8G8B8 (BGRA in memory)
        u32   muTexWidth, muTexHeight;
    };
}

#endif
