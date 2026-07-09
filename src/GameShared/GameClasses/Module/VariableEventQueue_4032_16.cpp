#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<4032, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission.
// Ledger: 10 methods for this instance --
//   Clear @0x821FFAB8, Construct @0x822116D8, Destruct @0x822118E0,
//   GetEventPaddingSize @0x821FFC30, GetFirstEvent @0x821FC180, GetLength @0x823B0BA0,
//   GetNextEvent @0x82211988, OutputQueueContents @0x823C8298, Prepare @0x82211790,
//   Release @0x82211838.
// BUFSIZE=4032 => miBufferWritePos@+0xFC4(4036), miLength@+0xFC8(4040),
// miFirstEventOffset@+0xFCC(4044) (base+0 mbIsConstructed, +1 macData[4032]).

template void        CgsModule::VariableEventQueue<4032, 16>::Construct();
template void        CgsModule::VariableEventQueue<4032, 16>::Clear();
template bool        CgsModule::VariableEventQueue<4032, 16>::Prepare();
template bool        CgsModule::VariableEventQueue<4032, 16>::Release();
template void        CgsModule::VariableEventQueue<4032, 16>::Destruct();
template s32         CgsModule::VariableEventQueue<4032, 16>::GetLength() const;
template s32         CgsModule::VariableEventQueue<4032, 16>::GetEventPaddingSize(s32) const;
template s32         CgsModule::VariableEventQueue<4032, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32         CgsModule::VariableEventQueue<4032, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template void        CgsModule::VariableEventQueue<4032, 16>::OutputQueueContents() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <4032,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<4032, 16>::Append<32768, 16>(const CgsModule::VariableEventQueue<32768, 16>&);
