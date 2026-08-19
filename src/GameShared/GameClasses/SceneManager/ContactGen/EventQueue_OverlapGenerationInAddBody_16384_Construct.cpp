#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                  // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"    // OverlapGenerationIO::InAddBody (64-byte element)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InAddBody, 16384>::Construct
//   @ 0x828C4B80   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InAddBody,16384>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (16384) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length (0x4000 == 16384), and clears the live count. The generic Construct body is
// already inline in CgsEventQueue.h / CgsBaseEventQueue.h (the `lpEventBuffer != NULL`
// assert is that generic body; Hex-Rays `result == -16` is a misread of the addi
// r30,r31,0x10 + cmplwi that computes &maEvents). maEvents lands at this+0x10 (12-byte
// {T*,s32,s32} header + 4 tail pad for the alignas(16) InAddBody[]). The element
// stride is 64 bytes (sibling AddEvent's slwi r10,r10,6 @0x828B85D8), so
// sizeof(InAddBody)==0x40. This is the InputBuffer's add-body queue member type;
// its Construct is called from CgsSceneManager::OverlapGenerationIO::InputBuffer::Construct.
// Direct sibling of EventQueue_OverlapGenerationInUpdateBody_16384_Construct.cpp.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InAddBody, 16384>::Construct();
