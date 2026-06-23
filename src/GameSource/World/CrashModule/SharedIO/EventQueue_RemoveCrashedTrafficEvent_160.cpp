#include "GameShared/GameClasses/Module/CgsEventQueue.h"                           // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // BrnWorld::CrashIO::RemoveCrashedTrafficEvent

// CgsModule::EventQueue<BrnWorld::CrashIO::RemoveCrashedTrafficEvent, 160>::Construct @ 0x827608A0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the inline generic
// Construct: points the base queue at its inline maEvents buffer (this + 0xC -- the base subobject
// is 3 ints == 12 bytes, no padding because the u16 element is 4-byte aligned), sets
// miMaxLength = 160 (0xA0), clears miLength. The asm stores mpEvents @0, 0xA0 @4, 0 @8. The
// lpEventBuffer != NULL assert (CgsBaseEventQueue.h:160) is a non-gating tripwire.
template void CgsModule::EventQueue<BrnWorld::CrashIO::RemoveCrashedTrafficEvent, 160>::Construct();
