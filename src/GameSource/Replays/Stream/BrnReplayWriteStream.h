#pragma once

// BrnReplays::WriteStream -- the record-side replay stream: it accumulates per-frame
// "frunks" into an intermediate buffer and flushes them to a DiskWriteStream, keeping
// a ring of StreamOffset index entries in its StreamHeader.
// DWARF home: GameSource/Replays/Stream/BrnReplayWriteStream.h:48.
//
// MINIMAL SLICE created for the InvalidateFrunksAhead @0x8264D190 leaf. That function
// touches only the StreamHeader pointer (mpStreamHeader). ResetStream and
// ResetStartFrame now use the real allocator and index fields below; the
// AddFrunk path is restored from raw ARTIST 0x8265EB70. StartNewStream/PauseStream
// remain to be recovered. GROW this home additively; do NOT fork it.
//
// X360 layout note (from InvalidateFrunksAhead asm): mpStreamHeader is read at
// *(this + 0x1C). The leading mHeaderMalloc uses its canonical LinearMalloc
// type. Console offsets document original ownership; native pointers widen.

#include "types.hpp"
#include "GameSource/Replays/Stream/BrnReplayFrunkHeader.h"
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc mHeaderMalloc

namespace BrnReplays
{
    class ReplayModule;

    class GPUDiskWriteStream;

    class WriteStream
    {
        // ReplayModule owns this stream inline and the ARTIST constructor/recording
        // state machine initialize its original subfields directly.
        friend class ReplayModule;

    public:
        // ResetStream @ 0x8264D100. Free every header allocation, carve a fresh
        // StreamHeader, stamp the "REPLAY " magic, and clear the per-recording
        // bookkeeping (the DiskWriteStream link mpStream is left untouched --
        // StartNewStream owns it). Returns the freshly allocated StreamHeader.
        StreamHeader* ResetStream();

        // ARTIST raw body @ 0x8265EB70. Assemble one frunk header, its serialiser
        // entries, and their payloads in the original shared 128 KiB scratch buffer,
        // then submit the rounded blocks to the embedded GPU disk writer.
        void AddFrunk(s32 liFrameIndex, f32 lfFrameTime,
                      FrunkSerialiserEntry* lpEntries, void** lppBlocks,
                      s32 liNumEntries, bool lbIsKeyFrame);

        // InvalidateFrunksAhead @ 0x8264D190. After a new frunk is added at liStartFrunk,
        // mark later frunks whose disk bytes are overwritten as VOID (KU_FLAG_VOID),
        // then advance miFirstFrunk past any leading
        // run of voided non-keyframe frunks (trimming them from the live ring).
        void InvalidateFrunksAhead(s32 liStartFrunk);

        // ARTIST 8264D2D0 (raw image); retain the requested history beginning
        // at an original live keyframe, using the authored 60-frunk rate.
        void ResetStartFrame(f32 lfHistorySeconds);

    protected:
        // Construction lives in the full WriteStream TU; until then, derived test
        // shims set the header pointer this slice's bodied function reads. Reaches
        // mpStreamHeader BY NAME (no offset cast).
        void SetStreamHeaderForTest(StreamHeader* lpHeader) { mpStreamHeader = lpHeader; }

    private:
        // Members recovered by ResetStream @0x8264D100, AddFrunk @0x8265EB70, and
        // InvalidateFrunksAhead @0x8264D190; reached BY NAME (native layout, X360 byte
        // offsets documentary).
        CgsMemory::LinearMalloc mHeaderMalloc;         // @0x00  header bump allocator (X360: 0x1C bytes)
        StreamHeader*           mpStreamHeader;        // @0x1C  the frunk index / stream header
        FrunkHeader             mCurrentFrunkHeader;   // @0x20  staged 0x20-byte frunk header
        s32                     miCurrentWriteIndex;   // @0x40  write cursor
        s32                     miStallCount;          // @0x44
        bool                    mbEnded;               // @0x48
        bool                    mbPaused;              // @0x49
        s32                     miNumFrunksAllocated;  // @0x4C
        GPUDiskWriteStream*     mpStream;              // @0x50  disk sink (NOT reset by ResetStream)
        s32                     miFilePosition;        // @0x54
        s32                     miBufferPosition;      // @0x58
        // ARTIST AddFrunk's 0x20000-byte scratch area is a separate shared buffer at
        // 0x82FDA380, not an embedded WriteStream tail. Later stream members remain open.
    };
}
