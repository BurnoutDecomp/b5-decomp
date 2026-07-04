#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<3072, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so the safe
// siblings the ledger does NOT attribute to this instance stay un-instantiated.
// Ledger: 10 methods (used by BrnGameState::GameStateModuleIO::OutputBuffer).
//
// BUFSIZE=3072 (0xC00): miBufferWritePos@0xC04, miLength@0xC08, miFirstEventOffset@0xC0C
// (== BUFSIZE+4/+8/+12); mbIsConstructed @+0, macData @+1; CBufferEntry stride 0x10.

template void   CgsModule::VariableEventQueue<3072, 16>::Construct();
template void   CgsModule::VariableEventQueue<3072, 16>::Clear();
template s32    CgsModule::VariableEventQueue<3072, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<3072, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<3072, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<3072, 16>::GetFirstWritePointer();
