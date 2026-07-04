#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnDetachedPartRenderEvent.h" // BrnWorld::DetachedPartRenderEvent (80-byte stride element)

// CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>::GetEvent(s32) const  @ 0x822AD0F0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnWorld::RaceCarEntityModule::RenderRaceCar to read the per-frame detached-part transforms an
// ActiveRaceCar queued (EventQueue<DetachedPartRenderEvent, 20>, inline at ActiveRaceCar this+5520).
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the CONST GetEvent(int) overload (:270) -- then returns
// &mpEvents[liIndex] (`result = 80*a2 + mpEvents`, computed as liIndex*5<<4 via
// `slwi r11,liIndex,2; add r11,liIndex,r11; slwi r11,r11,4` == liIndex*0x50). The 80-byte (0x50)
// stride is sizeof(DetachedPartRenderEvent): the alignas(16) 72-byte payload (Matrix44Affine 64 +
// EntityId + u8 part index) padded to a 16-byte multiple, matching the stride of the sibling
// AddEventSafe (@0x822C88B0). The Hex-Rays `int` return is the ABI-returned T& pointer; the DWARF
// gives the real `const DetachedPartRenderEvent&`.
template const BrnWorld::DetachedPartRenderEvent&
CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>::GetEvent(s32) const;
