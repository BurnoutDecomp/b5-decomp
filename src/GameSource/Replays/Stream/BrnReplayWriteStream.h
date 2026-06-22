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

namespace BrnReplays
{
    class WriteStream
    {
    public:
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
        // @0x00 LinearMalloc mHeaderMalloc -- header allocator state (opaque; 0x1C bytes
        // on X360 so mpStreamHeader lands at +0x1C). Not reached by name by this TU.
        u8            maHeaderMalloc[0x1C]; // @0x00 (LinearMalloc, layout unrecovered)
        StreamHeader* mpStreamHeader;       // @0x1C the frunk index / stream header
        // ... remaining WriteStream members (mCurrentFrunk, write/stall counters,
        //     mpStream, the 128 KiB intermediate buffer, ...) are NOT modelled in this
        //     minimal slice; the full WriteStream TU grows them.
    };
}
