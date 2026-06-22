#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<8192, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so safe siblings
// not attributed to this instance stay un-instantiated.
// Ledger: 12 methods (same as <4096,16>: Construct/Prepare set + accessors, NO Release).

template void   CgsModule::VariableEventQueue<8192, 16>::Construct();
template void   CgsModule::VariableEventQueue<8192, 16>::Clear();
template bool   CgsModule::VariableEventQueue<8192, 16>::Prepare();
template void   CgsModule::VariableEventQueue<8192, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<8192, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<8192, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<8192, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<8192, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<8192, 16>::GetFirstWritePointer() const;
