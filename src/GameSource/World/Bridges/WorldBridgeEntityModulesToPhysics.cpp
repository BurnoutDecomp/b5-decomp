#include "GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"  // Props::PropInputInterface (Append)
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

}   // namespace WorldModule
