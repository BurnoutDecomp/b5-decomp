#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<131072, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h; there is no per-instance code
// to author -- only the out-of-line emission forced by these `template` lines.
//
// X360 ledger for THIS instantiation (10 methods attested):
//   Construct @0x822C89F0, Clear @0x822AF4A0, GetEventPaddingSize @0x822AF630,
//   GetFirstEvent @0x822AD570, GetFirstWritePointer @0x823B0880,
//   GetLength @0x8235F840, GetNextEvent @0x822C8A98, GetSizeInBytes @0x823B0930,
//   OutputQueueContents @0x82374350, AddEvent @0x8237FE50.
// Only the non-const GetFirstWritePointer is attested for this instance.
//
// Capacity/stride from the asm: members at +131076/+131080/+131084 == BUFSIZE+4/+8/+12,
// BUFSIZE = 131072 (0x20000); ALIGN = 16 (sizeof(CBufferEntry) == 16).

template void   CgsModule::VariableEventQueue<131072, 16>::Construct();
template void   CgsModule::VariableEventQueue<131072, 16>::Clear();
template s32    CgsModule::VariableEventQueue<131072, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<131072, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template char*  CgsModule::VariableEventQueue<131072, 16>::GetFirstWritePointer();
template bool   CgsModule::VariableEventQueue<131072, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template s32    CgsModule::VariableEventQueue<131072, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<131072, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<131072, 16>::GetSizeInBytes() const;
template void   CgsModule::VariableEventQueue<131072, 16>::OutputQueueContents() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <131072,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<131072, 16>::Append<131072, 16>(const CgsModule::VariableEventQueue<131072, 16>&);
