#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/World/CrashModule/BrnTrafficCrash.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the BrnWorld::TrafficCrash,160 leaf instantiation -- the committed Array_/EventQueue_
// explicit-instantiation pattern. The X360 ARTIST build emitted these three out-of-line for the
// crash module's tracked-crashing-traffic array (Array<TrafficCrash,160>; sizeof TrafficCrash == 8,
// so the live-count word sits at +0x500 == 160*8 == 1280, matching every `lwz/stw 0x500(this)` and
// the 8-byte element stride `slwi ...,3` in the asm):
//   EraseFast(u32) @ 0x827B55E8  -- asserts constructed (count != -1, CgsArray.h:405) and
//                                   index < count ("Trying to erase an unused element", :406),
//                                   then copies the last live element over the erased slot and
//                                   decrements the count (2-word copy `*v4=*v5; v4[1]=v5[1]`).
//   GetItem(u32)   @ 0x827BA5D0  -- the bounds-checked indexed accessor (CgsArray.h:556/557),
//                                   returns &maElements[index] (8*index + base).
//   Grow()         @ 0x827B54D8  -- reserve the next free slot (CgsArray.h:289/290), returns
//                                   &maElements[count] (8*count + base) then count++.
template void                Array<BrnWorld::TrafficCrash, 160>::EraseFast(u32);
template BrnWorld::TrafficCrash& Array<BrnWorld::TrafficCrash, 160>::GetItem(u32);
template BrnWorld::TrafficCrash* Array<BrnWorld::TrafficCrash, 160>::Grow();
