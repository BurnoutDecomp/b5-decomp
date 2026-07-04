#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnVehicle::GraphicsSpec>::operat  @ 0x822C81C8
//     == CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>::operator->() const.
//
// The X360 body is the generic CONST operator->() inline in CgsResourcePtr.h:
// read offset 0 (mpResourceMemory), assert it non-null with the
//   "Can not instance resource pointer - it has no main memory resource\n"
// message (baked file CgsResourcePtr.h, baked line 563 == li r5,0x233 in the
// asm), and return it as const GraphicsSpec*. The asm loads *a1 twice: once for
// the null test (cmpwi cr6,r11,0) and once for the return (result = *a1) -- the
// generic returns the pointer VALUE, i.e. operator->() not operator*(). The
// "instance" message + baked line 563 pin this to the const operator->() overload
// (the const operator*() uses the distinct "dereference" message at baked 637).
// This .cpp only forces the out-of-line emission of the one symbol the ARTIST
// build attested; the body lives in the ONE shared home (CgsResourcePtr.h).
//
// Called by BrnWorld::RaceCarEntityModule::RenderRaceCar (the vehicle graphics
// spec is dereferenced during race-car render setup).
//
// BrnVehicle::GraphicsSpec is a COMMITTED struct whose full layout is TU-local in
// BrnVehicleGraphicsSpecResourceType.cpp (no exposed header); it is forward-
// declared in BrnRaceCarStreamer.h:49 alongside the GraphicsResourcePtr typedef
// (== ResourcePtr<BrnVehicle::GraphicsSpec>). ResourcePtr<T>'s operator->() only
// static_casts a void* to const Type*, so an incomplete (forward) declaration is
// sufficient here and avoids forking the layout.
namespace BrnVehicle
{
    struct GraphicsSpec;   // full layout committed TU-local in BrnVehicleGraphicsSpecResourceType.cpp
}

// operator->() const @ 0x822C81C8 (baked assert line 563, "instance" msg).
template const BrnVehicle::GraphicsSpec*
    CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>::operator->() const;
