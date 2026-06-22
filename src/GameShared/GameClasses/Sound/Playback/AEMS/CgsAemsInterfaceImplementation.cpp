// ============================================================================
// CgsAemsInterfaceImplementation.cpp -- CgsSound::Playback::AemsRWSamplePlayer dtor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826A2E80
//   (CgsSound::Playback::AemsRWSamplePlayer::`scalar deleting destructor')
//
//   *this = vtable;                  // off_820AB14C
//   if (a2 & 1) operator delete(this);
//
// The destructor reads NO data members -- every member of AemsRWSamplePlayer is a
// trivially-destructible pointer / float / double / enum / byte, and the base
// Snd9::IAemsSamplePlayer destructor is empty -- so the only work is the vtable store
// (which the compiler emits as part of running the destructor) plus the conditional
// operator delete (the scalar-deleting thunk). Defining the class destructor
// out-of-line emits exactly that sequence; the body is empty.
//
// NOTE: this TU owns the destructor only. The ctor and the Release/Pause/Unpause/
// SetInput/SetAzimuth/GetOutputs overrides are their own (not-yet-done) TUs and
// resolve at consolidation against this shared home.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"

namespace CgsSound
{
namespace Playback
{
    AemsRWSamplePlayer::~AemsRWSamplePlayer()
    {
        // All members are trivially destructible and the base dtor is empty: the only
        // emitted work is the vtable store + the scalar-deleting tail.
    }
}
}
