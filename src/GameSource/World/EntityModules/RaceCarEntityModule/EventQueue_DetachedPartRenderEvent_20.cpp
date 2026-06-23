#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                   // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnDetachedPartRenderEvent.h"  // BrnWorld::DetachedPartRenderEvent

// CgsModule::EventQueue<BrnWorld::DetachedPartRenderEvent, 20>::Construct @ 0x822E3910
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the inline generic
// Construct: points the base queue at its inline maEvents buffer (this + 0x10 -- the base
// subobject is 3 ints == 12 bytes, padded to 16 by the 16-byte-aligned (alignas-16, Matrix44Affine)
// element), sets miMaxLength = 20 (0x14), clears miLength. The asm stores mpEvents @0, 0x14 @4,
// 0 @8. The lpEventBuffer != NULL assert (CgsBaseEventQueue.h:160) is a non-gating tripwire.
template void CgsModule::EventQueue<BrnWorld::DetachedPartRenderEvent, 20>::Construct();
