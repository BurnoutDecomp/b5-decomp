#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                  // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"    // OverlapGenerationIO::InRemoveBody (16-byte element)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InRemoveBody, 16384>::Construct
//   @ 0x828C4C60   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InRemoveBody,16384>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (16384) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length (0x4000 == 16384), and clears the live count. The generic Construct body is
// already inline in CgsEventQueue.h / CgsBaseEventQueue.h (the `lpEventBuffer != NULL`
// assert is that generic body; Hex-Rays `result == -16` is a misread of the addi
// r30,r31,0x10 + cmplwi that computes &maEvents). maEvents lands at this+0x10 (12-byte
// {T*,s32,s32} header + 4 pad; the pad comes from InRemoveBody's u64 8-byte
// alignment -- this element is NOT alignas(16)). The element stride is 16 bytes
// (sibling RemoveBody AddEvent @0x828B8888 slwi r11,r11,4), so
// sizeof(InRemoveBody)==0x10. This is the InputBuffer's mRemoveBodyQueue member
// type (the derived EventQueue); the exact analog of the InUpdateBody Construct
// @ 0x828C4BF0. Called by CgsSceneManager::OverlapGenerationIO::InputBuffer::Construct.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InRemoveBody, 16384>::Construct();
