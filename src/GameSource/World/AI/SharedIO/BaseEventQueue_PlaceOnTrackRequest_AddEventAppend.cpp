#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"        // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h" // BrnAI::AIModuleIO::PlaceOnTrackRequest (48-byte element)

// CgsModule::BaseEventQueue<BrnAI::AIModuleIO::PlaceOnTrackRequest>::AddEvent @ 0x8277B330
// CgsModule::BaseEventQueue<BrnAI::AIModuleIO::PlaceOnTrackRequest>::Append  @ 0x827A72A8
//   (ledger id: class:BrnAI::AIModuleIO::PlaceOnTrackRequest>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The generic
// BaseEventQueue<T>::AddEvent / Append bodies are already committed inline in CgsBaseEventQueue.h;
// this TU is the thin explicit instantiation that forces the per-element out-of-line emission the
// X360 build emitted. The companion EventQueue<PlaceOnTrackRequest,128>::Construct @0x822E2950 is
// already instantiated by EventQueue_PlaceOnTrackRequest_128.cpp.
//
// X360 AddEvent body (asm @0x8277B330) matches the generic exactly -- an UNCONDITIONAL append
// guarded by non-gating assert tripwires:
//   - assert mpEvents != NULL                         (CgsBaseEventQueue.h:312, `lwz r11,0(this)`)
//   - assert miLength < miMaxLength ("...Reached Max length", :313 -- the bge-to-assert path
//     falls through and still appends; not a gate)
//   - copy the event: `v12 = (48*miLength + mpEvents); 6 x { *v12++ = *src++ }` (six 8-byte std
//     copies = 48 bytes; `slwi r7,len,1; add len; slwi 4` = len*48), confirming
//     sizeof(PlaceOnTrackRequest) == 48 (2x Vector3 SIMD + EGlobalRaceCarIndex + f32, align 16).
//   - `++miLength` (`lwz r11,8(this); addi r11,r11,1; stw r11,8(this)`); returns 1 (true).
//
// X360 Append body (asm @0x827A72A8) matches the generic exactly:
//   - assert mpEvents != NULL                              (CgsBaseEventQueue.h:413)
//   - assert source.miLength + miLength <= miMaxLength ("Base event queue overflow", :414)
//   - assert source.mpEvents != NULL (GetQueueStartPointer, :486)
//   - `XMemCpy(48*miLength + mpEvents, source.mpEvents, 48*source.miLength)` -- block copy at
//     stride 48 (modelled as std::memcpy in the generic), then `miLength += source.miLength`.
//
// The Hex-Rays `int AddEvent(_DWORD*, _QWORD*)` / `int Append(_DWORD*, _DWORD*)` prototypes are
// the ABI shapes of `bool AddEvent(this, const PlaceOnTrackRequest&)` /
// `bool Append(this, const BaseEventQueue<PlaceOnTrackRequest>&)`.
//   Callers (X360): AddEvent <- BrnAI::AIModule::UpdateDrivers;
//                   Append   <- BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::SetAIModuleResultInterface.
template bool
CgsModule::BaseEventQueue<BrnAI::AIModuleIO::PlaceOnTrackRequest>::AddEvent(
    const BrnAI::AIModuleIO::PlaceOnTrackRequest&);
template bool
CgsModule::BaseEventQueue<BrnAI::AIModuleIO::PlaceOnTrackRequest>::Append(
    const CgsModule::BaseEventQueue<BrnAI::AIModuleIO::PlaceOnTrackRequest>&);
