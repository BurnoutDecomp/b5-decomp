#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"  // BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"    // BrnAI::RouteMapModuleIO::RouteResponse (5136-byte element)

// CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RouteResponse>::{AddEvent,Append}
//   AddEvent @ X360 0x8277B090, Append @ X360 0x8277B668
//   (dossier id "class:BrnAI::RouteMapModuleIO::RouteResponse>", funcs: 2).
//
// The generic AddEvent / Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations (the X360 emits one out-of-line copy per using-TU). Both X360
// bodies match the generic store-for-store. Layout a1[0]=mpEvents, a1[1]=miMaxLength,
// a1[2]=miLength.
//
// AddEvent (@0x8277B090) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r29)` vs
//     `lwz r10,4(r29)`, blt skips) -- tripwire only; the copy below always runs;
//   * memcpy(mpEvents + miLength, &lEvent, 5136) at a 5136-byte stride (`li r5,0x1410` Size==5136,
//     `mulli r11,miLength,0x1410`, `add r3,r11,mpEvents`); post-increments miLength
//     (`stw r11,8(r29)`); returns 1.
//
// Append (@0x8277B668) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", lSource.miLength +
//     this->miLength vs this->miMaxLength, ble skips);
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + miLength, lSource.mpEvents, sizeof(T)*lSource.miLength) at the 5136-byte
//     stride (`mulli r5,r29,0x1410` count*5136, `mulli r11,r11,0x1410` dest offset); bumps
//     miLength; returns 1.
//
// The 5136-byte stride matches sizeof(RouteResponse) == 5136 (X360-attested, see
// BrnRouteMapModuleIO.h). AddEvent is called from RouteMapModule::Process{RaceRoute,
// ExtrapolatedRoute}; Append from AIModule::{Update,PausedUpdate} and
// BrnWorldIO::UpdateOutputBuffer::AppendRouteResponseQueue.
template bool
CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RouteResponse>::AddEvent(
    const BrnAI::RouteMapModuleIO::RouteResponse&);

template bool
CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RouteResponse>::Append(
    const CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RouteResponse>&);

// CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RouteResponse>::GetEvent(s32) const
//   X360 @0x823AC158 (callers BrnGameModule::BridgeWorldRouteInformationToGui /
//   BridgeWorldToGameState / AIModule::UpdateCarRoutes).
// The generic const GetEvent body is inline in CgsBaseEventQueue.h: asserts mpEvents != NULL,
// liIndex < GetLength(), liIndex >= 0, then returns mpEvents[liIndex]. The X360 body matches
// store-for-store; the return `mulli index,0x1410` == index*sizeof(T) with
// sizeof(RouteResponse)==5136. Thin out-of-line instantiation of the const GetEvent overload.
template const BrnAI::RouteMapModuleIO::RouteResponse&
CgsModule::BaseEventQueue<BrnAI::RouteMapModuleIO::RouteResponse>::GetEvent(s32) const;
