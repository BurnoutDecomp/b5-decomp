#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<BrnWorld::EnvironmentSettings::TimeLine>.
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only forces the
// out-of-line emission of the single symbol the X360 ARTIST build attested:
//
//   GetMemoryResource()  @ 0x827C3048  (NON-const; baked null-resource assert at
//                                        CgsResourcePtr.h:581 -- `li r5, 0x245` == 581)
//
// X360 body (0x827C3048): reads *this (the leading dword == mpResourceMemory) once, asserts it
// non-null (de-inlined BeginAssert / ... / FireAssert / EndAssert -> one CGS_ASSERT, message
// "Can not instance resource pointer - it has no main memory resource\n"), then reloads and
// returns it as the TimeLine*. This is exactly the generic ResourcePtr<Type>::GetMemoryResource()
// NON-const accessor: line 581 discriminates it from the const overload (:599) and from
// operator->/operator* (:544/:563/:612).
//
// Callers (DWARF BrnEnvironmentManager.cpp:717/732/758): BrnWorld::EnvironmentSettings::
// EnvironmentManager::{RequestNextSeason, StreamOut, StreamIn, SetupBlend} and
// DebugComponent::RenderHUD, all of which instance the season timeline resource pointer
// (DWARF BrnEnvironmentManager.h:214 maSeasonPtrs -> ResourcePtr<TimeLine>[2]).
//
// TimeLine (the season-timeline resource) is forward-declared incomplete -- following the
// CgsResourcePtr_BrnWheel_GraphicsSpec precedent, the generic body only needs
// static_cast<Type*>(void*). Type + namespace + callers confirmed against the DecFIGS DWARF
// (BrnEnvironmentManager.h/.cpp).
namespace BrnWorld { namespace EnvironmentSettings { struct TimeLine; } }

template BrnWorld::EnvironmentSettings::TimeLine*
CgsResource::ResourcePtr<BrnWorld::EnvironmentSettings::TimeLine>::GetMemoryResource();
