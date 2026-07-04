#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"  // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"    // BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest (64-byte element)

// CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest>::AddEventSafe
//   @ X360 0x8277AFD8   (dossier id "class:BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest>", funcs: 1).
//
// The generic AddEventSafe body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation the X360 emitted out-of-line for the ExtrapolatedRouteRequest specialisation. The
// X360 body matches the generic store-for-store. Layout a1[0]=mpEvents, a1[1]=miMaxLength,
// a1[2]=miLength.
//
// AddEventSafe (@0x8277AFD8) -- bounds-gated append:
//   * mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire, `lwz r11,0(r31)`;
//     bne skips the assert);
//   * if miLength >= miMaxLength return false WITHOUT appending (`lwz r11,8(r31)` miLength vs
//     `lwz r10,4(r31)` miMaxLength; `bge` -> `li r3,0`);
//   * otherwise copy lEvent into &mpEvents[miLength] as 8 qwords at a 64-byte stride
//     (`slwi r10,miLength,6` == *64, `add` mpEvents; 8x `ld`/`std` 8-byte copy loop, ctr=8);
//     bump miLength (`stw r11,8(r31)`); return 1.
//
// The 64-byte stride matches sizeof(ExtrapolatedRouteRequest) == 64 (X360-attested, see
// BrnRouteMapModuleIO.h). Called from BrnAI::RouteRequestManager::GenerateExtrapolatedRouteRequest
// and BrnAI::RouteRequestManager::GenerateRouteFleeingRouteRequest.
template bool
CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest>::AddEventSafe(
    const BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest&);
