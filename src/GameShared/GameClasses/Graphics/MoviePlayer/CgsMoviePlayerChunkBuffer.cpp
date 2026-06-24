// X360-native movie-player chunk-buffer accessors + the core-allocator teardown thunk.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte-match):
//
//   CgsGraphics::ChunkBuffer::GetChunkDef                          @ 0x827E9F48
//   CgsGraphics::ChunkBuffer::GetChunkData                         @ 0x827EA0A0
//   CgsGraphics::MoviePlayerCoreAllocator::'vector deleting destructor' @ 0x827DBAC0
//
// Layouts from the DecFIGS DWARF (CgsMoviePlayer.h:113/149/161). The asserts in the
// console build streamed their messages through CgsDev::StrStream into the assert
// buffer; the house CGS_ASSERT front-end forwards the plain message string directly, so
// the StrStreamBase / BasePriorityQueue::Clear call-edges in the Hex-Rays dump are
// vacuous machinery the substitution elides.

#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayerChunkBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsGraphics
{
    // GetChunkDef @ 0x827E9F48. Bounds-checks luIndex against muCurrentChunk (a1[18],
    // offset 0x48) and -- when reading the in-progress current chunk -- that its header
    // has been fully read (muCurrentChunkRead, a1[19] @0x4C, >= 8 bytes). Returns the
    // chunk header at mpcBuffer + mauChunkOffsets[luIndex]:
    //   result = a1[luIndex + 2] + a1[1]  ==  mauChunkOffsets[luIndex] + mpcBuffer.
    EacChunkDef* ChunkBuffer::GetChunkDef(u32 luIndex)
    {
        CGS_ASSERT(luIndex <= muCurrentChunk,
                   "Chunk index has not been read");                    // CgsMoviePlayer.h:467
        CGS_ASSERT(!(luIndex == muCurrentChunk && muCurrentChunkRead < 8u),
                   "Chunk is being read, but header has not been completed"); // :471

        return reinterpret_cast<EacChunkDef*>(mpcBuffer + mauChunkOffsets[luIndex]);
    }

    // GetChunkData @ 0x827EA0A0. Validates that the chunk has a non-zero size (the asm
    // byte-swaps the big-endian-stored muSize and tests it against 0 -- a NULL-chunk
    // guard; the zero-test is endian-invariant, so it reads muSize directly here) then
    // returns the payload pointer, 8 bytes past the EacChunkDef header:
    //   result = a1[luIndex + 2] + a1[1] + 8  ==  mauChunkOffsets[luIndex] + mpcBuffer + 8.
    void* ChunkBuffer::GetChunkData(u32 luIndex)
    {
        EacChunkDef* lpChunkDef = GetChunkDef(luIndex);
        CGS_ASSERT(lpChunkDef->muSize != 0u, "NULL Chunk");             // CgsMoviePlayer.h:493

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
}
