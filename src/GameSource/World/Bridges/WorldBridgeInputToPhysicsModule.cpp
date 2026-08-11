#include "GameSource/World/Bridges/WorldBridgeInputToEntityModules.h"

#include "GameSource/World/BrnWorldModule.h"                                          // BrnWorld::WorldModule (GetLastCameraInput)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h" // Vehicle::VehicleDriverInputInterface::Append
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"               // CgsSystem::TimerStatusInterface

// ============================================================================
// GameSource/World/Bridges/WorldBridgeInputToPhysicsModule.cpp
//
// ⭐⭐ WorldModule::BridgeInputToPhysicsModule -- X360 @ 0x827AB830.
//
// THE ADDRESS HAD TO BE RECOVERED FROM THE IMAGE. This function is a HOLE in the IDA export
// set: it appears in NEITHER progress/identity.json NOR any of the 30,084 per-function JSONs
// under .ida-exports/BURNOUT_X360_ARTIST.XEX/, even though IDA prints its NAME at the call
// site. It was located by decoding the `bl` word at the call site VA 0x827D6A74 out of the
// X360 image (0x4BFD4DBD -> op18, AA0, LK1, displacement -0x2B244 -> 0x827AB830) and proving
// the decoder on the very next bridge call at 0x827D6AD8, which resolves to the known
// BridgeInputToCrashModule @0x827ADEE8. Its extent is fixed on both sides by the neighbouring
// exports (BridgeInputToAIModule @0x827AB738 ends at 0x827AB82C;
// BridgePhysicsModuleToCrashModule_PostPhysics starts at 0x827AB8B0) -- exactly 128 bytes,
// 32 instructions, disassembled with capstone against the image bytes.
//
// ⭐ FILE SPLIT, same pattern and same reason as the sibling WorldBridgeRaceCarToWorldModule.cpp
// (car-select hand-off wave): the DWARF home is the WorldBridgeInputToEntityModules.cpp unity
// TU, but THAT TU IS NOT MOUNTED (its BridgeInputToEntityModules leg still needs entity-module
// IO accessors that are declaration-only), so the copy that linked was the inert one-shot log
// in WorldLinkStubs.cpp. This bridge is fully closed on its own, so it gets its own TU.
// DELETE-WHEN: WorldBridgeInputToEntityModules.cpp can be mounted whole -- then fold this back.
//
// ⛔ WHAT THIS BRIDGE IS: the physics module's per-frame INPUT FEED. WorldModule::Update
// @0x827D63E8 write-locks the physics input buffer and calls it once per frame; until now the
// buffer was never written, so PhysicsModule::Update ran against a Construct-cleared input.
//
// The whole console body is FOUR statements -- no branches, no loops, no asserts of its own
// (every assert on this path lives inside the locked accessors it calls). Register trace:
//   r29 = lpWorldModule (r3)   r31 = lpPhysicsModuleInputBuffer (r4)   r30 = lpWorldInput (r5)
//   0x827AB84C  r3=r30 -> UpdateInputBuffer::GetVehicleDriverInputInterface  @0x827A3660 -> r28
//   0x827AB858  r3=r31 -> InputBuffer::GetVehicleDriverInterface [mutable]   @0x8279EDD0
//   0x827AB860  r4=r28 -> Vehicle::VehicleDriverInputInterface::Append       @0x823DB640
//   0x827AB868  r3=r30 -> UpdateInputBuffer::GetVehicleInputInterface        @0x827A35B8 -> r28
//   0x827AB874  r3=r31 -> InputBuffer::GetVehicleInputInterface [mutable]    @0x8279ED28
//   0x827AB87C  r4=r28 -> Vehicle::VehicleInputInterface::Append             @0x823C87C0
//   0x827AB884  r3=r30 -> UpdateInputBuffer::GetTimerStatusInterface         @0x827A37B0 -> r4
//   0x827AB890  r3=r31 -> InputBuffer::SetTimerInterface                     @0x8279F128
//   0x827AB894  r4 = r29 + 0x5E1CC0 (== WorldModule::mLastCameraInput)
//   0x827AB8A0  r3=r31 -> InputBuffer::SetCameraInput                        @0x827A9D30
// The console evaluates each Append's ARGUMENT (the world-side getter) before its OBJECT (the
// physics-side getter); that ordering is codegen, not semantics, and the two getters are
// side-effect-free apart from their own lock asserts.
// ============================================================================

namespace WorldModule
{

// @ 0x827AB830 -- stage the world module's physics-facing input: merge the frame's
// vehicle-driver and vehicle request/event interfaces into the physics input buffer, copy the
// timer status across, and publish the module's last director camera.
void BridgeInputToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()->Append(
        lpWorldInput->GetVehicleDriverInputInterface());

    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->Append(
        *lpWorldInput->GetVehicleInputInterface());

    // FLAG cross-home cast: BrnWorldIO models the world buffer's timer block as its own
    // 48-byte pointer-free POD while the physics buffer typedefs the canonical
    // CgsSystem::TimerStatusInterface. Both model the SAME X360 member (the console copies a
    // flat 0x30 block between them), and the two sizes are pinned equal immediately below, so
    // the cast can never become a partial copy. Retire it when BrnWorldIO adopts the canonical
    // type -- blocked today only because CgsSystem::TimerStatusInterface::operator= is
    // declaration-only and UpdateInputBuffer::SetTimerStatusInterface assigns through it.
    static_assert(sizeof(BrnWorldIO::TimerStatusInterface)
                      == sizeof(CgsSystem::TimerStatusInterface),
                  "world/physics timer-status blocks must be the same 48-byte X360 member");
    lpPhysicsModuleInputBuffer->SetTimerInterface(
        reinterpret_cast<const CgsSystem::TimerStatusInterface*>(
            lpWorldInput->GetTimerStatusInterface()));

    // The console's `this + 0x5E1CC0`, by name. SetCameraInput copies the 0x160-byte camera
    // extent, which is the head of the 0x170-byte camera type -- the console does the same.
    const BrnWorld::WorldModule* lpModule = static_cast<const BrnWorld::WorldModule*>(lpWorldModule);
    lpPhysicsModuleInputBuffer->SetCameraInput(
        reinterpret_cast<const BrnPhysics::PhysicsModuleIO::InputBuffer::CameraStorage*>(
            lpModule->GetLastCameraInput()));
}

}
