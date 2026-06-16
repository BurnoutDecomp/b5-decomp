#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISectionsData

// Per-instantiation TU for CgsResource::ResourcePtr<BrnAI::AISectionsData>.
// Bodies are the generic inline accessors in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the two non-const symbols the X360 ARTIST build attested.
// (The ledger name "class:BrnAI::AISectionsData>" is a truncation of
//  CgsResource::ResourcePtr<BrnAI::AISectionsData>.)
//
//   operator->()        @ 0x82324FA0  (baked assert line 544)
//   GetMemoryResource() @ 0x8277AE80  (baked assert line 581)
template BrnAI::AISectionsData* CgsResource::ResourcePtr<BrnAI::AISectionsData>::operator->();
template BrnAI::AISectionsData* CgsResource::ResourcePtr<BrnAI::AISectionsData>::GetMemoryResource();
