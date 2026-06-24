#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // BrnWorld::CrashIO::CleanupTrafficEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::CleanupTrafficEvent>::AddEvent @ 0x827C2C10
// CgsModule::BaseEventQueue<BrnWorld::CrashIO::CleanupTrafficEvent>::Append  @ 0x827A8490
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent/Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
//
// AddEvent (0x827C2C10) asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and
// miLength < miMaxLength (CgsBaseEventQueue.h:313) -- both non-gating tripwires whose StrStream
// "Reached Max length" formatting collapses to the house CGS_ASSERT -- then appends the element
// unconditionally and bumps miLength. Element stride is 8 bytes: the X360 body does
// `slwi r11,miLength,3; ld r10,0(src); stdx r10,r11,mpEvents` -- a single 8-byte store at an
// 8-byte stride (no two-half split), i.e. the whole 8-byte element copy.
//
// Append (0x827A8490) asserts mpEvents != NULL (CgsBaseEventQueue.h:413), the combined length
// will not overflow (CgsBaseEventQueue.h:414, "Base event queue overflow"), and the source owns
// a buffer (CgsBaseEventQueue.h:486 via GetQueueStartPointer) -- all non-gating tripwires -- then
// block-copies the source's live events (XMemCpy with `slwi count,miLength,3` == 8*count bytes,
// dest `slwi,3` == mpEvents + 8*miLength) and advances miLength by source's miLength. Returns 1.
//
// sizeof(CleanupTrafficEvent) == sizeof(CgsSceneManager::VolumeInstanceId) == 8, matching the
// 8-byte stride in both bodies. AddEvent is called by CrashModule::ForceClearupAllCrashes /
// ClearupCrashes; Append by BrnTraffic::BrnTrafficIO::InputBuffer_PostScene::SetCrashTrafficOutputInterface.
template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::CleanupTrafficEvent>::AddEvent(
    const BrnWorld::CrashIO::CleanupTrafficEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::CleanupTrafficEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::CrashIO::CleanupTrafficEvent>&);
