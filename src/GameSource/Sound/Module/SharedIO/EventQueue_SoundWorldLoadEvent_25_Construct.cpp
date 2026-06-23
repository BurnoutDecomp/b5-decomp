#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h"   // BrnSound::Module::Io::SoundWorldLoadEvent (8-byte element)

// =============================================================================
// CgsModule::EventQueue<BrnSound::Module::Io::SoundWorldLoadEvent, 25>::Construct
//   @ 0x822E50D0   (ledger id: class:BrnSound::Module::Io::SoundWorldLoadEvent,25>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This is the fixed-capacity (N = 25) SoundWorldLoadEvent event queue: the 25
// SoundWorldLoadEvent records live inline in the derived EventQueue's maEvents[25]
// buffer (right after the 12-byte BaseEventQueue header), and Construct() points
// the base queue at that inline storage, sets the capacity and clears the live
// count. The generic EventQueue<T, N>::Construct body is already inline in
// CgsEventQueue.h; this TU is the thin explicit instantiation the X360 emitted
// out-of-line for the SoundWorldLoadEvent/25 specialisation.
//
// X360 store-for-store (asm at 0x822E50D0, offsets are the BaseEventQueue header):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents
//    is never null; the Hex-Rays `result == -12` is a misread of this addi+cmplwi)
//   stw   r30,  0(this)         ; mpEvents     = &maEvents
//   li    r11, 0x19 ; stw r11, 4(this)  ; miMaxLength = 25
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 25, which
// stores mpEvents = maEvents (= this + 12, the inline buffer offset), miMaxLength
// = 25, miLength = 0 -- matching the three header stores exactly. The mpEvents
// store landing at this+0xC confirms the 8-byte SoundWorldLoadEvent element sits
// inline at offset 12, immediately past the {T* @0, s32 @4, s32 @8} header.
//
// Callers (X360): the SoundWorldLoadEvent queue is the embedded
// SoundWorldLoadEvent buffer constructed by BrnWorld::WorldEntityIO::
// OutputBuffer_PreScene::Construct, BrnSound::Module::Io::RootInputBuffer::
// Construct, and BrnWorldIO::UpdateOutputBuffer::Construct.
// =============================================================================
template void
CgsModule::EventQueue<BrnSound::Module::Io::SoundWorldLoadEvent, 25>::Construct();
