#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<CgsResource::AlignedBinaryFileResource>
// (IDA-truncated ledger id "CgsResource::AlignedBinar"). The body IS the generic non-const
// operator->() already inline in CgsResourcePtr.h; this .cpp only forces out-of-line emission of
// the one symbol the X360 ARTIST build attested:
//
//   operator->()  @ 0x826AAD48  (non-const; baked CgsResourcePtr.h line 0x220 = 544)
//
// X360 @0x826AAD48 (verified store-for-store): lwz r11,0(this)=mpResourceMemory (the +0 word),
// null-test, fire the "Can not instance resource pointer - it has no main memory resource\n"
// assert (baked line 544) when null, then lwz r3,0(this) return mpResourceMemory DIRECTLY (single
// deref). Byte-identical to the committed CgsResourcePtr_CgsPhysics_CollisionMeshData.cpp
// operator->() @0x828B7B98. Baked file/line discarded per project convention. Reached from
// CgsResource::AlignedBinaryFileResource::UpdateResourceMo and
// CgsSound::Playback::GenericRwacReverbIRContent::DoGetData (mLoader embeds a
// ResourcePtr<AlignedBinaryFileResource>).
//
// AlignedBinaryFileResource stays an INCOMPLETE forward declaration here (the accessor only
// static_casts void* mpResourceMemory to it): matches the sole committed forward-decl
// (`struct AlignedBinaryFileResource;` in CgsGenericRwacContent.h). Complete type lives with
// CgsAlignedBinaryFileResource.h.
namespace CgsResource { struct AlignedBinaryFileResource; }

template CgsResource::AlignedBinaryFileResource*
    CgsResource::ResourcePtr<CgsResource::AlignedBinaryFileResource>::operator->();
