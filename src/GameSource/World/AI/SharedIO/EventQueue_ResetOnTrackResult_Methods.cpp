#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"

// Out-of-line per-instantiation bodies of CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackResult>.
// The X360 build emits each BaseEventQueue<T> member used by an instantiation out-of-line; the shared
// generic bodies live in CgsBaseEventQueue.h. ResetOnTrackResult is 48 bytes (two Vector3s + state +
// index + speed), so the per-element copy is a 6-qword stride. The result queue uses:
//   - AddEvent @0x8277B1D0 (BrnAI::ResetOnTrackManager::ProcessResetOnTrackRequest):
//       asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and miLength < miMaxLength (:313, the
//       streamed "Reached Max length" tripwire), then UNCONDITIONALLY copies the event to
//       mpEvents[miLength] via a 6-iteration `ld/std` loop (`miLength*3<<4` -> 48-byte stride) and
//       bumps miLength; returns 1.
//   - Append @0x827A71B8 (BrnWorld::PropEntityIO::InputBuffer_PrePhysics::AppendResetOnTrackResultQueue,
//       BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::SetAIModuleResultInterface):
//       asserts mpEvents != NULL (:413), miLength + source.miLength <= miMaxLength ("Base event queue
//       overflow", :414) and source.mpEvents != NULL (:486), then XMemCpy's source.miLength elements
//       (48-byte stride) onto the tail and advances miLength; returns 1.
// Both match the generic CgsBaseEventQueue.h AddEvent/Append bodies for a 48-byte element; this TU
// only forces their emission for the ResetOnTrackResult instantiation.

template bool CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackResult>::AddEvent(
    const BrnAI::AIModuleIO::ResetOnTrackResult&);
template bool CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackResult>::Append(
    const CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackResult>&);

// GetEvent(s32) const @0x822AC500 (callers RaceCarEntityModule::ProcessResetOnTrackResultQueue,
// PropEntityModule::PrePhysicsUpdate, AIModule::UpdateResetOnTrackManager): the checked const
// element accessor. Generic body inline in CgsBaseEventQueue.h; asserts mpEvents != NULL,
// liIndex < GetLength() and liIndex >= 0, then returns &mpEvents[liIndex] as mpEvents + liIndex*48
// (stride 48 == sizeof(ResetOnTrackResult)). This TU only forces the out-of-line emission.
template const BrnAI::AIModuleIO::ResetOnTrackResult&
CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackResult>::GetEvent(s32) const;
