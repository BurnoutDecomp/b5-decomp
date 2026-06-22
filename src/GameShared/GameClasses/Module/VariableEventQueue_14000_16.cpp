#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<14000, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission. Per-method
// (not whole-class) so the declared-only safe siblings the ledger does NOT attribute
// to this instance (AddEventSafe/AllocateEvent(Safe)/AddStringEvent(Safe)/GetMaxLength)
// stay un-instantiated, matching the X360 ledger (13 methods).

template void   CgsModule::VariableEventQueue<14000, 16>::Construct();
template void   CgsModule::VariableEventQueue<14000, 16>::Clear();
template bool   CgsModule::VariableEventQueue<14000, 16>::Prepare();
template bool   CgsModule::VariableEventQueue<14000, 16>::Release();
template void   CgsModule::VariableEventQueue<14000, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<14000, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<14000, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<14000, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<14000, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<14000, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<14000, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<14000, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<14000, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<14000, 16>::GetFirstWritePointer() const;
