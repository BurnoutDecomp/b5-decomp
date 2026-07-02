#include "GameShared/GameClasses/Sound/Playback/CgsMasterVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSound::Playback::MasterVoice -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameShared/GameClasses/Sound/Playback/CgsMasterVoice.cpp):
//   MasterVoice::~MasterVoice @0x826D7A28
//
// The X360 body is the usual dtor mechanics (the MasterVoice vtable store, then
// the SubmixVoice-folded Voice::~Voice chain -- implicit in C++) around the one
// source statement pair: assert the singleton slot is occupied, then clear it.

namespace CgsSound
{
namespace Playback
{

MasterVoice* MasterVoice::spMasterVoice;   // cpp:43 (X360 dword_82FFB9E0)

// @ 0x826D7A28
MasterVoice::~MasterVoice()
{
    // Non-gating tripwire (cpp:48).
    CGS_ASSERT(spMasterVoice != NULL, "spMasterVoice");
    spMasterVoice = NULL;
}

}
}
