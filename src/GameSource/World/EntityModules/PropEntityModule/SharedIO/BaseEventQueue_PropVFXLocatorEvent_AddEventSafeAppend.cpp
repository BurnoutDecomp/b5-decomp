#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::AddEventSafe/Append (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"   // BrnWorld::PropEntityIO::PropVFXLocatorEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>::AddEventSafe @ 0x822C9970
// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>::Append       @ 0x823C4058
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 80 bytes (sizeof(PropVFXLocatorEvent):
// Matrix44Affine(64) + muTypeId(4) + meEventType(4), 16-aligned) -- matched exactly:
//   AddEventSafe (@0x822C9970): asserts mpEvents!=NULL (CgsBaseEventQueue.h:331), returns false
//     WITHOUT appending when miLength >= miMaxLength (`bge cr6 -> li r3,0`), else writes the new
//     event at `*mpEvents + 80*miLength` -- four 16-byte vector stores (the Matrix44Affine, asm
//     lvx128/stvx128 at +0/+16/+32/+48) plus two trailing dwords (muTypeId @+64, meEventType @+68,
//     asm `*(r11+0x40)=*(r31+0x40); *(r11+0x44)=*(r31+0x44)`) -- bumps miLength, returns true.
//   Append (@0x823C4058): asserts mpEvents!=NULL (:413), no-overflow (:414) and source owns a
//     buffer (:486), then XMemCpy's `80 * srcLen` bytes onto the tail (asm derives the 80-byte
//     stride as `(v + 4*v) << 4` == 80*v for both dst offset and byte count) and advances miLength.
// The 80-byte stride is the same one the EventQueue<PropVFXLocatorEvent,10>::Construct TU asserts.
template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>::AddEventSafe(
    const BrnWorld::PropEntityIO::PropVFXLocatorEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>&);

// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>::GetEvent(s32) const
//   @ 0x8227BDA8
// Checked indexed accessor, const overload: asserts mpEvents!=NULL (CgsBaseEventQueue.h:272),
// liIndex < GetLength() (:274) and liIndex >= 0 (:275), then returns mpEvents[liIndex]
// (X360 `slwi r11,liIndex,2; add r11,liIndex,r11; slwi r11,r11,4; add r3,r11,mpEvents`
// == mpEvents + liIndex*80, stride 80 == sizeof(PropVFXLocatorEvent)). The generic const
// GetEvent(s32) inline body is already committed in CgsBaseEventQueue.h; this is the thin
// explicit instantiation. Called by BrnEffects::PropCollisions::UpdateLocatorVfx.
template const BrnWorld::PropEntityIO::PropVFXLocatorEvent&
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent>::GetEvent(s32) const;
