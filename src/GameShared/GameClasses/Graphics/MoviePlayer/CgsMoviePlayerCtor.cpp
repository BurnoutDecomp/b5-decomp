// CgsGraphics::MoviePlayer::MoviePlayer -- the player constructor.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x827DD390, semantic-parity (not byte-matching).
//
// Bodied here (1 ledger function):
//   CgsGraphics::MoviePlayer::MoviePlayer   @0x827DD390
//
// The X360 ctor does three things in order:
//   1. RtlInitializeCriticalSection(this + 0x1E0)  -- initialize the per-player lock.
//   2. store the vtable pointer (off_820CE054) at this + 0x86C.
//   3. zero two runs of words at this + 0x884..0x894 and this + 0x89C..0x8AC -- the
//      streaming / decode-job book-keeping that on console tracked the EA-chunk read
//      cursor and the On2 VP6 decode handles.
//
// On PC the lock is the committed EA::Thread::Futex member (its ctor performs the
// CRITICAL_SECTION init, matching step 1), the vtable is implicit, and the zeroed
// book-keeping is the playback + decode-stream state that Construct() clears -- so the
// constructor delegates the field-zeroing to Construct() rather than restate it.
//
// This file is kept apart from CgsMoviePlayer.cpp so the constructor's compile depends
// only on the header (no FFmpeg backend headers), matching the per-TU gate.

#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"

namespace CgsGraphics
{
    // @0x827DD390. mLock's Futex ctor runs first (the CRITICAL_SECTION init); Construct()
    // then clears every playback + decode-stream field to its empty-stopped state.
    MoviePlayer::MoviePlayer()
    {
        Construct();
    }
}
