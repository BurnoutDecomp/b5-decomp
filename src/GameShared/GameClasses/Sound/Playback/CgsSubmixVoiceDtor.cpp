// ============================================================================
// CgsSubmixVoiceDtor.cpp -- CgsSound::Playback::SubmixVoice destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826C7EF8
//   (CgsSound::Playback::SubmixVoice::`vector deleting destructor')
//
// The X360 compiler-synthesised deleting destructor is:
//   *this = off_820B35E4;                 // install the SubmixVoice vtable
//   CgsSound::Playback::Voice::~Voice();  // run the Voice base destructor
//   if (a2 & 1) operator delete(this);    // (vector-)deleting tail
//
// mpSubmix is a trivially-destructible raw pointer -- no destructor asm. There is no
// array-of-subobjects member, so the *vector*-deleting shape collapses to the scalar
// (base dtor + deleting tail): an empty out-of-line class destructor emits exactly
// that sequence. Mirrors the committed CgsAemsContentDtor.cpp.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"

namespace CgsSound
{
namespace Playback
{
    SubmixVoice::~SubmixVoice()
    {
        // The Voice base destructor runs implicitly here (the X360 `bl ~Voice`).
        // mpSubmix is a trivially-destructible raw plug-in pointer -- no dtor asm.
    }
}
}
