#pragma once

// BrnReplays::WriteStream -- the record-side replay stream: it accumulates per-frame
// "frunks" into an intermediate buffer and flushes them to a DiskWriteStream, keeping
// a ring of StreamOffset index entries in its StreamHeader.
// DWARF home: GameSource/Replays/Stream/BrnReplayWriteStream.h:48.
//
// MINIMAL SLICE created for the InvalidateFrunksAhead @0x8264D190 leaf. That function
// touches only the StreamHeader pointer (mpStreamHeader), so this slice models the
// leading members up to and including mpStreamHeader and leaves the rest of the class
// (the intermediate buffer, the DiskWriteStream link, AddFrunk/StartNewStream/
// PauseStream/ResetStartFrame, etc.) to the full WriteStream TU. GROW this home
// additively when that lands; do NOT fork it.
//
// X360 layout note (from InvalidateFrunksAhead asm): mpStreamHeader is read at
// *(this + 0x1C). The leading mHeaderMalloc (a LinearMalloc allocator-state blob, no
// recovered layout) therefore occupies the first 0x1C bytes. It is modelled as an
// opaque sized placeholder so mpStreamHeader lands at its X360 offset; the bodied
// function reaches mpStreamHeader BY NAME, so the placeholder's internals are
// immaterial (no offset is asserted across it).

#include "types.hpp"
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc mHeaderMalloc

namespace BrnReplays
{
    // The record-side disk sink. Reached BY POINTER only (mpStream is neither read nor
    // reset by the bodied functions in this slice), so an incomplete type suffices; the
    // full WriteStream TU owns its concrete home.
    class DiskWriteStream;

    class WriteStream
    {
    public:
        // ResetStream @ 0x8264D100. Free every header allocation, carve a fresh
        // StreamHeader, stamp the "REPLAY " magic, and clear the per-recording
        // bookkeeping (the DiskWriteStream link mpStream is left untouched --
        // StartNewStream owns it). Returns the freshly allocated StreamHeader.
        StreamHeader* ResetStream();

        // InvalidateFrunksAhead @ 0x8264D190. After a new frunk is added at liStartFrunk,
        // mark every later frunk whose start frame falls within the new frunk's covered
        // frame range as VOID (KU_FLAG_VOID), then advance miFirstFrunk past any leading
        // run of voided non-keyframe frunks (trimming them from the live ring).
        void InvalidateFrunksAhead(s32 liStartFrunk);

    protected:
        // Construction lives in the full WriteStream TU; until then, derived test
        // shims set the header pointer this slice's bodied function reads. Reaches
        // mpStreamHeader BY NAME (no offset cast).
        void SetStreamHeaderForTest(StreamHeader* lpHeader) { mpStreamHeader = lpHeader; }

    private:
        // Members recovered by ResetStream @0x8264D100 / InvalidateFrunksAhead
        // @0x8264D190; reached BY NAME (native layout, X360 byte offsets documentary).
        CgsMemory::LinearMalloc mHeaderMalloc;         // @0x00  header bump allocator (X360: 0x1C bytes)
        StreamHeader*           mpStreamHeader;        // @0x1C  the frunk index / stream header
        s32                     miCurrentWriteIndex;   // @0x40  write cursor
        s32                     miStallCount;          // @0x44
        bool                    mbEnded;               // @0x48
        bool                    mbPaused;              // @0x49
        s32                     miNumFrunksAllocated;  // @0x4C
        DiskWriteStream*        mpStream;              // @0x50  disk sink (NOT reset by ResetStream)
        s32                     miFilePosition;        // @0x54
        s32                     miBufferPosition;      // @0x58
        // ... remaining WriteStream tail (the 128 KiB intermediate buffer, ...) is NOT
        //     modelled in this slice; the full WriteStream TU grows it. No offset is
        //     asserted across these members.
    };
}
