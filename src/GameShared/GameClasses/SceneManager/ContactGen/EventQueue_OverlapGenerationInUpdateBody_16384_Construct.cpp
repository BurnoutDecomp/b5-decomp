#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                  // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"    // OverlapGenerationIO::InUpdateBody (64-byte element)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InUpdateBody, 16384>::Construct
//   @ 0x828C4BF0   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InUpdateBody,16384>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (16384) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length (0x4000 == 16384), and clears the live count. The generic Construct body is
// already inline in CgsEventQueue.h / CgsBaseEventQueue.h (the `lpEventBuffer != NULL`
// assert is that generic body; Hex-Rays `result == -16` is a misread of the addi
// r30,r31,0x10 + cmplwi that computes &maEvents). maEvents lands at this+0x10 (12-byte
// {T*,s32,s32} header + 4 tail pad for the alignas(16) InUpdateBody[]). The element
// stride is 64 bytes (AddEvent's slwi r10,r10,6), so sizeof(InUpdateBody)==0x40.
// This is the InputBuffer's mUpdateBodyQueue member type (the derived EventQueue).
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InUpdateBody, 16384>::Construct();
