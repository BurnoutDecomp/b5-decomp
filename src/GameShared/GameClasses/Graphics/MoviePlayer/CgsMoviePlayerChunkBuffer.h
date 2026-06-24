#ifndef CGS_MOVIE_PLAYER_CHUNK_BUFFER_H
#define CGS_MOVIE_PLAYER_CHUNK_BUFFER_H

#include "types.hpp"
#include <cstddef>   // size_t
#include "coreallocator/icoreallocator_interface.h"   // EA::Allocator::ICoreAllocator

// X360-native CgsGraphics movie-player support types. These are the console movie
// player's on-disk EA-chunk container helpers and its RenderWare-allocator bridge --
// distinct from the PC FFmpeg substitution in CgsMoviePlayer.h (which reuses only the
// CgsGraphics::MoviePlayer NAME for the front-end interface). Layouts recovered from the
// DecFIGS DWARF (CgsMoviePlayer.h:113/149/161) and the X360 method asm.

namespace rw { struct IResourceAllocator; }

namespace CgsGraphics
{
    // EacChunkDef @ CgsMoviePlayer.h:149. An 8-byte EA-chunk record header: a 4CC id
    // (aliased as a u32 type) followed by the chunk payload size. Stored big-endian on
    // the X360 stream; GetChunkData byteswaps muSize when validating.
    struct EacChunkDef
    {
        union
        {
            char id[4];   // +0x00  4-character chunk tag
            u32  type;    // +0x00  same bytes as a u32
        };
        u32 muSize;       // +0x04  chunk payload byte size
    };

    static const s32 KI_MAX_CHUNKS_IN_SET = 16;   // CgsMoviePlayer.h:163

    // ChunkBuffer @ CgsMoviePlayer.h:161. A ring of up to 16 EA-chunk records read into a
    // single byte buffer; mauChunkOffsets[i] is the byte offset of chunk i (including its
    // 8-byte EacChunkDef header) within mpcBuffer.
    struct ChunkBuffer
    {
        s32   miBufferSize;                          // +0x00
        char* mpcBuffer;                             // +0x04 backing byte buffer
        u32   mauChunkOffsets[KI_MAX_CHUNKS_IN_SET]; // +0x08..+0x48 per-chunk byte offset
        u32   muCurrentChunk;                        // +0x48 highest chunk index read
        u32   muCurrentChunkRead;                    // +0x4C bytes read into the current chunk
        u32   muChunkBufferPos;                      // +0x50 write cursor

        // GetChunkDef @ 0x827E9F48: pointer to chunk luIndex's EacChunkDef header.
        EacChunkDef* GetChunkDef(u32 luIndex);
        // GetChunkData @ 0x827EA0A0: pointer to chunk luIndex's payload (past the 8-byte header).
        void*        GetChunkData(u32 luIndex);
    };

    // MoviePlayerCoreAllocator @ CgsMoviePlayer.h:113. An EA::Allocator::ICoreAllocator
    // that forwards onto a RenderWare resource allocator and counts outstanding blocks.
    struct MoviePlayerCoreAllocator : public EA::Allocator::ICoreAllocator
    {
        MoviePlayerCoreAllocator();
        ~MoviePlayerCoreAllocator() override;

        void Construct(rw::IResourceAllocator* lpAllocator);
        void Destruct();

        void* Alloc(size_t nSize, const char* pName, unsigned int nFlags) override;
        void* Alloc(size_t nSize, const char* pName, unsigned int nFlags,
                    unsigned int nAlignment, unsigned int nAlignmentOffset = 0) override;
        void  Free(void* pBlock, size_t nSize = 0) override;

    private:
        int                      miAllocations;  // +0x04 (after vtable) outstanding block count
        rw::IResourceAllocator*  mpAllocator;    // +0x08 backing RenderWare allocator
    };
}

#endif // CGS_MOVIE_PLAYER_CHUNK_BUFFER_H
