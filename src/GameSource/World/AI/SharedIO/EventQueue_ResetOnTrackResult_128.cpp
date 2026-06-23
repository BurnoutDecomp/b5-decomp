#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"

// Explicit instantiation of CgsModule::EventQueue<BrnAI::AIModuleIO::ResetOnTrackResult, 128>::Construct.
// X360 @0x822E28E0. Shared generic body in CgsEventQueue.h: point the base queue at the inline
// maEvents buffer, set miMaxLength = 128, clear miLength. ResetOnTrackResult is 16-byte aligned
// (it embeds two Vector3s), so maEvents lands at this+0x10 (12-byte base + 4 pad) -- matching the
// constructor's `addi r30, r31, 0x10` and `stw 0x80 (=128) @+4`.

template void CgsModule::EventQueue<BrnAI::AIModuleIO::ResetOnTrackResult, 128>::Construct();
