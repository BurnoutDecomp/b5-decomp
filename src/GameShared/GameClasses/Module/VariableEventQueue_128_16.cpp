#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<128, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission. Per-method
// (not whole-class) so the declared-only safe siblings (AddEventSafe/AllocateEventSafe/
// AddStringEvent(Safe)/GetMaxLength) stay un-instantiated, matching the X360 ledger.
// Ledger: 8 methods (NO Prepare/Release/Destruct/AllocateEvent/GetSizeInBytes/
// GetFirstWritePointer for this instance).

template void CgsModule::VariableEventQueue<128, 16>::Construct();
template void CgsModule::VariableEventQueue<128, 16>::Clear();
template s32  CgsModule::VariableEventQueue<128, 16>::GetLength() const;
template s32  CgsModule::VariableEventQueue<128, 16>::GetEventPaddingSize(s32) const;
template s32  CgsModule::VariableEventQueue<128, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32  CgsModule::VariableEventQueue<128, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool CgsModule::VariableEventQueue<128, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void CgsModule::VariableEventQueue<128, 16>::OutputQueueContents() const;
