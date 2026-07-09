#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<18432, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). This size set is X360-only (absent from the PS3 DWARF). The
// shared generic bodies live in CgsVariableEventQueue.h; these per-method lines force
// this instantiation's out-of-line emission. Per-method (not whole-class) so the
// declared-only safe siblings (AddEventSafe/AllocateEventSafe/AddStringEvent(Safe)/
// GetMaxLength) stay un-instantiated, matching the X360 ledger.

template void CgsModule::VariableEventQueue<18432, 16>::Construct();
template void CgsModule::VariableEventQueue<18432, 16>::Clear();
template bool CgsModule::VariableEventQueue<18432, 16>::Prepare();
template bool CgsModule::VariableEventQueue<18432, 16>::Release();
template void CgsModule::VariableEventQueue<18432, 16>::Destruct();
template s32  CgsModule::VariableEventQueue<18432, 16>::GetLength() const;
template s32  CgsModule::VariableEventQueue<18432, 16>::GetSizeInBytes() const;
template s32  CgsModule::VariableEventQueue<18432, 16>::GetEventPaddingSize(s32) const;
template s32  CgsModule::VariableEventQueue<18432, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32  CgsModule::VariableEventQueue<18432, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool CgsModule::VariableEventQueue<18432, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void* CgsModule::VariableEventQueue<18432, 16>::AllocateEvent(s32, s32);
template void CgsModule::VariableEventQueue<18432, 16>::OutputQueueContents() const;
template char* CgsModule::VariableEventQueue<18432, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<18432, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <18432,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<18432, 16>::Append<18432, 16>(const CgsModule::VariableEventQueue<18432, 16>&);
template bool CgsModule::VariableEventQueue<18432, 16>::Append<256, 16>(const CgsModule::VariableEventQueue<256, 16>&);
template bool CgsModule::VariableEventQueue<18432, 16>::Append<4096, 16>(const CgsModule::VariableEventQueue<4096, 16>&);
