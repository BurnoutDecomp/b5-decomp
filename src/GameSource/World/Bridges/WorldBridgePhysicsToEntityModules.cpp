// ============================================================================
// b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.cpp
//
// WorldModule physics -> entity-module post-physics bridges (X360 TU
// GameSource/Unity/../World/Bridges/WorldBridgePhysicsToEntityModules.cpp).
//
// RETIRED BODY NOTE (world-drive wave 2026-07-27)
// -----------------------------------------------
// This TU previously carried a single reconstruction of
// WorldModule::BridgePhysicsModuleToAIModule_PostPhysics (X360 0x827A5680) written
// against by-name STAND-IN buffer structs, because BrnAI::AIModuleIO::
// InputBuffer_PostPhysics / BrnPhysics::PhysicsModuleIO::OutputBuffer were not
// committed when it landed. The real IO homes exist now and the world-drive spine
// (WorldModule::Update @0x827D63E8 -> EntityModulePostPhysicsUpdate @0x827D3F10)
// calls all five of this TU's bridges with the REAL buffer types, so the header was
// retyped and the stand-in structs retired -- keeping the old body would have
// bound the drive call site to fabricated types.
//
// The X360 data flow it recorded is preserved verbatim for the reconstruction that
// replaces the boot gate (WorldLinkStubs.cpp):
//   BridgePhysicsModuleToAIModule_PostPhysics @0x827A5680
//     if (!aiInput)       assert "lpAIModuleInputBuffer_PostPhysics != NULL"  (:142)
//     if (!physicsOutput) assert "lpPhysicsModuleOutputBuffer != NULL"        (:143)
//     read-lock check on the physics output (bit 0x10, "Not locked for reading",
//       Physics/BrnPhysicsModuleIO.h:369)
//     *(aiInput + 4) = *(physicsOutput + 998192)   ; 0xF3CB0, the post-physics
//                                                  ; AI sub-interface's leading word
//   (the X360 `bl sub_8279F8E0` getter is the read-lock + that direct read; the PS3
//    DecFIGS body @0xA2B84C inlines it.)
//
// The four sibling bridges (@0x827AE9D0 race car, @0x827AB910 traffic,
// @0x827AB998 prop, @0x827AB8B0 crash) are declaration-only here for the same
// reason: their bodies walk physics-output sub-interfaces whose accessor band is
// not homed yet. This TU is therefore intentionally body-free; it stays in the tree
// as the declared home so the reconstruction lands here (and not in the link-stub
// TU) when the physics IO pass runs.
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"
