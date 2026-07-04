#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<BrnPhysics::Props::PropPhysicsDataHeader>.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   @ 0x822CA0A8 == CgsResource::ResourcePtr<BrnPhysics::Props::PropPhysicsDataHeader>::operator->() const.
//
// The X360 body is the generic CONST operator->() inline in CgsResourcePtr.h: read offset 0
// (mpResourceMemory), assert it non-null with the
//   "Can not instance resource pointer - it has no main memory resource\n"
// message (baked file CgsResourcePtr.h, baked line 563 == li r5,0x233 in the asm), and return it
// as const PropPhysicsDataHeader*. The asm loads *a1 twice -- once for the null test
// (cmpwi cr6,r11,0) and once for the return (r3 = 0(r28)) -- so it returns the pointer VALUE
// (operator->(), not operator*()). The "instance" message + baked line 563 pin this to the const
// operator->() overload (const operator*() uses the distinct "dereference" message at baked 637).
// This .cpp only forces the out-of-line emission of the one symbol the ARTIST build attested; the
// body lives in the ONE shared home (CgsResourcePtr.h). Mirrors the committed sibling
// CgsResourcePtr_BrnVehicle_GraphicsSpec.cpp.
//
// TYPE IS DWARF-ATTESTED (authoritative): BrnPropZoneManager.h:130 typedefs
//   PropPhysicsResourcePtr == CgsResource::ResourcePtr<BrnPhysics::Props::PropPhysicsDataHeader>
// and BrnPropZoneManager.cpp:687 calls
//   CgsResource::ResourcePtr<BrnPhysics::Props::PropPhysicsDataHeader>::operator->().
// Callers BrnWorld::PropZoneManager::UpdateInstance / ::UnloadZone. PropPhysicsDataHeader is a
// COMMITTED type (SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h); operator->() only
// static_casts a void* to const T*, so a forward decl suffices and avoids pulling the full header.
namespace BrnPhysics { namespace Props {
    class PropPhysicsDataHeader;   // full layout committed in BrnPropPhysicsDataHeader.h
}}

// operator->() const @ 0x822CA0A8 (baked assert line 563, "instance" msg).
template const BrnPhysics::Props::PropPhysicsDataHeader*
    CgsResource::ResourcePtr<BrnPhysics::Props::PropPhysicsDataHeader>::operator->() const;
