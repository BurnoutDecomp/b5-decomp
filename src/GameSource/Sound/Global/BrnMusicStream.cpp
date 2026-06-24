#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (queued-flag tripwire)

// =============================================================================
// BrnSound::Logic::MusicStream::Stream -- out-of-line bodies for the two ledger
// functions owned by this TU:
//   GetCre         @ 0x82687368
//   SetSongQueued  @ 0x82683208
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// MusicStream shares the BrnMusicEffect.h DWARF/assert home with EaTraxData; the
// layout map and the un-DWARF'd-member FLAG live in that header. All members
// reached BY NAME. This is a separate .cpp from BrnMusicEffect.cpp so each TU owns
// its own translation unit (no shared external-linkage symbol is redefined).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace MusicStream
{

// ---------------------------------------------------------------------------
// Stream::GetCre  @ 0x82687368
//
//   return &maCre[0]          ; addi r3, r3, 0xC ; blr
//
// Store-for-store with the X360: the whole body is `this + 0x0C` returned in r3 --
// the address of the sub-object at +0x0C. No assert, no load: a pure address-of
// accessor.
// ---------------------------------------------------------------------------
Cre* Stream::GetCre()
{
    return reinterpret_cast<Cre*>( &maCre[0] );
}

// ---------------------------------------------------------------------------
// Stream::SetSongQueued  @ 0x82683208
//
//   if ( lbSongQueued == mbSongQueued )
//       assert("lbSongQueued != mbSongQueued", BrnMusicEffect.h:1080)  ; tripwire
//   mbSongQueued = lbSongQueued                                        ; stb 0x5A
//
// Store-for-store with the X360: the current flag is read as a byte (lbz 0x5A),
// the incoming bool is zero-extended (clrlwi r10,r30,24) and compared (cmplw); on
// equality the non-gating assert fires; the new value is written as a byte
// (stb r30, 0x5A). The assert is "the caller should only flip the flag" -- it does
// not gate the store.
// ---------------------------------------------------------------------------
void Stream::SetSongQueued( bool lbSongQueued )
{
    CGS_ASSERT( lbSongQueued != (mbSongQueued != 0), "lbSongQueued != mbSongQueued" );

    mbSongQueued = lbSongQueued ? 1 : 0;
}

} // namespace MusicStream
} // namespace Logic
} // namespace BrnSound
