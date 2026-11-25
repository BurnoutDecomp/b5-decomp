// X360-native movie-player chunk-buffer accessors + the core-allocator teardown thunk.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   CgsGraphics::ChunkBuffer::GetChunkDef                          @ 0x827E9F48
//   CgsGraphics::ChunkBuffer::GetChunkData                         @ 0x827EA0A0
//   CgsGraphics::MoviePlayerCoreAllocator::'vector deleting destructor' @ 0x827DBAC0
//   CgsGraphics::MoviePlayerCoreAllocator::Alloc (both overloads) and ::Free -- the three
//   virtuals of its vtable after the destructor. As everywhere in this build, the vtable
//   holds the aligned Alloc ahead of the plain one (overloads in reverse order).
//
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayerChunkBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gxMessageFilterFlags / gpDebugPrint
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStreamBase
#include "rw/rwcore_structs.h"                         // rw::IResourceAllocator / Resource / ResourceDescriptor

#define CGS_ARTIST_MOVIE_PLAYER_FILE "..\\..\\..\\GameShared\\GameClasses\\Graphics/MoviePlayer/CgsMoviePlayer.h"
#define CGS_ARTIST_MOVIE_CHUNK_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\graphics\\movieplayer\\CgsMoviePlayer.h"

namespace CgsGraphics
{
    // GetChunkDef @ 0x827E9F48. Bounds-checks luIndex against muCurrentChunk (a1[18],
    // offset 0x48) and -- when reading the in-progress current chunk -- that its header
    // has been fully read (muCurrentChunkRead, a1[19] @0x4C, >= 8 bytes). Returns the
    // chunk header at mpcBuffer + mauChunkOffsets[luIndex]:
    //   result = a1[luIndex + 2] + a1[1]  ==  mauChunkOffsets[luIndex] + mpcBuffer.
    EacChunkDef* ChunkBuffer::GetChunkDef(u32 luIndex)
    {
        CGS_ASSERT_STREAM_AT(luIndex <= muCurrentChunk,
            "Chunk index " << luIndex << " has not been read\n", CGS_ARTIST_MOVIE_CHUNK_FILE, 467);
        CGS_ASSERT_STREAM_AT(!(luIndex == muCurrentChunk && muCurrentChunkRead < sizeof(EacChunkDef)),
            "Chunk " << luIndex << " is being read, but header has not been completed\n",
            CGS_ARTIST_MOVIE_CHUNK_FILE, 471);

        return reinterpret_cast<EacChunkDef*>(mpcBuffer + mauChunkOffsets[luIndex]);
    }

    // ARTIST 827EA0A0 validates the decoded size before returning the payload.
    void* ChunkBuffer::GetChunkData(u32 luIndex)
    {
        CGS_ASSERT(GetChunkSize(luIndex) != 0,
            CGS_ASSERT_STREAM_MESSAGE_AT("NULL Chunk", CGS_ARTIST_MOVIE_CHUNK_FILE, 493));
        return mpcBuffer + mauChunkOffsets[luIndex] + sizeof(EacChunkDef);
    }

    // MoviePlayerCoreAllocator::'vector deleting destructor' @ 0x827DBAC0. The X360
    // emits the MSVC deleting-destructor thunk: re-install this allocator's vtable
    // (off_8200F5B4), run the destructor, and -- when the low bit of the flag arg is set
    // -- operator delete the object. Modelled as the destructor body; MSVC re-derives the
    // deleting thunk from this concrete virtual destructor. (The destructor has no further
    // field teardown in the asm beyond the vtable store; Destruct() owns the allocation
    // accounting.)
    MoviePlayerCoreAllocator::~MoviePlayerCoreAllocator()
    {
    }

    // The plain allocation: every movie-player block comes out of the RenderWare allocator as a
    // single main-memory lane at 16-byte alignment (the other lanes keep the identity {0, 1}).
    // Logged under message filter bit 0; counts the outstanding blocks.
    void* MoviePlayerCoreAllocator::Alloc(size_t nSize, const char* /*pName*/, unsigned int /*nFlags*/)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Allocate " << static_cast<u32>(nSize) << " bytes\n";

        CGS_ASSERT_AT(mpAllocator != 0, "mpAllocator", CGS_ARTIST_MOVIE_PLAYER_FILE, 127);

        void* lpAlloc = mpAllocator->AllocateMemoryResource(nSize, 16, 0);
        CGS_ASSERT_AT(lpAlloc != 0, "lpAlloc", CGS_ARTIST_MOVIE_PLAYER_FILE, 129);

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "    " << lpAlloc << "\n";

        ++miAllocations;
        return lpAlloc;
    }

    // The aligned allocation is not supported by the movie player: it fires the (non-gating)
    // assert and hands back null.
    void* MoviePlayerCoreAllocator::Alloc(size_t /*nSize*/, const char* /*pName*/, unsigned int /*nFlags*/,
                                          unsigned int /*nAlignment*/, unsigned int /*nAlignmentOffset*/)
    {
        CGS_ASSERT(false, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid allocation\n", CGS_ARTIST_MOVIE_PLAYER_FILE, 137));
        return 0;
    }

    // Return a block to the RenderWare allocator as a main-memory-lane resource (the other lanes
    // null). Logged under message filter bit 0. The outstanding count is not touched here.
    void MoviePlayerCoreAllocator::Free(void* pBlock, size_t /*nSize*/)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Free " << pBlock << "\n";

        mpAllocator->FreeMemoryResource(pBlock);
    }
}

#undef CGS_ARTIST_MOVIE_CHUNK_FILE
#undef CGS_ARTIST_MOVIE_PLAYER_FILE
