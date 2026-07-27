#pragma once

#include "types.hpp"
#include "GameSource/World/BrnWorldModuleIO.h"            // BrnWorldIO::UpdateInputBuffer
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"   // BrnAI::AIModuleIO::InputBuffer

// WorldModule game-input -> AI bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeInputToAI.h
//   (DWARF home: GameSource/Unity/../World/Bridges/WorldBridgeInputToAI.cpp -- the
//    assert file string baked into the body names that TU)
//
// Per-frame: stage the world module's Update input buffer into the AI module's
// input buffer, immediately before the AI update in WorldModule::Update
// @0x827D63E8. The leading lpWorldModule arg is the X360 r3 (the WorldModule
// `this`). Body boot-gated in WorldLinkStubs.cpp until the AI module IO is homed.
namespace WorldModule
{
    // @ 0x827AB738
    void BridgeInputToAIModule(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);
}
