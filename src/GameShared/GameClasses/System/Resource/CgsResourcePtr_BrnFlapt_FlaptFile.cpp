#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"   // BrnFlapt::FlaptFile (complete type)

// Per-instantiation TU for CgsResource::ResourcePtr<BrnFlapt::FlaptFile>.
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only
// forces the out-of-line emission of the one symbol the X360 ARTIST build
// attested:
//
//   operator->() const  @ 0x8246E2F8  (baked assert line 563, const)
//
// X360 body (0x8246E2F8): asserts the base resource pointer is non-null
// ("Can not instance resource pointer - it has no main memory resource\n") then
// returns *this (the leading dword == mpResourceMemory) as the FlaptFile*. This
// is exactly the generic ResourcePtr<Type>::operator->() const.
template const BrnFlapt::FlaptFile* CgsResource::ResourcePtr<BrnFlapt::FlaptFile>::operator->() const;
