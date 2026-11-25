#define CGS_RESOURCE_PTR_MATCH_RELATIVE_ASSERTS // ARTIST 0x82324F00 references 0x82007258.
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/StreetData/BrnStreetDataResourceType.h"   // BrnStreetData::StreetData
// Per-instantiation TU for CgsResource::ResourcePtr<BrnStreetData::StreetData>.
// Bodies are the generic inline accessors in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the three symbols the X360 ARTIST build attested.
//
//   operator->() const  @ 0x82324DC0  (baked assert line 563)
//   operator->()        @ 0x82324E60  (baked assert line 544)
//   GetMemoryResource() @ 0x82324F00  (baked assert line 581)
template const BrnStreetData::StreetData* CgsResource::ResourcePtr<BrnStreetData::StreetData>::operator->() const;
template BrnStreetData::StreetData* CgsResource::ResourcePtr<BrnStreetData::StreetData>::operator->();
template BrnStreetData::StreetData* CgsResource::ResourcePtr<BrnStreetData::StreetData>::GetMemoryResource();
