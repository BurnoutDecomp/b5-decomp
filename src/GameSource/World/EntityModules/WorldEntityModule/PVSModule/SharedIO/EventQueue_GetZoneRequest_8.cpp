#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h"

// Explicit-instantiation TU for the GetZoneRequest-element fixed-capacity queue the
// X360 build emitted out-of-line. The generic bodies are committed inline in the
// CgsModule::EventQueue<T,N> / BaseEventQueue<T> templates; this TU only forces the
// per-instantiation emission and pins the layout the X360 Construct/AddEvent attest.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsModule::EventQueue<BrnWorld::PVSIO::GetZoneRequest, 8>::Construct  @ 0x822E5140
//     points the base queue at its inline maEvents (this + 0x10, 16-aligned because
//     GetZoneRequest carries a Vector4), sets miMaxLength = 8, clears miLength. The
//     X360 asserts the buffer is non-null ("lpEventBuffer != NULL",
//     CgsBaseEventQueue.h:160) before storing it -- that assert lives in
//     BaseEventQueue::Construct. Called by BrnWorld::PVSIO::InputBuffer::Construct.
//   CgsModule::BaseEventQueue<BrnWorld::PVSIO::GetZoneRequest>::AddEvent  @ 0x822C9CD0
//     appends a copy of the event (the X360 emits the qword do-loop copying 8 qwords
//     == 64 bytes == sizeof(GetZoneRequest), stride `miLength << 6`), bumps miLength,
//     returns 1. The two asserts ("mpEvents != NULL" :312, "Reached Max length" :313)
//     are non-gating tripwires -- the committed generic AddEvent matches. Called by
//     BrnWorld::WorldEntityModule::PreSceneUpdate.

// Lock the element stride (64) the X360 AddEvent attests (`miLength << 6` / 8-qword copy).
static_assert(sizeof(BrnWorld::PVSIO::GetZoneRequest) == 64, "GetZoneRequest stride drift");

template void
CgsModule::EventQueue<BrnWorld::PVSIO::GetZoneRequest, 8>::Construct();

template bool
CgsModule::BaseEventQueue<BrnWorld::PVSIO::GetZoneRequest>::AddEvent(const BrnWorld::PVSIO::GetZoneRequest&);
