#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                  // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"    // OverlapGenerationIO::InForceNoPadding (4-byte u32 element)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InForceNoPadding, 128>::Construct
//   @ 0x828C4CD0   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InForceNoPadding,128>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (128) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length (0x80 == 128), and clears the live count. The generic Construct body is
// already inline in CgsEventQueue.h / CgsBaseEventQueue.h (the `lpEventBuffer != NULL`
// assert -- CgsBaseEventQueue.h:20 -- is that generic body; Hex-Rays `result == -12`
// is a misread of the addi r30,r31,0xC + cmplwi that computes &maEvents). Because the
// element is 4-byte-aligned (InForceNoPadding is the empty event base plus one u32 --
// DWARF CgsOverlapGenerationModuleIO.h:111), the ON-CONSOLE {T*,s32,s32} header is a
// bare 12 bytes with NO tail pad, so maEvents lands at this+0xC (stw r30,0(r31) stores
// that pointer; contrast the alignas(16) InUpdateBody queue whose maEvents is at +0x10).
// ⚠️ 12 is an X360 value: on the x64 host the event POINTER widens and the same header
// is 16 bytes. Nothing depends on either -- Construct passes maEvents by address.
// The element stride is 4 bytes (sibling AddEvent @0x828B89D0: single slwi r11,r11,2 /
// 32-bit move), so sizeof(InForceNoPadding)==4. This is the InputBuffer's
// force-no-padding queue member type (the derived EventQueue); called from
// OverlapGenerationIO::InputBuffer::Construct.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlapGenerationIO::InForceNoPadding, 128>::Construct();
