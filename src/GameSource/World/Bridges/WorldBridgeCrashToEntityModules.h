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
namespace BrnWorld { namespace CrashModuleIO { class OutputBuffer_PostScene; }
                     namespace RaceCarEntityModuleIO { class InputBuffer_PostScene; }
                     namespace PropEntityIO { class InputBuffer_PostScene; } }
namespace BrnTraffic { namespace BrnTrafficIO { class InputBuffer_PostScene; } }

namespace WorldModule
{
    // @ 0x827AAC70
    // ---- ADDITIVE DECLS (callers in WorldModule::EntityModulePostSceneUpdate
    //      @0x827C3C58; ledger-'reviewed' PHANTOMS -- reconstruct from
    //      @0x827AD5A0 / @0x827AD5E0 / @0x827AAD78: each appends the crash module's
    //      post-scene event queues into the target module's post-scene input. ----
    void BridgeCrashModuleToRaceCarModule_PostScene(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene* lpRaceCarInputBuffer_PostScene,
        const BrnWorld::CrashModuleIO::OutputBuffer_PostScene* lpCrashOutputBuffer_PostScene);

    void BridgeCrashModuleToTrafficModule_PostScene(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
        const BrnWorld::CrashModuleIO::OutputBuffer_PostScene* lpCrashOutputBuffer_PostScene);

    void BridgeCrashModuleToPropModule_PostScene(
        void* lpWorldModule,
        BrnWorld::PropEntityIO::InputBuffer_PostScene* lpPropInputBuffer_PostScene,
        const BrnWorld::CrashModuleIO::OutputBuffer_PostScene* lpCrashOutputBuffer_PostScene);

    void BridgeCrashModuleToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutput_PreScene);
}
