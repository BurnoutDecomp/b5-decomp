#ifndef CGS_MOVIE_PLAYER_H
#define CGS_MOVIE_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayerChunkBuffer.h"   // CgsGraphics::ChunkBuffer
#include "GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.h"        // CgsFileSystem::StreamDeviceDiskRead
#include "SDKs/EATech/include/Common/vp6/vfw_pb_interface.h"                         // xPB_INST, YUV_BUFFER_CONFIG

namespace rw { namespace movie { class IVideoRenderableOperations; } }
namespace BrnGui { class MovieManager; }

namespace CgsGraphics
{
    class MovieVideoRenderer;
    // ARTIST native player; DecFIGS corroborates the member names and order.
    class MoviePlayer
    {
    public:
        MoviePlayer();
        void Construct();
        bool SetMovieFile(const char* lpcMovieFileName, const char* lpcLanguageCode, bool lbLooped);
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

        static const s32 KI_MAX_VIDEOS = 4;   // CgsMoviePlayer.h

        // CgsMoviePlayer.h -- a stream's "MVhd" chunk, copied whole (tag and length
        // included) and converted to host byte order by AddVideoStream.
        struct VP6ChunkHeader
        {
            u32 mId;
            s32 mLength;
            u32 mCompressor;
            s16 mWidth;
            s16 mHeight;
            s32 mTotalFrames;
            s32 mSuggestedBufferSize;
            s32 mFpsNumerator;
            s16 mFpsDenominator;
            u16 mFlags;
        };

        PlayerStateType GetPlayerState() const { return mePlayerState; }
        bool            IsFinished() const     { return miCurrentFrame >= maVP6Headers[0].mTotalFrames; }

        // Original unit-screen rectangle; the shared screen transform maps it to the display.
        void SetRectangle(float fLeft, float fTop, float fRight, float fBottom);
        // Fade the quad in over the first liCrossfadeInFrames and out over the last liCrossfadeOutFrames
        // (0 = no fade). Frame->time uses the stream's frame rate.
        void SetCrossfade(int32_t liCrossfadeInFrames, int32_t liCrossfadeOutFrames);

        void Play();
        void Stop();

        void Update();                                                  // advance to the frame due now
        void Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer); // present the held frame via Im2d

    private:
        friend class MovieVideoRenderer;
        friend class BrnGui::MovieManager;
        void PrepareResources(const char* lpcLanguageCode);
        void ReleaseResources();
        void ResetChunkSet();

        // ---- console chunk / VP6 decode pipeline (CgsMoviePlayer_wBT_01.cpp) ------------------
        // Read chunks until luNumChunks are complete in mChunkBuffer; false when the stream ran dry.
        bool ReadChunkSet(u32 luNumChunks);
        // Register the "MVhd" chunk luChunkIndex as the next video stream.
        void AddVideoStream(u32 luChunkIndex);
        // Queue the current chunk set's video chunks on the stream decoders.
        void StartDecodes();
        // Wait for the queued decodes, take their pictures and empty the chunk buffer.
        void WaitForDecode();

        // Movie chunk headers are little-endian; Xenon swaps them before use.
        static u32 ByteSwap(u32 luValue)
        {
            return (luValue >> 24) | ((luValue >> 8) & 0x0000FF00u)
                 | ((luValue << 8) & 0x00FF0000u) | (luValue << 24);
        }
        static s32 ByteSwap(s32 liValue) { return static_cast<s32>(ByteSwap(static_cast<u32>(liValue))); }
        static u16 ByteSwap(u16 luValue) { return static_cast<u16>((luValue >> 8) | (luValue << 8)); }
        static s16 ByteSwap(s16 lsValue) { return static_cast<s16>(ByteSwap(static_cast<u16>(lsValue))); }

        // ARTIST offsets. The critical section belongs to mMovieStream at
        // +0x1E0 (ctor827DD390), not to a separate player-level lock.
        PlayerStateType mePlayerState;                     // +0x000
        char            macMovieFileName[256];             // +0x004
        bool            mbIsLooped;                        // +0x104
        f32             mfRectLeft, mfRectTop, mfRectRight, mfRectBottom; // +0x108
        s32             miCrossfadeInFrames, miCrossfadeOutFrames; // +0x118
        s32             miVideoRender;                     // +0x120 perf monitor
        rw::IResourceAllocator* mpRwAllocator;              // +0x124
        Im2dRenderBuffer* mpIm2dRenderBuffer;                // +0x128
        xPB_INST          maVP6Decoders[KI_MAX_VIDEOS];      // +0x12C
        YUV_BUFFER_CONFIG maYUVConfigs[KI_MAX_VIDEOS];       // +0x13C
        CgsFileSystem::StreamDeviceDiskRead mMovieStream;    // +0x1E0
        void*             mpMoviePlayerBuffer;              // +0x780
        ChunkBuffer       mChunkBuffer;                     // +0x784
        VP6ChunkHeader    maVP6Headers[KI_MAX_VIDEOS];       // +0x7D8
        s32               miCurrentFrame;                   // +0x858
        u64               muStartTimeInCycles;              // +0x860
        bool              mbReadyToRender;                  // +0x868
        bool              mbDroppingFrames;                 // +0x869
        MoviePlayerCoreAllocator mAllocator;                // +0x86C
        s32               miNumVideos;                      // +0x878
        s32               miNumAudios;                      // +0x87C
        bool              mbDecodingFrame;                  // +0x880
        rw::Resource      mRwVideoRendererResource;          // +0x884
        MovieVideoRenderer* mpRwVideoRenderer;               // +0x898
        rw::Resource      mRwVideoRenderableOperationsResource; // +0x89C
        rw::movie::IVideoRenderableOperations* mpRwVideoRenderableOperations; // +0x8B0

    };
    CGS_STATIC_ASSERT((sizeof(MoviePlayer) == 0x8B8), "ARTIST native MoviePlayer extent");
}

#endif
