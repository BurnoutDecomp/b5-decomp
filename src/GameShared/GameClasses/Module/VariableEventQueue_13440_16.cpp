#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<13440, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission.
//
// BUFSIZE=13440 attested by AddEvent overflow check cmpwi r11,0x3480 (=13440) and the
// member displacements: miBufferWritePos @+0x3484 (=BUFSIZE+4), miLength @+0x3488
// (=BUFSIZE+8), miFirstEventOffset @+0x348C (=BUFSIZE+12); mbIsConstructed @+0,
// macData @+1. CBufferEntry stride 16 (ALIGN).

template void         CgsModule::VariableEventQueue<13440, 16>::Construct();
template void         CgsModule::VariableEventQueue<13440, 16>::Clear();
template s32          CgsModule::VariableEventQueue<13440, 16>::GetLength() const;
template s32          CgsModule::VariableEventQueue<13440, 16>::GetSizeInBytes() const;
template s32          CgsModule::VariableEventQueue<13440, 16>::GetEventPaddingSize(s32) const;
template s32          CgsModule::VariableEventQueue<13440, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32          CgsModule::VariableEventQueue<13440, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool         CgsModule::VariableEventQueue<13440, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void         CgsModule::VariableEventQueue<13440, 16>::OutputQueueContents() const;
template char*        CgsModule::VariableEventQueue<13440, 16>::GetFirstWritePointer();

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <13440,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<13440, 16>::Append<13440, 16>(const CgsModule::VariableEventQueue<13440, 16>&);
