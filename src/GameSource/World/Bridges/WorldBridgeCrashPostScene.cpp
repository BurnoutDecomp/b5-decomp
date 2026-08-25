// =================================================================================================
// GameSource/World/Bridges/WorldBridgeCrashPostScene.cpp
//
// The two post-scene crash bridges the crash module drives into the race-car and traffic entity
// modules:
//   WorldModule::BridgeCrashModuleToRaceCarModule_PostScene  @0x827AD5A0  (15 insns)
//   WorldModule::BridgeCrashModuleToTrafficModule_PostScene  @0x827AD5E0  (15 insns)
//
// Their DWARF home is WorldBridgeCrashToEntityModules.cpp, which still carries other
// unreconstructed crash bridges; this file follows the FILE-SPLIT precedent already set by
// WorldBridgePropModule.cpp (see that file's banner) so these two can land without waiting on
// the rest of the home TU. Their declarations live in WorldBridgeCrashToEntityModules.h --
// nothing here re-declares anything.
// DELETE-WHEN: the home TU becomes mountable whole; move both bodies back then.
//
// =================================================================================================
// ⭐⭐ WHY THESE TWO WERE BOOT GATES FOR A MONTH, AND WHAT WAS ACTUALLY WRONG
// =================================================================================================
// Both were declared against `BrnWorld::CrashModuleIO::OutputBuffer_PostScene`, a type modelled in
// BrnCrashModule.h as `struct { u8 maDeferredPayload[16]; }`. That placeholder was recorded across
// the campaign as "the single structural blocker for the whole crash-exit path" -- the crash
// module's RaceCarCrashCompleteEvent ring "could not be reached by name".
//
// THE TYPE DOES NOT EXIST. There was never a layout to recover. The crash module is a
// CgsModule::ModuleSingleBuffered: it has ONE output buffer, written pre-scene and read
// post-scene. "PostScene" in these bridges' mangled names is the BRIDGE's phase, not a buffer
// type, and the real parameter is BrnWorld::CrashIO::OutputBuffer_PreScene -- which this tree has
// modelled all along, with the very accessors these bodies call.
//
// PROVEN AT THE CALLER (WorldModule::Update @0x827D63E8), not inferred from the symbol names:
//   * It creates exactly FOUR crash IO buffers: CrashIO::{InputBuffer_PreScene,
//     OutputBuffer_PreScene, InputBuffer_PostPhysics, OutputBuffer_PostPhysics}. There is no
//     CreateIOBuffer<...CrashModuleIO::OutputBuffer_PostScene> anywhere in the image.
//     ⭐ The control that makes this conclusive rather than merely suggestive: in the SAME
//     function, EntityModulePostSceneUpdate really does CreateIOBuffer/DestroyIOBuffer a
//     BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene locally. When the console genuinely
//     has a distinct post-scene output buffer, it allocates one. The crash side never does.
//   * CrashModule::PreSceneUpdate is called with `v202` -- the OutputBuffer_PreScene -- as its
//     OUTPUT buffer, and EntityModulePostSceneUpdate is then called with that same `v202` in
//     ARGUMENT SLOT 38, which is precisely the argument these two bridges (and the prop one)
//     receive. One buffer, one producer, three post-scene consumers.
//   * The call targets agree by ADDRESS: 0x827AD5A0 calls 0x827A2530 ==
//     CrashIO::OutputBuffer_PreScene::GetRaceCarOutputInterface (the +0x231D0 read-lock getter),
//     and 0x827AD5E0 calls 0x827A23E0 == ::GetTrafficOutputInterface (+0x8).
//   * In-tree corroboration: WorldBridgeCrashToEntityModules.h's fourth bridge,
//     BridgeCrashModuleToPhysicsModule @0x827AAC70, has ALWAYS been declared against
//     CrashIO::OutputBuffer_PreScene. It reads the same buffer these do.
//
// ⛔ THE PREVIOUS WAVE CONSIDERED THIS AND RULED IT OUT -- read why it was wrong before undoing
// any of it. WorldBridgePropModule.cpp's banner argued: "IDA names 0x827A2530
// OutputBuffer_PreScene::GetRa..., but the call site's argument is the crash POST-SCENE output
// buffer. Both buffers place a race-car output interface at +0x231D0 behind an identical
// read-lock getter, so the two bodies are byte-identical and ICF folded them; IDA kept one of the
// two names." It is a careful theory and it is refuted by the caller: there is no second buffer,
// so there is no second getter for ICF to fold. The same banner's companion measurement --
// "CrashModuleIO::OutputBuffer_PostScene 16 [bytes], racecar iface @143824 OUT OF BOUNDS" --
// was measuring the phantom's sizeof, and 143824 == 0x231D0 is exactly OutputBuffer_PreScene's
// own attested member offset. The evidence for the identity was already in the tree, read as
// evidence against it.
//
// ---- LOCKING ----------------------------------------------------------------------------------
// Both getters used here are the const (READ-lock) overloads, because the caller read-locks the
// source buffer and write-locks the destination -- the same pairing every other bridge in this
// family uses, and the lock-bit tripwire inside each getter is what checks it.
//
// ---- NO ASSERTS -------------------------------------------------------------------------------
// ⚠️ Neither console body has a null guard. Both are a straight two-call sequence over 15
// instructions; r3 (the WorldModule `this`) is overwritten by the first `mr` and never read. Do
// not add the guards some sibling bridges carry -- these two genuinely have none.
// =================================================================================================

