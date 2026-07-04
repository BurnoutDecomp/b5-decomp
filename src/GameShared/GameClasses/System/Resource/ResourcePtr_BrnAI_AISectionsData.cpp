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
//   ResourcePtr(const ResourceHandle&) ctor @ 0x8277AE28 (BrnAI::AIModule::Prepare)
template BrnAI::AISectionsData* CgsResource::ResourcePtr<BrnAI::AISectionsData>::operator->();
template BrnAI::AISectionsData* CgsResource::ResourcePtr<BrnAI::AISectionsData>::GetMemoryResource();

// ResourcePtr(const ResourceHandle&) ctor @ 0x8277AE28: zero the base identity, self-link
// the intrusive alias list, clear muThreadId, then BaseResourcePtr::CreateFromHandle(&lrHandle).
// The generic ctor body is inline in CgsResourcePtr.h; this TU only forces its out-of-line
// emission for the AISectionsData instantiation.
template CgsResource::ResourcePtr<BrnAI::AISectionsData>::ResourcePtr(const CgsResource::ResourceHandle& lrHandle);
