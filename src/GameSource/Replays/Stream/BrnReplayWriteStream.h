#pragma once

// BrnReplays::WriteStream -- the record-side replay stream: it accumulates per-frame
// "frunks" into an intermediate buffer and flushes them to the GPU disk stream, keeping
// a ring of StreamOffset index entries in its StreamHeader.
// DWARF home: GameSource/Replays/Stream/BrnReplayWriteStream.h:48.
//
// This home retains the reconstructed ResetStream/InvalidateFrunksAhead leaves and the
// exact initializer inlined by ReplayModule::Construct, plus the original AddFrunk/Write
// consumer chain. Start/open/pause/close operations remain separate recovery work.
//
// X360 layout note (from InvalidateFrunksAhead asm): mpStreamHeader is read at
// *(this + 0x1C). The leading mHeaderMalloc uses its canonical LinearMalloc
// type. Console offsets document original ownership; native pointers widen.

#include "types.hpp"
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/Stream/BrnReplayFrunkHeader.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc mHeaderMalloc

namespace BrnReplays
{
    // ARTIST 8265EFC8 calls GPUDiskWriteStream::AddBlock on this sink.
    class GPUDiskWriteStream;
    class ReplayModule;
    class DebugComponent;

    class WriteStream
    {
    public:
        static const s32 KI_INTERMEDIATEBUFFERSIZE = 131072;

        // ARTIST 8265EB70: s64 frame in r4, time in f1, entries/buffers/count/keyframe
        // in r6/r7/r8/r9. The original body has no completion return value.
        void AddFrunk(s64 liFrameNumber, f32 lfFrameTime,
                      FrunkSerialiserEntry* lpEntries, void** lppBuffers,
                      s32 liNumSerialisers, bool lbIsKeyFrame);

        void Construct(); // ARTIST ReplayModule::Construct initializes this embedded stream
        // ResetStream @ 0x8264D100. Free every header allocation, carve a fresh
        // StreamHeader, stamp the "REPLAY " magic, and clear the per-recording
        // bookkeeping (the GPUDiskWriteStream link mpStream is left untouched --
        // StartNewStream owns it). Returns the freshly allocated StreamHeader.
        StreamHeader* ResetStream();

        // InvalidateFrunksAhead @ 0x8264D190. After a new frunk is added at liStartFrunk,
        // mark later frunks whose disk bytes are overwritten as VOID (KU_FLAG_VOID),
        // then advance miFirstFrunk past any leading
        // run of voided non-keyframe frunks (trimming them from the live ring).
        void InvalidateFrunksAhead(s32 liStartFrunk);

        // ARTIST 8264D2D0 (raw image); retain the requested history beginning
        // at an original live keyframe, using the authored 60-frunk rate.
        void ResetStartFrame(f32 lfHistorySeconds);

    protected:
        // Test shim used by the focused invalidation fixture.
        void SetStreamHeaderForTest(StreamHeader* lpHeader) { mpStreamHeader = lpHeader; }

    private:
        friend class ReplayModule;
        friend class DebugComponent; // original block overlay reads the named stream-header pointer
        // Members recovered by ResetStream @0x8264D100 / InvalidateFrunksAhead
        // @0x8264D190; reached BY NAME (native layout, X360 byte offsets documentary).
        CgsMemory::LinearMalloc mHeaderMalloc;         // @0x00  header bump allocator (X360: 0x1C bytes)
        StreamHeader*           mpStreamHeader;        // @0x1C  the frunk index / stream header
        FrunkHeader             mCurrentFrunk;         // @0x20, real 32-byte ARTIST header
        s32                     miCurrentWriteIndex;   // @0x40  write cursor
        s32                     miStallCount;          // @0x44
        bool                    mbEnded;               // @0x48
        bool                    mbPaused;              // @0x49
        s32                     miNumFrunksAllocated;  // @0x4C
        GPUDiskWriteStream*     mpStream;              // @0x50  disk sink (NOT reset by ResetStream)
        s32                     miFilePosition;        // @0x54
        s32                     miBufferPosition;      // @0x58
        // ARTIST 8265EEC4 uses 82FDA380 directly; the buffer is class-static,
        // as in DecFIGS, and does not occupy each WriteStream instance.
        static char mpcIntermediateBuffer[KI_INTERMEDIATEBUFFERSIZE];

        // DecFIGS private Write; inlined by ARTIST 8265EEA0..8265EF94.
        s32 Write(void* lpData, s32 liSize);
    };
}
