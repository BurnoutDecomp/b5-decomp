#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"  // BaseEventQueue<T>::AddEventSafe / Append (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"    // BrnAI::RouteMapModuleIO::RaceRouteRequest (128-byte element)

// CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RaceRouteRequest>::{AddEventSafe,Append}
//   AddEventSafe @ X360 0x8277AF20, Append @ X360 0x8277B588
//   (dossier id "class:BrnAI::RouteMapModuleIO::RaceRouteRequest>", funcs: 2).
//
// The generic AddEventSafe / Append bodies are already inline in CgsBaseEventQueue.h; these are
// the thin explicit instantiations (the X360 emits one out-of-line copy per using-TU). Both
// X360 bodies match the generic store-for-store. Layout a1[0]=mpEvents, a1[1]=miMaxLength,
// a1[2]=miLength.
//
// AddEventSafe (@0x8277AF20) -- bounds-gated append:
//   * mpEvents != NULL (CgsBaseEventQueue.h:331, `lwz r11,0(r31)`; bne skips the assert);
//   * if miLength >= miMaxLength return false WITHOUT appending (`lwz r11,8`; `lwz r10,4`;
//     bge -> `li r3,0`);
//   * otherwise copy lEvent into &mpEvents[miLength] as 16 qwords at a 128-byte stride
//     (`slwi r10,miLength,7` == *128, `add` mpEvents; 16x `ld`/`std` 8-byte copy loop,
//     ctr=0x10); bump miLength (`stw r11,8(r31)`); return 1.
//
// Append (@0x8277B588) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", lSource.miLength +
//     this->miLength vs this->miMaxLength, ble skips);
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + miLength, lSource.mpEvents, sizeof(T)*lSource.miLength) at the 128-byte
//     stride (`slwi r5,r29,7` count*128, `slwi r11,r11,7` dest offset); bumps miLength; returns 1.
//
// The 128-byte stride matches sizeof(RaceRouteRequest) == 128 (X360-attested, see
// BrnRouteMapModuleIO.h). AddEventSafe is called from RouteRequestManager::Generate*RouteRequest
// and AIModule::HandleGameActions; Append from AIModule::{Update,PausedUpdate} and
// AIModuleIO::InputBuffer::AppendRaceRouteRequestQueue.
template bool
CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RaceRouteRequest>::AddEventSafe(
    const BrnAI::RouteMapModuleIO::RaceRouteRequest&);

template bool
CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RaceRouteRequest>::Append(
    const CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RaceRouteRequest>&);
