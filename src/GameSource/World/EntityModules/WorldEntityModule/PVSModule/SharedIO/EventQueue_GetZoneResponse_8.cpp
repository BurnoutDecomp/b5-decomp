#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h"

// Explicit-instantiation TU for the GetZoneResponse-element fixed-capacity queue the
// X360 build emitted out-of-line. The generic bodies are committed inline in the
// CgsModule::EventQueue<T,N> / BaseEventQueue<T> templates; this TU only forces the
// per-instantiation emission and pins the layout the X360 Construct/AddEvent attest.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsModule::EventQueue<BrnWorld::PVSIO::GetZoneResponse, 8>::Construct  @ 0x822E51B0
//     points the base queue at its inline maEvents (this + 0x10, 16-aligned), sets
//     miMaxLength = 8, clears miLength. The "lpEventBuffer != NULL" assert
//     (CgsBaseEventQueue.h:160) lives in BaseEventQueue::Construct. Called by
//     BrnWorld::PVSIO::OutputBuffer::Construct (which then places its
//     VariableEventQueue<512,16> at +0x1718, i.e. 0x10 + 8*736).
//   CgsModule::BaseEventQueue<BrnWorld::PVSIO::GetZoneResponse>::AddEvent  @ 0x822C9E28
//     appends a copy of the event (the X360 emits `memcpy(736*miLength + mpEvents,
//     src, 736)`, stride 736 == sizeof(GetZoneResponse)), bumps miLength, returns 1.
//     The two asserts (:312/:313) are non-gating tripwires -- the committed generic
//     AddEvent (which copies via T::operator=) matches semantically. Called by
//     BrnWorld::PVSModule::Update.

// Lock the element stride (736) the X360 AddEvent memcpy attests.
static_assert(sizeof(BrnWorld::PVSIO::GetZoneResponse) == 736, "GetZoneResponse stride drift");

template void
CgsModule::EventQueue<BrnWorld::PVSIO::GetZoneResponse, 8>::Construct();

template bool
CgsModule::BaseEventQueue<BrnWorld::PVSIO::GetZoneResponse>::AddEvent(const BrnWorld::PVSIO::GetZoneResponse&);
