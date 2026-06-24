#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // BrnWorld::CrashIO::RaceCarCrashCompleteEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>::AddEvent @ 0x827C2AC8
// CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>::Append  @ 0x827A7D70
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent/Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
//
// AddEvent (0x827C2AC8) asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and
// miLength < miMaxLength (CgsBaseEventQueue.h:313, the StrStream "Reached Max length" tripwire
// that collapses to the house CGS_ASSERT) -- both non-gating -- then appends the element
// unconditionally and bumps miLength. Element stride is 16 bytes: the X360 body does
// `slwi r11,miLength,4; add mpEvents` then two 8-byte stores (ld/std @0 + ld/std @8), i.e. the
// whole 16-byte element copy (matching the generic `mpEvents[miLength] = lEvent`).
//
// Append (0x827A7D70) asserts mpEvents != NULL (CgsBaseEventQueue.h:413), no overflow
// (:414, "Base event queue overflow"), and the source owns a buffer (:486 via
// GetQueueStartPointer) -- all non-gating -- then block-copies the source's live events
// (XMemCpy with `slwi count,miLength,4` == 16*count bytes, dest `slwi,4` == mpEvents + 16*miLength)
// and advances miLength by source's miLength. Returns 1.
//
// sizeof(RaceCarCrashCompleteEvent) == 16 (VolumeInstanceId(u64, 8-byte aligned)(8) + bool flag(1),
// padded to 16), matching the 16-byte stride in both bodies. AddEvent is called by
// BrnWorld::CrashModule::ResetRaceCarFromCrashIndex; Append by
// WorldModule::BridgeCrashModuleToPropModule_PostScene and
// BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene::SetCrashInterface.
template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>::AddEvent(
    const BrnWorld::CrashIO::RaceCarCrashCompleteEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent>&);
