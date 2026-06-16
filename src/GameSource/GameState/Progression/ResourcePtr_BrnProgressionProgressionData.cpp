#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Progression/BrnProgressionResourceType.h"   // BrnProgression::ProgressionData

// Per-instantiation TU for CgsResource::ResourcePtr<BrnProgression::ProgressionData>.
// Bodies are the generic inline accessors in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the three symbols the X360 ARTIST build attested.
// (The ledger names "BrnProgression::ProgressionData / ProgressionData> / ProfileUpgrad"
//  are truncated PPC demangles; all three are ResourcePtr<ProgressionData> accessors.)
//
//   operator*()         @ 0x82325790  (baked assert line 563)
//   operator->()        @ 0x823690C0  (baked assert line 544)
//   GetMemoryResource() @ 0x824FBDD8  (baked assert line 581)
template BrnProgression::ProgressionData& CgsResource::ResourcePtr<BrnProgression::ProgressionData>::operator*();
template BrnProgression::ProgressionData* CgsResource::ResourcePtr<BrnProgression::ProgressionData>::operator->();
template BrnProgression::ProgressionData* CgsResource::ResourcePtr<BrnProgression::ProgressionData>::GetMemoryResource();
