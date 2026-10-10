// X360-native movie-player chunk-buffer accessors + the core-allocator teardown thunk.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte-match):
//
//   CgsGraphics::ChunkBuffer::GetChunkDef                          @ 0x827E9F48
//   CgsGraphics::ChunkBuffer::GetChunkData                         @ 0x827EA0A0
//   CgsGraphics::MoviePlayerCoreAllocator::'vector deleting destructor' @ 0x827DBAC0
//   CgsGraphics::MoviePlayerCoreAllocator::Alloc (both overloads) and ::Free -- the three
//   virtuals of its vtable after the destructor. As everywhere in this build, the vtable
//   holds the aligned Alloc ahead of the plain one (overloads in reverse order).
//
// Layouts from the DecFIGS DWARF (CgsMoviePlayer.h:113/149/161). The asserts in the
// console build streamed their messages through CgsDev::StrStream into the assert
// buffer; the house CGS_ASSERT front-end forwards the plain message string directly, so
// the StrStreamBase / BasePriorityQueue::Clear call-edges in the Hex-Rays dump are
// vacuous machinery the substitution elides.

#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayerChunkBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gxMessageFilterFlags / gpDebugPrint
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStreamBase
#include "rw/rwcore_structs.h"                         // rw::IResourceAllocator / Resource / ResourceDescriptor

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

    // The plain allocation: every movie-player block comes out of the RenderWare allocator as a
    // single main-memory lane at 16-byte alignment (the other lanes keep the identity {0, 1}).
    // Logged under message filter bit 0; counts the outstanding blocks.
    void* MoviePlayerCoreAllocator::Alloc(size_t nSize, const char* /*pName*/, unsigned int /*nFlags*/)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Allocate " << static_cast<u32>(nSize) << " bytes\n";

        CGS_ASSERT(mpAllocator != 0, "mpAllocator");

        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(nSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        void* lpAlloc = mpAllocator->DoAllocate(lDescriptor, 0).m_baseResources[0];
        CGS_ASSERT(lpAlloc != 0, "lpAlloc");

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
        CGS_ASSERT(false, "Invalid allocation\n");
        return 0;
    }

    // Return a block to the RenderWare allocator as a main-memory-lane resource (the other lanes
    // null). Logged under message filter bit 0. The outstanding count is not touched here.
    void MoviePlayerCoreAllocator::Free(void* pBlock, size_t /*nSize*/)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Free " << pBlock << "\n";

        rw::Resource lResource;
        lResource.m_baseResources[0] = pBlock;
        mpAllocator->DoFree(lResource);
    }
}
