#pragma once

#include "types.hpp"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                         // PhysicsModuleIO::InputBuffer
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"        // CrashIO::OutputBuffer_PreScene

// WorldModule crash -> physics bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h
//
// Per-frame: append the crash module's staged vehicle-input events into the physics
// module's vehicle-input interface.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827AAC70; the module-IO buffer slices
// live at their own homes (see the includes). The leading lpWorldModule arg is the
// X360 r3 (the WorldModule context); the bridge never reads through it.
namespace WorldModule
{
    // @ 0x827AAC70
    void BridgeCrashModuleToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutput_PreScene);
}
