#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"   // complete BrnWorld::GlobalColourPalette

// Per-instantiation TU for CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>.
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   operator->()  @ 0x822C9A48  (baked assert line 544, non-const,
//                                "Can not instance resource pointer - it has no
//                                main memory resource"; asserts mpResourceMemory
//                                non-null then returns it as GlobalColourPalette*)
//
// The X360 body is: if (!mpResourceMemory) Begin/Fire/EndAssert(...); return
// mpResourceMemory;  -- i.e. the generic ResourcePtr<Type>::operator->() with
// Type = BrnWorld::GlobalColourPalette. Eleven call sites in
// BrnWorld::RaceCarEntityModule dereference this palette pointer.
template BrnWorld::GlobalColourPalette*
CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>::operator->();
