#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<135168, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission.
//
// Used by CgsSceneManager::SpatialPartitionManager (SpatialPartitionIO InputBuffer_Update).
// Capacity/stride X360-attested: BUFSIZE = 135168, ALIGN = 16. Clear @0x828AD4E8 stores
// miLength at this+0x21008 (BUFSIZE+8), miBufferWritePos at this+0x21004 (BUFSIZE+4),
// miFirstEventOffset at this+0x2100C (BUFSIZE+12). 16-byte CBufferEntry header.

template void CgsModule::VariableEventQueue<135168, 16>::Construct();
template void CgsModule::VariableEventQueue<135168, 16>::Clear();
template void CgsModule::VariableEventQueue<135168, 16>::Destruct();
template s32  CgsModule::VariableEventQueue<135168, 16>::GetEventPaddingSize(s32) const;
template s32  CgsModule::VariableEventQueue<135168, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
