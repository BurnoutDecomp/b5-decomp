#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                  // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropBecamePhysicalEvent.h" // PropBecamePhysicalEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent>::Append @ 0x823C4318
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already committed inline in CgsBaseEventQueue.h; this is the thin explicit instantiation that
// forces the per-element out-of-line emission. The X360 body matches the generic exactly:
//   - assert mpEvents != NULL                         (CgsBaseEventQueue.h:413)
//   - assert no overflow ("Base event queue overflow", CgsBaseEventQueue.h:414)
//   - read the source via GetQueueStartPointer (assert mpEvents != NULL, :486)
//   - XMemCpy(mpEvents + miLength, src, sizeof(T) * srcLen) with stride 16 (`slwi r,n,4`)
//   - miLength += source.miLength; return true
// Element stride 16 == aligned sizeof(PropBecamePhysicalEvent) (a single Vector3 mPosition).
// The Hex-Rays `int Append(_DWORD*, _DWORD*)` prototype is the ABI shape of
// `bool Append(this, const BaseEventQueue<T>&)`; the asm uses only r3 (this) / r4 (source).
template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent>&);
