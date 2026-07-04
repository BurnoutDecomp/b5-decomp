#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<CgsResource::VideoDataResource>
// (ledger id: class:CgsResource::VideoDataResource>).
//
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only forces the
// out-of-line emission of the single symbol the X360 ARTIST build attested:
//
//   operator->()  @ 0x824FBBF0  (non-const)
//
// X360 @0x824FBBF0 is a SINGLE deref: `lwz r11,0(r28)` loads mpResourceMemory at +0x00,
// `cmplwi/bne` asserts it non-null ("Can not instance resource pointer - it has no main
// memory resource\n", baked CgsResourcePtr.h:544 discarded per project convention), then
// `lwz r3,0(r28)` returns that same +0 word directly as VideoDataResource*. One load of
// offset 0, returned as-is => the generic non-const ResourcePtr<Type>::operator->() inline
// in CgsResourcePtr.h -- NOT the double-deref member specialization used for
// ResourcePtr<Font> (0x823C1C20). So this TU is a pure explicit-instantiation line; the
// body lives in the ONE shared home (CgsResourcePtr.h). Called by
// BrnGui::MovieManager::QueueNextMovie (member
// CgsResource::ResourcePtr<CgsResource::VideoDataResource> mpVideoDataResource,
// BrnGuiMovieManager.h:161).
//
// CgsResource::VideoDataResource is forward-declared (NOT #include "CgsVideoDataResource.h"):
// operator->() only static_casts a void* (mpResourceMemory) to VideoDataResource* and
// returns it -- a pointer static_cast from void* needs only an incomplete class type. The
// committed complete home is CgsVideoDataResource.h (`class VideoDataResource`).
namespace CgsResource { class VideoDataResource; }

// operator->() (non-const) @ 0x824FBBF0.
template CgsResource::VideoDataResource*
CgsResource::ResourcePtr<CgsResource::VideoDataResource>::operator->();
