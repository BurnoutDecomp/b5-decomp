#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<1024, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission. Per-method
// (not whole-class) so the declared-only safe siblings (AddEventSafe/AllocateEventSafe/
// AddStringEvent(Safe)/GetMaxLength) stay un-instantiated, matching the X360 ledger.
// Ledger: full bring-up set (14 functions: Construct/Prepare/Release/Destruct/Clear +
// accessors + AddEvent + AllocateEvent + GetSizeInBytes + GetFirstWritePointer both).

template void CgsModule::VariableEventQueue<1024, 16>::Construct();
template bool CgsModule::VariableEventQueue<1024, 16>::Prepare();
template bool CgsModule::VariableEventQueue<1024, 16>::Release();
template void CgsModule::VariableEventQueue<1024, 16>::Destruct();
template void CgsModule::VariableEventQueue<1024, 16>::Clear();
template s32  CgsModule::VariableEventQueue<1024, 16>::GetLength() const;
template s32  CgsModule::VariableEventQueue<1024, 16>::GetSizeInBytes() const;
template s32  CgsModule::VariableEventQueue<1024, 16>::GetEventPaddingSize(s32) const;
template s32  CgsModule::VariableEventQueue<1024, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32  CgsModule::VariableEventQueue<1024, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool CgsModule::VariableEventQueue<1024, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void* CgsModule::VariableEventQueue<1024, 16>::AllocateEvent(s32, s32);
template void CgsModule::VariableEventQueue<1024, 16>::OutputQueueContents() const;
template char* CgsModule::VariableEventQueue<1024, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<1024, 16>::GetFirstWritePointer() const;
