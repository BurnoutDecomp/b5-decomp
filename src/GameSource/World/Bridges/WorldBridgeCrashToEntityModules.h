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
// [crash exit 2026-08-25] the CrashModuleIO::OutputBuffer_PostScene forward declaration is
// GONE: the type was a phantom (see BrnCrashModule.h). The three post-scene bridges below
// take the crash module's ONE output buffer, CrashIO::OutputBuffer_PreScene -- the same type
// the physics bridge at the bottom of this header has always taken, itself corroboration.
namespace BrnWorld { namespace RaceCarEntityModuleIO { class InputBuffer_PostScene; }
                     namespace PropEntityIO { class InputBuffer_PostScene; } }
namespace BrnTraffic { namespace BrnTrafficIO { class InputBuffer_PostScene; } }

namespace WorldModule
{
    // @ 0x827AAC70
    // ---- The three post-scene crash bridges (callers in
    //      WorldModule::EntityModulePostSceneUpdate @0x827C3C58, which receives the crash
    //      OutputBuffer_PreScene in argument slot 38). Each is a straight two-call sequence
    //      handing one of that buffer's embedded interfaces to the target module's
    //      post-scene input: @0x827AD5A0 race car, @0x827AD5E0 traffic, @0x827AAD78 props. ----
    void BridgeCrashModuleToRaceCarModule_PostScene(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene* lpRaceCarInputBuffer_PostScene,
        const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer);

    void BridgeCrashModuleToTrafficModule_PostScene(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
        const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer);

    void BridgeCrashModuleToPropModule_PostScene(
        void* lpWorldModule,
        BrnWorld::PropEntityIO::InputBuffer_PostScene* lpPropInputBuffer_PostScene,
        const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer);

    void BridgeCrashModuleToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutput_PreScene);
}
