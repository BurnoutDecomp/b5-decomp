#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DAA50
//   CgsSound::Playback::AemsPlayerVoice::`scalar deleting destructor'
//   (compiler-synthesized; DWARF virtual ~AemsPlayerVoice(), CgsAemsPlayerVoice.h:96).
//
// X360 sequence:
//   *this = vtable(AemsPlayerVoice);                 // off_820B561C
//   *(this+0x8C) = 0;                                // clear mbRemoving
//   GenericRwacVoice::~GenericRwacVoice(this+0x2C);  // base @ +0x2C
//   *this = vtable(Voice);                            // off_820B3608
//   Voice::~Voice(this);                              // primary base @ +0
//   if (a2 & 1) operator delete(this);                // scalar-deleting tail
//
// Every AemsPlayerVoice member is trivially destructible; the only work is the flag
// clear + the two base dtors. Defining the class destructor out-of-line emits exactly
// that sequence (the base dtors run implicitly, host delete stands in for the
// custom-allocator tail). Mirrors the committed CgsAemsContent scalar/vector dtor.

namespace CgsSound
{
namespace Playback
{
    AemsPlayerVoice::~AemsPlayerVoice()
    {
        // GenericRwacVoice and Voice base destructors run implicitly; the vtable
        // stores, the mbRemoving clear, and the operator-delete tail are the
        // compiler's scalar-deleting-destructor synthesis.
    }
}
}
