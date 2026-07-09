#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<65536, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so safe siblings
// not attributed to this instance stay un-instantiated.
// Ledger: 12 methods (AddEvent + AddEventSafe, but NO Construct/Prepare/Release here --
// this instance is brought up elsewhere; this TU emits the accessor/add set only).

template void   CgsModule::VariableEventQueue<65536, 16>::Construct();
template void   CgsModule::VariableEventQueue<65536, 16>::Clear();
template void   CgsModule::VariableEventQueue<65536, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<65536, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<65536, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<65536, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<65536, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<65536, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<65536, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template bool   CgsModule::VariableEventQueue<65536, 16>::AddEventSafe(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<65536, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<65536, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<65536, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <65536,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<65536, 16>::Append<65536, 16>(const CgsModule::VariableEventQueue<65536, 16>&);
template bool CgsModule::VariableEventQueue<65536, 16>::AppendSafe<32768, 16>(const CgsModule::VariableEventQueue<32768, 16>&);
template bool CgsModule::VariableEventQueue<65536, 16>::AppendSafe<65536, 16>(const CgsModule::VariableEventQueue<65536, 16>&);
