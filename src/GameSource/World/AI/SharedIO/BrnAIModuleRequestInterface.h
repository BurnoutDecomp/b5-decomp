#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by AIModuleRequestInterface's own TU (DWARF home
// GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h). Size 256 (NOMINAL --
// not byte-verified, grown by own TU).
//
// Opaque sub-interface payload embedded BY VALUE in OutputBuffer_PostScene
// (mAIModuleRequestInterface :389). The IO header only returns &member, so a
// complete sized blob suffices here. Real layout (DWARF
// BrnAIModuleRequestInterface.h:109) is a single member:
//   EventQueue<BrnAI::AIModuleIO::ResetOnTrackRequest, 128> mResetOnTrackRequestQueue;
// reconstructed by this type's own ledger TU (pulls in the real EventQueue generic
// + ResetOnTrackRequest element cascade -- NOT instantiated here).
//
// alignas(16): the DWARF shows the member is an EventQueue instantiation.

#include "types.hpp"   // u8 (reserved blob)

namespace BrnAI
{
    namespace AIModuleIO
    {
        // DWARF: BrnAIModuleRequestInterface.h:89 (struct BrnAI::AIModuleIO::AIModuleRequestInterface).
        // Wraps an EventQueue<ResetOnTrackRequest,128> reset-on-track request queue.
        struct alignas(16) AIModuleRequestInterface
        {
            unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
        };
    }
}
