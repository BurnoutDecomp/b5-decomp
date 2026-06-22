#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<16384, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so siblings not
// attributed to this instance stay un-instantiated.
// Ledger: 14 methods -- this is the "safe-API" instance: it emits the Safe variants
// (AddEventSafe / AddStringEventSafe / AllocateEventSafe) alongside the bring-up and
// accessor set, but NOT Prepare/Release/GetMaxLength.

template void   CgsModule::VariableEventQueue<16384, 16>::Construct();
template void   CgsModule::VariableEventQueue<16384, 16>::Clear();
template void   CgsModule::VariableEventQueue<16384, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<16384, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<16384, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template bool   CgsModule::VariableEventQueue<16384, 16>::AddEventSafe(const CgsModule::Event*, s32, s32);
template void*  CgsModule::VariableEventQueue<16384, 16>::AllocateEventSafe(s32, s32);
template bool   CgsModule::VariableEventQueue<16384, 16>::AddStringEventSafe(const char*, s32);
template void   CgsModule::VariableEventQueue<16384, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<16384, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<16384, 16>::GetFirstWritePointer() const;
