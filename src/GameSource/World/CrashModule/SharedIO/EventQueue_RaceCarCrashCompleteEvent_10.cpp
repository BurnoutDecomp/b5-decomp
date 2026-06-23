#include "GameShared/GameClasses/Module/CgsEventQueue.h"                           // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // BrnWorld::CrashIO::RaceCarCrashCompleteEvent

// CgsModule::EventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent, 10>::Construct @ 0x822E3600
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the inline generic
// Construct: points the base queue at its inline maEvents buffer (this + 0x10 -- the base
// subobject is 3 ints == 12 bytes, padded to 16 by the 8-byte-aligned 16-byte element), sets
// miMaxLength = 10 (0xA), clears miLength. The asm stores mpEvents @0, 0xA @4, 0 @8. The
// lpEventBuffer != NULL assert (CgsBaseEventQueue.h:160) is a non-gating tripwire.
template void CgsModule::EventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent, 10>::Construct();
