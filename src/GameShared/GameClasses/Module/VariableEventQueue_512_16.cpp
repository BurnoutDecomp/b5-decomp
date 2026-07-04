#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<512, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (each instantiation emits methods out-of-line).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method (not whole-class) so
// declared-only siblings and un-emitted Prepare/Release/Destruct/AllocateEvent/
// const-GetFirstWritePointer stay un-instantiated, matching the X360 ledger for <512,16>.
//
// Ledger: Construct 0x822140B8, Clear 0x82201E18, GetLength 0x82202CA8,
// GetSizeInBytes 0x822AE9A0, GetEventPaddingSize 0x82202AA0, GetFirstEvent 0x82202D48,
// GetNextEvent 0x82215268, AddEvent 0x8224B828, OutputQueueContents 0x82230CC8,
// GetFirstWritePointer(non-const) 0x822AE8F8.
// BUFSIZE = 512 (cmpwi 0x200 in AddEvent overflow guard); ALIGN = 16.
// miBufferWritePos@0x204, miLength@0x208, miFirstEventOffset@0x20C.

template void  CgsModule::VariableEventQueue<512, 16>::Construct();
template void  CgsModule::VariableEventQueue<512, 16>::Clear();
template s32   CgsModule::VariableEventQueue<512, 16>::GetLength() const;
template s32   CgsModule::VariableEventQueue<512, 16>::GetSizeInBytes() const;
template s32   CgsModule::VariableEventQueue<512, 16>::GetEventPaddingSize(s32) const;
template s32   CgsModule::VariableEventQueue<512, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32   CgsModule::VariableEventQueue<512, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool  CgsModule::VariableEventQueue<512, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void  CgsModule::VariableEventQueue<512, 16>::OutputQueueContents() const;
template char* CgsModule::VariableEventQueue<512, 16>::GetFirstWritePointer();
