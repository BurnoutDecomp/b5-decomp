#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // BrnWorld::CrashIO::RaceCarCrashCompleteEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>::GetEvent(s32) const  @ 0x822ACCE0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnWorld::PropEntityModule::PostSceneUpdate and
// BrnWorld::RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents to walk the
// EventQueue<RaceCarCrashCompleteEvent, 10>.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the CONST GetEvent(int) overload (:270) -- then returns
// &mpEvents[liIndex] (`result = 16*a2 + mpEvents`, `slwi r11, liIndex, 4`). The 16-byte stride is
// sizeof(RaceCarCrashCompleteEvent) == VolumeInstanceId(8) + bool(@+8), padded to 16 by the
// 8-byte element alignment. The Hex-Rays `int` return is the ABI-returned T& pointer; the DWARF
// gives the real `const RaceCarCrashCompleteEvent&`.
template const BrnWorld::CrashIO::RaceCarCrashCompleteEvent&
CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>::GetEvent(s32) const;
