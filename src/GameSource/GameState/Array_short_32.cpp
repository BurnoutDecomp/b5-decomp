// b5-decomp/src/GameSource/GameState/Array_short_32.cpp
// (The four Array<u16,32> accessors form ONE coherent instantiation TU; the EventQueue
//  Construct at 0x822E32F0 is a SEPARATE class/file -- see EventQueue_unsigned_short_32_Construct.cpp.)

#include "GameShared/GameClasses/Containers/CgsArray.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (all inline in
// CgsArray.h) for the Array<u16, 32> leaf instantiation -- the committed Array_/EventQueue_
// explicit-instantiation pattern (mirrors the sibling Array_short_256.cpp / Array_short_9.cpp).
//
// Element type is the X360-authoritative 16-bit word: the asm loads/stores the element with
// lhz/sthx (halfword) and indexes with a 2-byte stride (slwi ...,1). Capacity N=32 (Append
// out-of-space `cmplwi 0x20`); the live-count word sits at byte +0x40 == align4(32 * 2),
// initialised to the -1 sentinel until Construct/Clear runs. A primitive element needs no
// element_home include.
//
// These are the BrnGameState::TriggerQueryManager per-frame maLastPlayerTriggers /
// maLastFrameTriggers members (BrnTriggerQueryManager.h:130), siblings of the committed
// maActiveTriggers Array<u16,256>. Drivers: TriggerQueryManager::PreWorldUpdate
// (FindFirstInstanceOf/GetItem/GetLength), ::PostWorldUpdate (Append),
// TriggerQueryManagerDebugComponent::RenderWorld (GetLength).
//
//   X360 0x8235D7C0 = Array<u16,32>::Append                     -- CgsArray.h:225/226 asserts
//   X360 0x8235D8E0 = Array<u16,32>::GetLength (const)          -- CgsArray.h:336 assert
//   X360 0x8235D938 = Array<u16,32>::FindFirstInstanceOf(const) -- CgsArray.h:480 assert
//   X360 0x8235FEF8 = Array<u16,32>::GetItem   (non-const)      -- CgsArray.h:556/557 asserts
//
template void Array<u16, 32>::Append(const u16&);
template u32  Array<u16, 32>::GetLength() const;
template s32  Array<u16, 32>::FindFirstInstanceOf(const u16&) const;
template u16& Array<u16, 32>::GetItem(u32);
