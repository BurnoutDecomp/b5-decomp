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

            mChunkBuffer.muCurrentChunk = 0;
            mChunkBuffer.muCurrentChunkRead = 0;
            mChunkBuffer.muChunkBufferPos = 0;
            memset(mChunkBuffer.mpcBuffer, 0, mChunkBuffer.miBufferSize);
        }

        mbDecodingFrame = false;
    }
}
