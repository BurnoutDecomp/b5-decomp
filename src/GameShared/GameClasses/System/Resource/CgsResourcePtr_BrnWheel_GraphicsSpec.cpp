#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>.
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only
// forces the out-of-line emission of the one symbol the X360 ARTIST build
// attested:
//
//   operator->() const  @ 0x822C8268  (baked assert line 563, CONST)
//
// X360 body (0x822C8268): reads *this (the leading dword == mpResourceMemory)
// once, asserts it non-null (de-inlined BeginAssert/Clear/FireAssert/EndAssert ->
// one CGS_ASSERT, message "Can not instance resource pointer - it has no main
// memory resource\n"), then reloads and returns it as the GraphicsSpec*. This is
// exactly the generic ResourcePtr<Type>::operator->() CONST accessor. The baked
// file/line CgsResourcePtr.h:563 discriminates the overload: line 563 == the
// const operator->() (matching the committed Flapt @0x8246E2F8 and BrnTrigger
// @0x82211DA0 const siblings); the NON-const operator->() bakes line 544
// (committed StreetData @0x82324E60). asm `li r5, 0x233` = 563 -> const overload.
//
// BrnWheel::GraphicsSpec is forward-declared (NOT #include'd): its complete layout
// is a .cpp-local struct in SharedClasses/World/BrnWheelGraphicsSpecResourceType.cpp
// (line 14) and is not exposed by any header; the static_cast<const Type*>(void*)
// in the generic body only needs the incomplete type. Namespace+struct confirmed
// via forward decl in BrnRaceCarStreamer.h:50. Caller:
// BrnWorld::RaceCarEntityModule::RenderRaceCar.
namespace BrnWheel { struct GraphicsSpec; }

template const BrnWheel::GraphicsSpec* CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>::operator->() const;
