// Embed-check for CgsAemsInterfaceImplementation.h + .cpp:
//   CgsSound::Playback::AemsRWSamplePlayer::`scalar deleting destructor' @ 0x826A2E80
//
// Compile-only (matches the gate): forces emission of the deleting destructor via a
// delete-through-pointer and pins down the polymorphic-base relationship the dtor's
// vtable store depends on. No object is constructed (the ctor + the six control
// overrides are cross-TU and resolve at consolidation).

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"

using namespace CgsSound::Playback;

// AemsRWSamplePlayer must derive from the Snd9 sample-player interface (the vtable the
// destructor stores is this hierarchy's). ABI-stable structural invariant only.
static_assert(static_cast<Snd9::IAemsSamplePlayer*>(static_cast<AemsRWSamplePlayer*>(nullptr)) == nullptr,
              "AemsRWSamplePlayer must derive from Snd9::IAemsSamplePlayer");
static_assert(sizeof(AemsRWSamplePlayer) > sizeof(void*),
              "AemsRWSamplePlayer carries its voice/plug-in chain past the vptr");

// delete-through-pointer forces the compiler to emit the (scalar-)deleting
// destructor: vtable store + base dtor + operator delete.
void embed_delete_aems_rw_sample_player(AemsRWSamplePlayer* lpPlayer)
{
    delete lpPlayer;
}
