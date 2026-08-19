#include "GameShared/GameClasses/Module/CgsEventQueue.h"           // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // SceneManagerIO::OutOverlapPair element (24B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>::Construct
//   @ 0x825A7C08   (ledger id: class:CgsSceneManager::SceneManagerIO::OutOverlapPair,128>)
//
// ⚠️ FILENAME IS A LEGACY MISNOMER, AND THE ELEMENT TYPE WAS WRONG UNTIL WAVE Q5
// (2026-08-18). This TU used to instantiate EventQueue<CgsSceneManager::OverlappingPair,128>
// -- the 16-byte broad-phase pair -- while naming 0x825A7C08, which the identity map calls
// SceneManagerIO::OutOverlapPair,128>::Construct. The two are DIFFERENT types (the header
// note in CgsSceneManagerIO_EventOutOverlapPair.h already said so) and the console settles
// it by size: inside SceneManagerIO::OutputBuffer this queue runs from +196656 to the next
// member at +199744, i.e. 3088 bytes == 16 (header + pad) + 128 * 24, and 24 is
// sizeof(OutOverlapPair) -- 128 * 16 would be 2064. The consumers agree: BrnPhysicsModuleIO.h
// and BrnVehicleManager.h both already type this queue EventQueue<OutOverlapPair,128>.
// There is NO EventQueue<OverlappingPair,128> anywhere in the console; the broad-phase
// OverlappingPair queue is the 16384-deep one (@0x828C4D40, instantiated in
// ContactGen/CgsOverlapCullingModuleIO.cpp). The file keeps its name only so the pending
// build_game_exe.bat mount handshake does not move under the conductor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 128) OutOverlapPair output queue: the 128 records live inline in
// the derived EventQueue's maEvents[128] buffer, and Construct() points the base queue at
// that inline storage, sets the capacity and clears the live count. The generic
// EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is the thin
// explicit instantiation the X360 emitted out-of-line.
//
// X360 store-for-store (asm at 0x825A7C08), offsets are the BaseEventQueue header
// (mpEvents @0, miMaxLength @4, miLength @8):
//   addi  r30, r31, 0x10        ; lpEventBuffer = &maEvents (this + 0x10)
//   cmplwi cr6, r30, 0; bne     ; assert lpEventBuffer != NULL (vacuous -- &maEvents
//                                 is never null; Hex-Rays `result == -16` is a misread
//                                 of this addi+cmplwi)
//   stw   r30, 0(r31)           ; mpEvents     = &maEvents
//   li    r11, 0x80 ; stw r11, 4(r31)  ; miMaxLength = 128
//   li    r10, 0    ; stw r10, 8(r31)  ; miLength    = 0
//   return this
// == BaseEventQueue<OutOverlapPair>::Construct(maEvents, KI_LENGTH==128). maEvents landing
// at this+0x10 is the 4 bytes of tail padding the compiler inserts after the 12-byte
// {T*,s32,s32} header so maEvents[] meets OutOverlapPair's 8-byte alignment (its leading
// two u64 VolumeInstanceId handles) -- it does NOT imply 16-byte element alignment, and it
// is exactly what distinguishes this queue from the 4-aligned OverlappingPair/ErrorEvent
// siblings whose buffers start at +0xC.
//
// Callers (X360): BrnPhysics::PhysicsModuleIO::InputBuffer::Construct @0x825ABA18,
// BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics::Construct @0x827615F8,
// CgsSceneManager::SceneManagerIO::OutputBuffer::Construct @0x828C7CA0.
// =============================================================================
static_assert(sizeof(CgsSceneManager::SceneManagerIO::OutOverlapPair) == 24,
              "OutOverlapPair stride 24 (X360 AddEvent @0x828AD390 / Append @0x827A6FE8)");

template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>::Construct();
