#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<5040, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission. Per-method
// (not whole-class) so the declared-only safe siblings (AddEventSafe/AllocateEventSafe/
// AddStringEvent(Safe)/GetMaxLength) stay un-instantiated, matching the X360 ledger.
// Ledger: 10 methods (Construct/Clear + accessors + AddEvent + GetSizeInBytes +
// GetFirstWritePointer both; NO Prepare/Release/Destruct/AllocateEvent for this instance).

template void CgsModule::VariableEventQueue<5040, 16>::Construct();
template void CgsModule::VariableEventQueue<5040, 16>::Clear();
template s32  CgsModule::VariableEventQueue<5040, 16>::GetLength() const;
template s32  CgsModule::VariableEventQueue<5040, 16>::GetSizeInBytes() const;
template s32  CgsModule::VariableEventQueue<5040, 16>::GetEventPaddingSize(s32) const;
template s32  CgsModule::VariableEventQueue<5040, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32  CgsModule::VariableEventQueue<5040, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool CgsModule::VariableEventQueue<5040, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void CgsModule::VariableEventQueue<5040, 16>::OutputQueueContents() const;
template char* CgsModule::VariableEventQueue<5040, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<5040, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <5040,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<5040, 16>::AddEvent<BrnPhysics::Vehicle::BrnPlayerDriverControls>(const BrnPhysics::Vehicle::BrnPlayerDriverControls*, s32);
// The Network / AI / Traffic driver-control variants (opaque payload homes in
// BrnVehicleDriverControls.h). Each typed thunk forwards to the three-arg AddEvent with
// liSize == sizeof(EventT); the sizeof is pinned to the record-size immediate baked into the
// X360 thunk (li r6,0xNN), so each instance's payload is a complete, correctly-sized type.
static_assert(sizeof(BrnPhysics::Vehicle::BrnAIDriverControls) == 0x50,
              "BrnAIDriverControls must be 80 bytes (X360 AddEvent thunk @0x82794C50 li r6,0x50)");
static_assert(sizeof(BrnPhysics::Vehicle::BrnNetworkDriverControls) == 0xC0,
              "BrnNetworkDriverControls must be 192 bytes (X360 AddEvent thunk @0x82595618 li r6,0xC0)");
static_assert(sizeof(BrnPhysics::Vehicle::BrnTrafficDriverControls) == 0x48,
              "BrnTrafficDriverControls must be 72 bytes (X360 AddEvent thunk @0x82746808 li r6,0x48)");
template bool CgsModule::VariableEventQueue<5040, 16>::AddEvent<BrnPhysics::Vehicle::BrnAIDriverControls>(const BrnPhysics::Vehicle::BrnAIDriverControls*, s32);
template bool CgsModule::VariableEventQueue<5040, 16>::AddEvent<BrnPhysics::Vehicle::BrnNetworkDriverControls>(const BrnPhysics::Vehicle::BrnNetworkDriverControls*, s32);
template bool CgsModule::VariableEventQueue<5040, 16>::AddEvent<BrnPhysics::Vehicle::BrnTrafficDriverControls>(const BrnPhysics::Vehicle::BrnTrafficDriverControls*, s32);
template bool CgsModule::VariableEventQueue<5040, 16>::Append<5040, 16>(const CgsModule::VariableEventQueue<5040, 16>&);
