#include "GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // CgsModule::Event (the drain loop's cursor)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"  // Props::PropInputInterface (Append)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h" // BrnPlayerDriverControls (miVehicleID, the filter key)
#include "GameSource/World/BrnWorldModule.h"                                // BrnWorld::WorldModule (maeCarControls / mbDEBUGPlayerCarAlwaysUnderAIControl)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface

#include <cstddef>   // offsetof / size_t (the source-extent gate below)

// ============================================================================
// WorldModule entity-modules -> physics-module bridges (the PRE-SCENE leg).
//   X360 home: GameSource/Unity/../World/Bridges/WorldBridgeEntityModulesToPhysics.cpp
//   (the assert file string baked into the body names that TU).
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX 0x827AADB8 (33 instructions,
// no loops), cross-read against the DecFIGS PS3 body 0xA30188 (which inlines every accessor
// and so carries each one's lock bit + header line as a baked assert).
//
// ⭐ WHY THIS BRIDGE MATTERS OUT OF ALL PROPORTION TO ITS SIZE: it is the ONLY caller in the
// whole X360 image of PhysicsModuleIO::InputBuffer::SetSolverMaxIterations @0x8279F240
// (xrefs_to has exactly one entry). While it was an inert boot gate the physics input
// buffer's solver iteration cap stayed at the 0 that InputBuffer::Construct writes, and the
// entire MaxIterations chain downstream of PhysicsModule::Update asserted:
//   PhysicsSimulationIO::InputBuffer::SetMaxIterations  "luMaxIterations > 0"
//   PhysicsSimulationIO::InputBuffer::GetMaxIterations  "muMaxIterations > 0"
//   PhysicsSimulationModule::Update                     "lpInput->GetMaxIterations() > 0"
//
// The null tripwires are NON-gating (the X360 falls through after firing). The X360-baked
// d:\p4 file/line pairs are intentionally not reproduced -- CGS_ASSERT stamps __FILE__ /
// __LINE__; the console source line is noted in a trailing comment.
// ============================================================================

namespace WorldModule
{

// @ 0x827AADB8
void BridgeEntityModulesToPhysicsModule_PreScene(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
    const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutputBuffer_PreScene)
{
    // The X360 passes the WorldModule in r3 and never reads it (the PS3 build spells the same
    // seat as the implicit `this`); kept so the call site's shape matches the console's.
    (void)lpWorldModule;

    CGS_ASSERT(lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer != NULL");     // :42
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene != NULL"); // :43
    // AS SHIPPED: there is no lpPropOutputBuffer_PreScene tripwire -- the console asserts only
    // the first two arguments even though it dereferences all three.

    // ---- 1. the race car module's staged vehicle input -> the physics input buffer --------
    // 0x827AAE24  bl 0x8279D3B0  OutputBuffer_PreScene::GetVehicleInputInterface() const  (R, +0x10)
    // 0x827AAE30  bl 0x8279ED28  InputBuffer::GetVehicleInputInterface()                  (W, +0x170)
    // 0x827AAE38  bl 0x823C87C0  Vehicle::VehicleInputInterface::Append
    // Both seats hold the REAL BrnPhysics::Vehicle::VehicleInputInterface, so this is a
    // same-type merge on both ends -- no cast.
    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->Append(
        *lpRaceCarOutputBuffer_PreScene->GetVehicleInputInterface());

    // ---- 2. the prop module's staged prop requests -> the physics prop manager ------------
    // 0x827AAE40  bl 0x827A1970  PropEntityIO::OutputBuffer_PreScene::GetPropInputInterface() const (R, +819824)
    // 0x827AAE4C  bl 0x8279F2F8  InputBuffer::GetPropManagerInputInterface()                        (W)
    // 0x827AAE54  bl 0x827A9CA8  Props::PropInputInterface::Append
    // FLAG cross-home cast (source only): the prop entity module still exposes its pre-scene
    // interface as opaque *Storage in its own IO home. The DESTINATION is same-type as of the
    // 2026-08-10 retype of PhysicsModuleIO::InputBuffer::PropInputInterfaceStorage -- see the
    // note there; before it, this Append would have written ~12 KB through a 1-byte member.
    lpPhysicsModuleInputBuffer->GetPropManagerInputInterface()->Append(
        reinterpret_cast<const BrnPhysics::Props::PropInputInterface*>(
            lpPropOutputBuffer_PreScene->GetPropInputInterface()));

    // ---- 3. publish the active race car snapshot into the physics input --------------------
    // 0x827AAE5C  bl 0x8279D500  OutputBuffer_PreScene::GetActiveRaceCarOutputInterface() const (R, +960960)
    // 0x827AAE68  bl 0x8279EF20  InputBuffer::SetRCEntityOutputInterface  (W; PS3 shows a 10480-byte copy)
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* const
        lpActiveRaceCars = lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface();

    // FLAG cross-home cast + a GATE, not a hope: the physics input buffer models this seat as
    // RCEntityOutputInterfaceStorage, a size-pinned 10480-byte span (the X360 extent, which
    // the PS3 memcpy count confirms), and SetRCEntityOutputInterface copies exactly that many
    // bytes OUT of whatever it is handed. On the host the real interface is the LARGER object,
    // so the copy is an in-bounds partial -- console-extent-faithful, and the read cannot
    // leave the source. The static_assert is what proves it: if the host type ever shrinks
    // below the console span this stops compiling instead of reading past the end.
    // (Nothing on this build reads the destination span back -- the buffer exposes no getter
    //  for it -- so the truncation is inert today. Retype the seat when a consumer needs it.)
    static_assert(sizeof(BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface)
                      >= sizeof(BrnPhysics::PhysicsModuleIO::InputBuffer::RCEntityOutputInterfaceStorage),
                  "SetRCEntityOutputInterface would read past the end of the source interface");
    lpPhysicsModuleInputBuffer->SetRCEntityOutputInterface(
        reinterpret_cast<const BrnPhysics::PhysicsModuleIO::InputBuffer::RCEntityOutputInterfaceStorage*>(
            lpActiveRaceCars));

    // ---- 4. the solver iteration cap -------------------------------------------------------
    // 0x827AAE70  li   r31, 2
    // 0x827AAE74  stw  r31, luSolverMaxIterations
    // 0x827AAE78  bl   0x8279D500        (GetActiveRaceCarOutputInterface again)
    // 0x827AAE7C  lwz  r11, 0x2858(r3)   ; cmpwi -1 ; else lbz r11, 0x460*idx + 0x77A
    // 0x827AAEA8  stw  r31, luSolverMaxIterations      <-- the SAME r31, still 2
    // 0x827AAEB4  bl   0x8279F240        InputBuffer::SetSolverMaxIterations(&luSolverMaxIterations)
    //
    // ⭐ AS SHIPPED -- THE CONDITIONAL HAS NO EFFECT, AND TWO IMAGES SAY SO.
    // On the X360 the compiler loaded 2 into r31 once and used that one register for BOTH
    // stores, so the taken and not-taken paths write the identical value. The DecFIGS PS3
    // build @0xA30188 goes further and folds the whole test away: its entire tail is a single
    // `*(lpPhysicsModuleInputBuffer + 327024) = 2;`, with the predicate gone. The source's two
    // named constants evidently ended up equal in the shipped configuration.
    // The predicate is REPRODUCED rather than folded, because it is the only surviving record
    // of the author's intent and it is free; the value it selects is 2 either way. It is NOT
    // an invented number -- 2 is read out of two independent shipped images.
    //
    // The console's `+0x2858 / ==-1 ? 0 : byte[1120*idx + 1914]` is byte-identical to the
    // expression the car-select wave already recovered from BridgeWorldToDirector @0x823E3AB0
    // and committed as RCEntityActiveRaceCarOutputInterface::IsPlayerCarCrashing()
    // (BrnRaceCarEntityModuleOutputInterface.h:188) -- so it is reached BY NAME here, not by
    // poking a console byte offset that would not land on x64.
    u32 luSolverMaxIterations = 2;                                      // WorldBridge...ToPhysics.cpp:51
    if (lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface()->IsPlayerCarCrashing())
    {
        luSolverMaxIterations = 2;
    }
    lpPhysicsModuleInputBuffer->SetSolverMaxIterations(&luSolverMaxIterations);
}

// ============================================================================================
// @ 0x827AAEC0 -- THE PRE-PHYSICS LEG. Reconstructed call-for-call from the 271-instruction
// X360 body (no export hole; asm + pseudocode both read). ⭐ NEW 2026-08-10; the
// WorldLinkStubs boot gate is deleted in the same commit.
//
// ⭐⭐ WHY IT MATTERS: it is the ONLY thing in the image that moves a staged CreateRaceCarEvent
// from the race-car entity module into the physics module. The producer chain
//   PlaceOnTrackManager::PrePhysicsUpdate -> RaceCarEntityModule::ResetActiveRaceCar
//     -> ActiveRaceCar::AddHandlingModel -> VehicleInputInterface::CreateRaceCar
// stages into RaceCarEntityModuleIO::OutputBuffer_PrePhysics::mVehicleInputInterface, and the
// previous wave MEASURED the consequence of this bridge being inert: the census it added at
// VehicleManager::ProcessCreateEvents printed `CreateRaceCarEvent queue length = 0` at every
// drain of a 275 s run. Step 5 below is what fills it.
//
// FRAME ORDER (BrnWorldModule.cpp, one WorldModule::Update): lpPhysicsInput is Constructed at
// :2285, filled here at :2738, drained by mPhysicsModule.PostSceneUpdate at :2778 and destroyed
// at :3054 -- so every queue this function writes is re-Constructed next frame and an undrained
// event is dropped, not accumulated. There is no overflow path.
//
// ⚠️ THE r3 SEAT IS NOT DEAD HERE. Unlike the pre-scene sibling above, this bridge READS the
// WorldModule: the DecFIGS DWARF declares it as a MEMBER of BrnWorld::WorldModule
// (BrnWorldModule.h:578, four parameters), so the X360's r3 is the implicit `this`. The PC tree
// models the whole bridge layer as free functions taking that seat explicitly, so the two
// members are reached through the named accessors added to BrnWorld::WorldModule in the same
// commit (a friend declaration was tried FIRST and is unusable here: naming the global
// `WorldModule` namespace from BrnWorldModule.h makes `using BrnWorld::WorldModule;` at
// BrnGameModule.hpp:63 an ambiguous multiple declaration -- MEASURED, not assumed).
// DELETE-WHEN the bridge layer is re-homed as WorldModule members.
//
// The X360-baked d:\p4 file/line pairs are not reproduced (CGS_ASSERT stamps __FILE__/__LINE__);
// the console source line is noted in a trailing comment, as in the pre-scene body above.
// ============================================================================================

// @ 0x827AAEC0
void BridgeEntityModulesToPhysicsModule_PrePhysics(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutputBuffer_PrePhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutputBuffer_PrePhysics,
    const BrnWorld::PropEntityIO::OutputBuffer_PrePhysics* lpPropOutputBuffer_PrePhysics)
{
    CGS_ASSERT(lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer != NULL");            // :76
    CGS_ASSERT(lpTrafficOutputBuffer_PrePhysics != 0, "lpTrafficOutputBuffer_PrePhysics != NULL"); // :77
    CGS_ASSERT(lpRaceCarOutputBuffer_PrePhysics != 0, "lpRaceCarOutputBuffer_PrePhysics != NULL"); // :78
    CGS_ASSERT(lpPropOutputBuffer_PrePhysics != 0, "lpPropOutputBuffer_PrePhysics != NULL");       // :79

    const BrnWorld::WorldModule* const lpModule =
        static_cast<const BrnWorld::WorldModule*>(lpWorldModule);

    // ---- 1. the race car module's staged DRIVER-CONTROL events, filtered --------------------
    // asm 0x827AAF78..0x827AB03C. The console walks the source queue event by event
    // (GetFirstEvent/GetNextEvent return the event's TYPE, < 0 == end) and re-adds only the
    // events whose car is currently under player control.
    {
        const BrnPhysics::Vehicle::VehicleDriverInputInterface* const lpSourceDriver =
            lpRaceCarOutputBuffer_PrePhysics->GetVehicleDriverInterface();       // 0x8279E070
        BrnPhysics::Vehicle::VehicleDriverInputInterface* const lpDestDriver =
            lpPhysicsModuleInputBuffer->GetVehicleDriverInterface();             // 0x8279EDD0

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liType = lpSourceDriver->GetUpdateDriverQueue()->GetFirstEvent(&lpEvent, &liSize);
             liType >= 0;
             liType = lpSourceDriver->GetUpdateDriverQueue()->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            // 0x827AAFC4 `cmpwi r30, 1` -- E_DRIVER_TYPE_AI. AS SHIPPED this is a NON-GATING
            // tripwire: the console fires it and then processes the event anyway.
            CGS_ASSERT(liType != BrnPhysics::Vehicle::E_DRIVER_TYPE_AI,
                       "What the hell is the RCEM doing generating AI controls??");              // :94

            // 0x827AAFE8 `lwz r11, 0(r31)` -- the event's leading word. Every driver-controls
            // record derives from BrnPlayerDriverControls, whose first member is miVehicleID
            // (@0x00), and BrnTrafficDriverControls adds no members -- so the key is the same
            // word for all four E_DRIVER_TYPEs.
            const BrnPhysics::Vehicle::BrnPlayerDriverControls* const lpControls =
                static_cast<const BrnPhysics::Vehicle::BrnPlayerDriverControls*>(lpEvent);

            // 0x827AAFEC..0x827AB008: `maeCarControls[id] == 1 && !mbDEBUGPlayerCarAlwaysUnderAIControl`.
            // Both members reached BY NAME (the console's `4*(id + 1541820)` == &maeCarControls[id]
            // and its `lbzx` at +6167312 == mbDEBUGPlayerCarAlwaysUnderAIControl; those console
            // byte offsets are documentation here, not addressing).
            // FLAG: the DWARF types maeCarControls as `BrnWorld::CarControl[8]` and that enum's
            // home is not reconstructed, so the shipped literal is reproduced through a named
            // local. 1 is what WorldModule::Construct primes every slot to; the sibling bridge
            // WorldBridgeRaceCarToWorldModule.cpp already names the OTHER observed value
            // (2 == "rival"/under-AI, which HandleGameActions case 7 also writes).
            const s32 KI_CAR_CONTROL_PLAYER = 1;
            if (lpModule->GetCarControl(lpControls->miVehicleID) == KI_CAR_CONTROL_PLAYER &&
                !lpModule->IsDEBUGPlayerCarAlwaysUnderAIControl())
            {
                lpDestDriver->GetUpdateDriverQueue()->AddEvent(lpEvent, liType, liSize);
            }
        }
    }

    // ---- 2. the race car module's target-assist list -> the physics driver interface --------
    // asm 0x827AB040..0x827AB0C8 -- VehicleDriverInputInterface::CopyTargetAssistParams
    // (DecFIGS BrnVehicleDriverInputInterface.h:102) inlined; see that method's banner.
    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()->CopyTargetAssistParams(
        lpRaceCarOutputBuffer_PrePhysics->GetVehicleDriverInterface());

    // ---- 3. ...and its per-car base-deformation snapshot -------------------------------------
    // 0x827AB0E0  bl 0x8279C290  VehicleDriverInputInterface::CopyBaseDeformationParams
    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()->CopyBaseDeformationParams(
        lpRaceCarOutputBuffer_PrePhysics->GetVehicleDriverInterface());

    // ---- 4. ⭐⭐ THE CREATE QUEUE: race-car vehicle input -> physics vehicle input ------------
    // 0x827AB0E8  bl 0x8279DFC8  OutputBuffer_PrePhysics::GetVehicleInputInterface() const (+16)
    // 0x827AB0F4  bl 0x8279ED28  InputBuffer::GetVehicleInputInterface()                  (+368)
    // 0x827AB0FC  bl 0x823C87C0  Vehicle::VehicleInputInterface::Append
    // Both seats hold the REAL VehicleInputInterface, so this is a same-type merge -- and
    // mCreateRaceCarEventQueue is one of the fifteen queues Append carries.
    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->Append(
        *lpRaceCarOutputBuffer_PrePhysics->GetVehicleInputInterface());

    // ---- 5. the traffic module's vehicle input -> the same seat -------------------------------
    // 0x827AB104  bl 0x827A0378  BrnTrafficIO::OutputBuffer_PrePhysics::GetVehicleInputInterface() const
    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->Append(
        *lpTrafficOutputBuffer_PrePhysics->GetVehicleInputInterface());

    // ---- 6. both modules' effects (air-ram + spin) -> the physics effects interface ----------
    // 0x827AB138/0x827AB144 and 0x827AB164/0x827AB170 -- VehicleEffectsInputInterface::Append
    // (DWARF :88) inlined as its two queue merges, once per source.
    lpPhysicsModuleInputBuffer->GetVehicleEffectsInputInterface()->Append(
        lpRaceCarOutputBuffer_PrePhysics->GetVehicleEffectsInterface());
    lpPhysicsModuleInputBuffer->GetVehicleEffectsInputInterface()->Append(
        lpTrafficOutputBuffer_PrePhysics->GetVehicleEffectsInterface());

    // ---- 7. the prop module's staged prop requests -> the physics prop manager ----------------
    // 0x827AB188..0x827AB1E0 -- Props::PropInputInterface::Append @0x827A9CA8 inlined (the
    // handle copy at +0x2BF8, the four queue Appends, and the OR-merge of the flag at +0x2C00).
    // Both seats are the real PropInputInterface as of the 2026-08-10 retypes, so no cast.
    lpPhysicsModuleInputBuffer->GetPropManagerInputInterface()->Append(
        lpPropOutputBuffer_PrePhysics->GetPropInputInterface());

    // ---- 8. the traffic module's driver interface -> the physics driver interface -------------
    // 0x827AB1FC..0x827AB29C -- VehicleDriverInputInterface::Append @0x823DB640 inlined (the
    // <5040,16> queue merge, the "at most one side holds a target-assist list" tripwire, and the
    // zero-count adoption arm).
    lpPhysicsModuleInputBuffer->GetVehicleDriverInterface()->Append(
        lpTrafficOutputBuffer_PrePhysics->GetVehicleDriverInterface());

    // ---- 9. the added-for-collision bitset --------------------------------------------------
    // 0x827AB2A0..0x827AB2F0 -- one 8-byte BitArray<8> copy through the DWARF-declared
    // Get/SetRaceCarsAddedForCollision pair (:245/:252); the setter carries the
    // "lpRaceCarsAddedForCollision != NULL" tripwire the console bakes at :254.
    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->SetRaceCarsAddedForCollision(
        lpRaceCarOutputBuffer_PrePhysics->GetVehicleInputInterface()->GetRaceCarsAddedForCollision());
}

}   // namespace WorldModule
