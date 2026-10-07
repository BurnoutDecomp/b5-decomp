#ifndef CGS_MOVIE_PLAYER_H
#define CGS_MOVIE_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer
#include "eathread/eathread_futex.h"                                          // EA::Thread::Futex (CRITICAL_SECTION-backed lock)
#include "rw/rwcore_structs.h"

// FFmpeg backend (PC decode substitution) -- forward-declared so this header stays light.
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace renderengine { class Texture; }
namespace BrnGui { class MovieManager; }

namespace CgsGraphics
{
    class MovieVideoRenderer;
    // ARTIST player state ids, supplied Render entry and arena-backed renderer owner.
    // FFmpeg replaces transport, On2 decode and conversion to a native BGRA frame.
    // Existing wall-clock pacing/crossfade remain reconstruction debt; this repair
    // does not claim the console's asynchronous file/decode pipeline is restored.
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
        bool Prepare(rw::IResourceAllocator* lpAllocator, const char* lpcMovieFileName,
                     const char* lpcLanguageCode = 0);
        bool Release();
        void Destruct();

        enum PlayerStateType
        {
            E_RW_MOVIE_PLAYER_NULL = 0,
            E_RW_MOVIE_PLAYER_CONSTRUCTED = 1,
            E_RW_MOVIE_PLAYER_OPENING_FILE = 2,
            E_RW_MOVIE_PLAYER_LOADING_VIDEO_STREAMS = 3,
            E_RW_MOVIE_PLAYER_LOADING_FIRST_BUFFER = 4,
            E_RW_MOVIE_PLAYER_INIT_DECODERS = 5,
            E_RW_MOVIE_PLAYER_PREPARED = 6,
            E_RW_MOVIE_PLAYER_PLAYING = 7,
            E_RW_MOVIE_PLAYER_STOPPED = 8,
            E_RW_MOVIE_PLAYER_CLOSING_FILE = 9,
            E_RW_MOVIE_PLAYER_MAX = 10,
            E_STOPPED = E_RW_MOVIE_PLAYER_STOPPED,
            E_PLAYING = E_RW_MOVIE_PLAYER_PLAYING
        };

        PlayerStateType GetPlayerState() const { return mePlayerState; }
        bool            IsFinished() const     { return mbFinished; }

        // Original unit-screen rectangle; the shared screen transform maps it to the display.
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

        // Native COM allocations follow the real manager arena's disposal or
        // reconstruction. Decoder Release does not retire queued render owners.
        void RetireArenaTexturesPC();

    private:
        friend class MovieVideoRenderer;
        friend class BrnGui::MovieManager;
        bool PrepareResources();                  // open the stream (FFmpeg) for mcMovieFileName
        void ReleaseResources();                  // free the stream + frame texture
        bool DecodeFrame();                       // [PC] pull one decoded frame into mpFrame (+ mfFramePtsSec)
        bool DecodeStripFrame();                  // [PC] 3-strip VP6: one band from its own decoder chain
        bool EnsureTexture(u32 luWidth, u32 luHeight);
        void UploadFrame();                       // [PC] sws_scale mpFrame -> the renderengine texture
        f32  ComputeCrossfadeAlpha(f64 lfElapsedSec) const;

        // ---- player state and original resource ownership ------------------------------------------------------
        // Per-player lock. The X360 ctor initializes a Win32 CRITICAL_SECTION at this slot
        // (RtlInitializeCriticalSection); the committed EA::Thread::Futex is that exact
        // CRITICAL_SECTION-backed mutex, reused BY NAME, so its ctor performs the init.
        EA::Thread::Futex mLock;

        PlayerStateType mePlayerState;
        char            mcMovieFileName[256];
        bool            mbIsOkToPlay;             // stream prepared, ready to Play
        bool            mbIsLooped;
        bool            mbFinished;
        rw::IResourceAllocator* mpAllocator;
        rw::Resource mRwVideoRendererResource;
        MovieVideoRenderer* mpRwVideoRenderer;
        Im2dRenderBuffer* mpIm2dRenderBuffer;
        bool mbReadyToRender;

        f32   mfRectLeft, mfRectTop, mfRectRight, mfRectBottom;   // unit-screen rectangle
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

        // ---- X360 vertically-strided VP6 (the boot/UI *.vp6 store each 1280x720 display
        //      frame as N stacked 1280x(720/N) strips; the decoder emits the strips as
        //      separate frames, so N consecutive decodes recombine into one display frame) --
        s32              miVerticalStrips;        // 1 = normal; 3 = the X360 1280x240 3-strip form
        s32              miCurrentStrip;          // which vertical band the next decode fills (0..N-1)
        s64              miStripsDecoded;         // running count -> display index = /N, band = %N
        u8*              mpStripStaging;          // CPU BGRA buffer (width x height*N); bands accumulate
        u32              muStripStagingBytes;     // allocated size of mpStripStaging
        // The X360 3-strip *.vp6 interleaves N INDEPENDENT VP6 prediction chains (packet i belongs to
        // band i%N); each band's P-frames reference the previous SAME-band frame, so each needs its own
        // decoder context. A single shared decoder mispredicts across bands -> macroblock garbage.
        AVCodecContext*  mpStripCtx[3];           // one decoder per band chain ([0] == mpVideoCtx)
        u32              muPacketRoute;            // next demuxed video packet -> mpStripCtx[muPacketRoute%N]

        // ---- native device-resource cleanup references ---------------------------------
        // Cleanup references to the actual arena allocations, not another ring.
        // The renderer metadata can be freed before the arena/device resources.
        renderengine::Texture* mapNativeTextureOwnersPC[4];
        bool mbPausedPC;
        s32 miCurrentFrame;
        u32   muTexWidth, muTexHeight;
    };
}

#endif
