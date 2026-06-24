#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"

// Out-of-line per-instantiation bodies of CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest>.
// The X360 build emits each BaseEventQueue<T> member used by an instantiation out-of-line; the shared
// generic bodies live in CgsBaseEventQueue.h. The reset-on-track request queue (element stride 16)
// uses:
//   - AddEvent @0x822C75B0 (BrnWorld::RaceCarEntityModule::SendResetOnTrackRequests):
//       asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and miLength < miMaxLength (:313, the
//       streamed "Reached Max length" tripwire), then UNCONDITIONALLY copies the four 32-bit event
//       words to mpEvents[miLength] (`slwi r11,r11,4` -> 16-byte stride) and bumps miLength; returns 1.
//   - Append @0x827A70D8 (BrnAI::AIModuleIO::InputBuffer::AppendAIModuleRequestInterface):
//       asserts mpEvents != NULL (:413), miLength + source.miLength <= miMaxLength ("Base event queue
//       overflow", :414) and source.mpEvents != NULL (:486), then XMemCpy's source.miLength elements
//       (16-byte stride) onto the tail and advances miLength; returns 1.
// Both match the generic CgsBaseEventQueue.h AddEvent/Append bodies for a 16-byte element; this TU
// only forces their emission for the ResetOnTrackRequest instantiation.

template bool CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest>::AddEvent(
    const BrnAI::AIModuleIO::ResetOnTrackRequest&);
template bool CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest>::Append(
    const CgsModule::BaseEventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest>&);
