#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by AIModuleResultInterface's own TU (DWARF home
// GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h). Size 256 (NOMINAL --
// not byte-verified, grown by own TU).
//
// Opaque sub-interface payload embedded BY VALUE in InputBuffer_PrePhysics
// (mAIModuleResultInterface :443). The IO header only returns &member, so a
// complete sized blob suffices here. Real layout (DWARF
// BrnAIModuleResultInterface.h:148-149) is two members:
//   EventQueue<BrnAI::AIModuleIO::ResetOnTrackResult, 128> mResetPosResultEventQueue;
//   EventQueue<BrnAI::AIModuleIO::PlaceOnTrackRequest, 128> mPlaceOnTrackRequestQueue;
// reconstructed by this type's own ledger TU (pulls in the real EventQueue generic
// + ResetOnTrackResult/PlaceOnTrackRequest element cascade -- NOT instantiated here).
//
// alignas(16): the DWARF shows the members are EventQueue instantiations.

#include "types.hpp"   // u8 (reserved blob)

namespace BrnAI
{
    namespace AIModuleIO
    {
        // DWARF: BrnAIModuleResultInterface.h:121 (struct BrnAI::AIModuleIO::AIModuleResultInterface).
        // Holds the reset-on-track result queue + place-on-track request queue
        // (both EventQueue<...,128> instantiations).
        struct alignas(16) AIModuleResultInterface
        {
            unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
        };
    }
}
