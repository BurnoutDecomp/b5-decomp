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
template bool CgsModule::VariableEventQueue<5040, 16>::Append<5040, 16>(const CgsModule::VariableEventQueue<5040, 16>&);
