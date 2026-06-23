#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"

// Explicit instantiation of CgsModule::EventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest, 128>::Construct.
// X360 @0x822E2870. The X360 emits each EventQueue instantiation's Construct out-of-line; the
// shared generic body lives in CgsEventQueue.h and points the base queue at the inline
// maEvents buffer, sets miMaxLength = 128, and clears miLength. ResetOnTrackRequest is 16 bytes
// with alignment 4, so the inline maEvents lands at this+0x0C (12-byte base, no pad) -- matching
// the constructor's `addi r30, r31, 0xC` and `stw 0x80 (=128) @+4`.

template void CgsModule::EventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest, 128>::Construct();