#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

namespace WorldModule
{

// =================================================================================================
// WorldModule::BridgeCrashModuleToRaceCarModule_PostScene  @ 0x827AD5A0  (15 insns)
//
// Hand the crash module's race-car output interface -- which carries the
// EventQueue<RaceCarCrashCompleteEvent,10>, i.e. "this race car has finished crashing" -- to the
// race-car entity module's post-scene input, so its ProcessRaceCarCrashCompleteEvents can reset
// the wrecked car.
//
//   0x827AD5B0  mr   r3, r5                 -- r5 = the crash output buffer (src)
//   0x827AD5B4  mr   r31, r4                -- r4 = the race-car post-scene input (dest)
//   0x827AD5B8  bl   0x827A2530             -- CrashIO::OutputBuffer_PreScene::
//                                              GetRaceCarOutputInterface() const (read-lock,
//                                              returns this + 0x231D0)
//   0x827AD5BC  mr   r4, r3
//   0x827AD5C0  mr   r3, r31
//   0x827AD5C4  bl   0x827ACA40             -- RaceCarEntityModuleIO::InputBuffer_PostScene::
//                                              SetCrashInterface(const CrashInterface*)
// The destination's `CrashInterface` typedef IS BrnWorld::CrashIO::RaceCarOutputInterface
// (BrnRaceCarEntityModuleIO.h:474), so the two sides name the same type and no cast is involved.
// =================================================================================================
void BridgeCrashModuleToRaceCarModule_PostScene(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene* lpRaceCarInputBuffer_PostScene,
    const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827AD5B0, never read

    lpRaceCarInputBuffer_PostScene->SetCrashInterface(
        lpCrashOutputBuffer->GetRaceCarOutputInterface());
}

// =================================================================================================
// WorldModule::BridgeCrashModuleToTrafficModule_PostScene  @ 0x827AD5E0  (15 insns)
//
// The traffic twin: hand the crash module's traffic output interface -- the cleanup-traffic and
// start-crashing-network-traffic rings -- to the traffic entity module's post-scene input, so the
// traffic system retires the vehicles this frame's crashes destroyed.
//
//   0x827AD5F0  mr   r3, r5
//   0x827AD5F4  mr   r31, r4
//   0x827AD5F8  bl   0x827A23E0             -- CrashIO::OutputBuffer_PreScene::
//                                              GetTrafficOutputInterface() const (read-lock, +0x8)
//   0x827AD5FC  mr   r4, r3
//   0x827AD600  mr   r3, r31
//   0x827AD604  bl   0x827ACDE8             -- BrnTrafficIO::InputBuffer_PostScene::
//                                              SetCrashTrafficOutputInterface(...)
// =================================================================================================
void BridgeCrashModuleToTrafficModule_PostScene(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
    const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827AD5F0, never read

    lpTrafficInputBuffer_PostScene->SetCrashTrafficOutputInterface(
        lpCrashOutputBuffer->GetTrafficOutputInterface());
}

}   // namespace WorldModule
