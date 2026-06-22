#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<4096, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so the safe
// siblings the ledger does NOT attribute to this instance stay un-instantiated.
// Ledger: 12 methods (same as <14000,16> but NO Release).

template void   CgsModule::VariableEventQueue<4096, 16>::Construct();
template void   CgsModule::VariableEventQueue<4096, 16>::Clear();
template bool   CgsModule::VariableEventQueue<4096, 16>::Prepare();
template void   CgsModule::VariableEventQueue<4096, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<4096, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<4096, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<4096, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<4096, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<4096, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<4096, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<4096, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<4096, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<4096, 16>::GetFirstWritePointer() const;
