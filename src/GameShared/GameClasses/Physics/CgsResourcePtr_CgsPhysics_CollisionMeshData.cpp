#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<CgsPhysics::CollisionMeshData>
// (the "CgsPhysics::CollisionMeshData>" templated-instance ledger id). The body IS the
// generic non-const accessor inline in CgsResourcePtr.h; this .cpp only forces the
// out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   operator->()  @ 0x828B7B98  (non-const; baked CgsResourcePtr.h line 0x220 = 544)
//
// X360 @0x828B7B98 (verified store-for-store against the assembly):
//   lwz   r11, 0(r28)        ; load mpResourceMemory (the +0 word)
//   cmplwi r11, 0            ; null-test it
//   bne   ...skip-assert     ; if non-null, skip
//   <BeginAssert / BasePriorityQueue::Clear (StrStream build) / FireAssert / EndAssert
//    with msg "Can not instance resource pointer - it has no main memory resource\n",
//    baked file CgsResourcePtr.h line 0x220 (544)>
//   lwz   r3, 0(r28)         ; return mpResourceMemory DIRECTLY (single deref)
// Byte-identical to the GuiHudMessageResource sibling @0x8267A448: a SINGLE load of
// offset 0, the "instance" assert at line 544, return the resource pointer (no double
// deref). Line 544 in CgsResourcePtr.h is the generic non-const operator->(). The
// X360-baked file/line are discarded per project convention -- CGS_ASSERT supplies
// __FILE__/__LINE__. Reached from CgsSceneManager::VolumeManager::GetRwVolume.
//
// CollisionMeshData stays an INCOMPLETE forward declaration here: the accessor only
// static_casts void* mpResourceMemory to CollisionMeshData* and returns it, which needs
// only an incomplete class type (never a member access or dereference). The complete
// CollisionMeshData type + its FixUp/FixDown live in CgsCollisionMeshData.cpp.
namespace CgsPhysics { struct CollisionMeshData; }

template CgsPhysics::CollisionMeshData*
    CgsResource::ResourcePtr<CgsPhysics::CollisionMeshData>::operator->();
