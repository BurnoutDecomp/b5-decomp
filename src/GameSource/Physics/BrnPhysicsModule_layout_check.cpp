// Layout gate for BrnPhysics::PhysicsModule (X360 sizeof 433,209 raw; seven embedded
// sub-objects + a 136-byte trailing state/perf-monitor block).
//
// WHY THIS TU EXISTS -- READ BEFORE DELETING IT.
// BrnPhysicsModule.h was re-seated on 2026-08-03 (task #123) from an opaque
// `u8 maTailPlaceholder[...]` + a fabricated `ContainedListInterface` into seven real typed
// members plus the DWARF's trailing block. The whole claim that this is a DERIVATION rather
// than thirty-five separate guesses is that TWO SOURCES THAT DO NOT KNOW ABOUT EACH OTHER meet
// with zero slack:
//   * the X360 asm's seven sub-object base offsets and its stwx/stbx/stdx tail literals, and
//   * each sub-object class's OWN console size, taken from that class's own header (which was
//     derived by a different wave from a different function).
// A claim like that is worth exactly as much as the gate that checks it.
//
// THIS TU MUST STAY MOUNTED IN tools/build/build_game_exe.bat.
// BrnVehicleManager.h learned this the hard way: its `_AssertLayout` / `_AssertLayoutPlayerStats`
// live in UNMOUNTED TUs, so for every wave that touched them a static_assert was a comment and a
// green build said nothing about the layout. Nothing is emitted by this file at link time --
// static_assert fires at compile time, so /OPT:REF discarding _AssertLayout afterwards is
// irrelevant -- but it has to be COMPILED.
//
// WHY PART 1 IS ARITHMETIC AND PART 2 IS RELATIVE-offsetof, AND WHY THAT SPLIT IS THE POINT.
// The seven sub-objects are all reconstructions whose HOST size differs from the console's --
// measured, not assumed: SimulationModule +1072, VehicleManager -5600, ContactSpyData +/-0,
// DeformationManager +800, DeformationInput +288, DeformationOutput +16, PropManager +64. An
// absolute (or even relative) host `offsetof` over that region would measure host widths, not the
// recovered console layout, so it would be false or vacuous. PART 1 therefore never touches a
// host type at all.
// The TAIL is the opposite case and gets the stronger treatment: two enums, a u64 handle, a
// 4-byte EntityId, an enum, twenty-eight int32s and a bool -- no pointer and no vptr anywhere in
// it -- so it is WIDTH-INVARIANT and its host spacing must equal its console spacing exactly.
// PART 2 pins all thirty-three tail members to their X360 seats relative to mePrepareStage.
//
// THE BLIND SPOTS, stated rather than hidden.
//  (a) PART 1 cannot see inside a sub-object; it checks that each one OCCUPIES its proven span.
//      What guards the interiors is each class's own gate/header map.
//  (b) PART 1's seven KU_X360_SIZEOF_* constants are DEFINITIONS quoted from the sub-classes'
//      headers, not derivations of this wave. If one of those headers is wrong, the walk fires
//      here -- which is the desired signal, and is what makes this a check and not a tautology.
//  (c) A member occupying no space (a trailing bool absorbed by alignment padding) is invisible
//      to arithmetic. That is what the named-member/type checks in PART 3 are for.
//
// TAMPER-TESTED 2026-08-03 -- see the log at the bottom of this file.

#include "GameSource/Physics/BrnPhysicsModule.h"

#include <cstddef>       // offsetof
#include <type_traits>   // is_same

namespace BrnPhysics
{
    // ==============================================================================================
    // PART 1 -- CONSOLE ARITHMETIC. Pure integer maths over the X360 literals; it does not touch a
    // host type anywhere, so it measures the RECOVERED LAYOUT and nothing else.
    //
    // Left-hand sides walk the DWARF declaration order using each sub-object's own console size.
    // Every right-hand side is an X360 instruction immediate from PhysicsModule's own asm.
    // ==============================================================================================
    namespace PhysicsModuleLayoutCheck
    {
        using namespace PhysicsModuleX360Layout;

        // ---------- the seven sub-objects, walked base-to-base ----------
        static_assert(KU_OFF_SIMULATIONMODULE + KU_X360_SIZEOF_SIMULATIONMODULE == KU_OFF_VEHICLEMANAGER,
                      "PhysicsModule: mSimulationModule (18544, CgsPhysicsSimulationModule.h's own "
                      "mpSimulation@0x4864 closure) must land mVehicleManager on the asm-literal "
                      "0x4AA0 (ctor/Construct/Destruct `addi r3,r31,0x4AA0`)");

        static_assert(KU_OFF_VEHICLEMANAGER + KU_X360_SIZEOF_VEHICLEMANAGER == KU_OFF_CONTACTDATA,
                      "PhysicsModule: mVehicleManager (172624, BrnVehicleManager.h's own map ending "
                      "at +172616) must land mContactData on the asm-literal 191728 "
                      "(`addis r3,r31,3 ; addi r3,r3,-0x1310`)");

        static_assert(KU_OFF_CONTACTDATA + KU_X360_SIZEOF_CONTACTSPYDATA == KU_OFF_DEFORMATIONMANAGER,
                      "PhysicsModule: mContactData (0x1DEB0, stated outright by BrnContactSpyData.h) "
                      "must land mDeformationManager on the asm-literal 314272 "
                      "(`addis r3,r31,5 ; addi r3,r3,-0x3460`)");

        static_assert(KU_OFF_DEFORMATIONMANAGER + KU_X360_SIZEOF_DEFORMATIONMANAGER == KU_OFF_DEFORMATIONINPUT,
                      "PhysicsModule: mDeformationManager (76752, its own perf-mon run ending at "
                      "+76736) must land mDeformationInput on the asm-literal 391024 "
                      "(`addis r3,r31,6 ; addi r3,r3,-0x890`)");

        static_assert(KU_OFF_DEFORMATIONINPUT + KU_X360_SIZEOF_DEFORMATIONINPUT == KU_OFF_DEFORMATIONOUTPUT,
                      "PhysicsModule: mDeformationInput (5072, its own mbResetPlayerScratches@+5056) "
                      "must land mDeformationOutput on the asm-literal 396096 "
                      "(`addis r3,r31,6 ; addi r3,r3,0xB40`)");

        static_assert(KU_OFF_DEFORMATIONOUTPUT + KU_X360_SIZEOF_DEFORMATIONOUTPUT == KU_OFF_PROPMANAGER,
                      "PhysicsModule: mDeformationOutput (10992, its own maLocatorData[28]@0x2A04) "
                      "must land mPropManager on the asm-literal 407088 -- the seat the retired "
                      "`ContainedListInterface mContainedList` was wrongly occupying");

        static_assert(KU_OFF_PROPMANAGER + KU_X360_SIZEOF_PROPMANAGER == KU_OFF_PREPARESTAGE,
                      "PhysicsModule: mPropManager (25984, its own miNumPropsAddedToContactGen@0x6570) "
                      "must land mePrepareStage on the asm-literal 433072 (`stwx 0, 0x69BB0`)");

        // ---------- the trailing state block, walked member-to-member ----------
        static_assert(KU_OFF_PREPARESTAGE + 4u == KU_OFF_RELEASESTAGE,
                      "PhysicsModule: meReleaseStage on the asm-literal 0x69BB4 (`stwx 7`)");

        // 433080 is 8-aligned, which is what lets the u64 handle sit here with no padding.
        static_assert(KU_OFF_RELEASESTAGE + 4u == KU_OFF_WORLDRIGIDBODYID, "PhysicsModule: mWorldRigidBodyId at 433080");
        static_assert(KU_OFF_WORLDRIGIDBODYID % 8u == 0u, "PhysicsModule: mWorldRigidBodyId's seat must be 8-aligned");

        // THE LOAD-BEARING ONE. If RigidBodyId were 4 bytes the whole tail would shift and every
        // assert below would move. The 8 comes from PrepareWorldRigidBody's `stdx r30,r29,r4`
        // (r4 == 0x69BB8) -- a store-DOUBLEWORD -- corroborated by CgsRigidBody.h deriving
        // CgsPhysics::RigidBodyId as a single u64 from a different function in a different class.
        static_assert(KU_OFF_WORLDRIGIDBODYID + 8u == KU_OFF_WORLDENTITYID,
                      "PhysicsModule: mWorldRigidBodyId is EIGHT bytes (stdx), so mWorldEntityId "
                      "lands on the asm-literal 0x69BC0");

        static_assert(KU_OFF_WORLDENTITYID + 4u == KU_OFF_CURRENTGAMEMODE,
                      "PhysicsModule: meCurrentGameMode on the asm-literal 0x69BC4 (`stwx -1` == E_MODE_NONE)");

        static_assert(KU_OFF_CURRENTGAMEMODE + 4u == KU_OFF_FIRSTPERFMON,
                      "PhysicsModule: miPhysicsPreSceneUpdatePM on the asm-literal 0x69BC8");

        // THE SECOND CLOSURE. KU_NUM_PERFMON_IDS is counted from the DWARF declaration list
        // (BrnPhysicsModule.h:211..:237), NOT from these two offsets -- and 27 four-byte ids fill
        // the asm-literal span 0x69BC8..0x69C30 with nothing left over. The brief this wave was
        // given said "twenty-one"; 21 leaves the block 24 bytes short and this assert is what says so.
        static_assert(KU_OFF_FIRSTPERFMON + (KU_NUM_PERFMON_IDS - 1u) * 4u == KU_OFF_LASTPERFMON,
                      "PhysicsModule: the DWARF's 27 int32 perf-monitor ids must exactly span the "
                      "asm literals 0x69BC8 (miPhysicsPreSceneUpdatePM) .. 0x69C30 "
                      "(miPhysicsUpdateFixUpVehContactsPM)");

        static_assert(KU_OFF_LASTPERFMON + 4u == KU_OFF_FRAMESTOFORCESLOWMO,
                      "PhysicsModule: miFramesToForceSuperSlowMotion on the asm-literal 0x69C34");
        static_assert(KU_OFF_FRAMESTOFORCESLOWMO + 4u == KU_OFF_ISONLINEGAMEMODE,
                      "PhysicsModule: mbIsOnlineGameMode on the asm-literal 0x69C38 (`stbx`, one byte)");
    }

// ==================================================================================================
// PART 2/3 -- HOST checks. offsetof on the private members needs member-function context, which is
// what PhysicsModule::_AssertLayout exists for. Never called.
// ==================================================================================================

// Pin one tail member to its X360 seat RELATIVE to mePrepareStage, and pin its type. The relative
// form is what makes it valid: the absolute host offset of the block depends on seven sub-object
// host sizes, but the SPACING inside the block is width-invariant (no pointer, no vptr), so it must
// reproduce the console deltas exactly.
#define BRNPHYS_TAIL_SEAT(member, type, consoleOffset)                                              \
    static_assert(std::is_same<decltype(PhysicsModule::member), type>::value,                       \
                  #member " must stay a " #type " -- a same-width retype (s32->u32, s32->f32) "     \
                  "would slip past every offset assert in this file");                              \
    static_assert(offsetof(PhysicsModule, member) - offsetof(PhysicsModule, mePrepareStage)         \
                      == (consoleOffset) - PhysicsModuleX360Layout::KU_OFF_PREPARESTAGE,            \
                  #member " must sit on its X360 seat " #consoleOffset " relative to mePrepareStage")

    void PhysicsModule::_AssertLayout()
    {
        using namespace PhysicsModuleX360Layout;

        // ------------------------------------------------------------------------------------
        // PART 2 -- the seven sub-objects are REAL TYPED MEMBERS, in DWARF declaration order.
        //
        // THIS IS THE ANTI-SLICING CHECK and it is the reason this file exists at all. The
        // failure this wave was called in to prevent is a sub-Construct being handed a raw offset
        // into a `u8[]` -- the ArticulatedJointPool sliced-call-site shape. If any of these seven
        // members is ever demoted back to a padding blob, or retyped to a base/other class, the
        // corresponding assert fires before a single byte is written.
        // ------------------------------------------------------------------------------------
        static_assert(std::is_same<decltype(PhysicsModule::mSimulationModule),
                                   CgsPhysics::PhysicsSimulationModule>::value,
                      "mSimulationModule must be the real PhysicsSimulationModule (its Construct is "
                      "dispatched through this member's own vtable slot 0)");
        static_assert(std::is_same<decltype(PhysicsModule::mVehicleManager),
                                   BrnPhysics::Vehicle::VehicleManager>::value,
                      "mVehicleManager must be the real VehicleManager");
        static_assert(std::is_same<decltype(PhysicsModule::mContactData),
                                   ContactSpy::ContactSpyData>::value,
                      "mContactData must be the real ContactSpyData");
        static_assert(std::is_same<decltype(PhysicsModule::mDeformationManager),
                                   Deformation::DeformationManager>::value,
                      "mDeformationManager must be the real DeformationManager");
        static_assert(std::is_same<decltype(PhysicsModule::mDeformationInput),
                                   Deformation::DeformationInputInterface>::value,
                      "mDeformationInput must be the real DeformationInputInterface");
        static_assert(std::is_same<decltype(PhysicsModule::mDeformationOutput),
                                   Deformation::DeformationOutputInterface>::value,
                      "mDeformationOutput must be the real DeformationOutputInterface");
        static_assert(std::is_same<decltype(PhysicsModule::mPropManager),
                                   Props::PropManager>::value,
                      "mPropManager must be the real PropManager -- this is the seat the retired "
                      "112-byte ContainedListInterface was sitting on");

        // Declaration order on the host must be the DWARF order, which is ascending console order.
        static_assert(offsetof(PhysicsModule, mSimulationModule)   < offsetof(PhysicsModule, mVehicleManager),      "order :189 < :190");
        static_assert(offsetof(PhysicsModule, mVehicleManager)     < offsetof(PhysicsModule, mContactData),         "order :190 < :192");
        static_assert(offsetof(PhysicsModule, mContactData)        < offsetof(PhysicsModule, mDeformationManager),  "order :192 < :194");
        static_assert(offsetof(PhysicsModule, mDeformationManager) < offsetof(PhysicsModule, mDeformationInput),    "order :194 < :195");
        static_assert(offsetof(PhysicsModule, mDeformationInput)   < offsetof(PhysicsModule, mDeformationOutput),   "order :195 < :196");
        static_assert(offsetof(PhysicsModule, mDeformationOutput)  < offsetof(PhysicsModule, mPropManager),         "order :196 < :198");
        static_assert(offsetof(PhysicsModule, mPropManager)        < offsetof(PhysicsModule, mePrepareStage),       "order :198 < :200");

        // ADJACENCY. Each declared member must be followed IMMEDIATELY by the next declared one,
        // with nothing but alignment padding in between. Without these, an EIGHTH sub-object could
        // be inserted anywhere in the chain and every other assert in this file would still pass:
        // PART 1 never touches a host type, and PART 3's seats are all relative to mePrepareStage,
        // so both are blind to an insertion. (Found by tamper-testing this very file -- the first
        // version of the "reorder" case inserted a member instead of swapping and came back SILENT.)
        // 16 is the largest alignment any of these types carries.
#define BRNPHYS_ADJACENT(prev, prevType, next)                                                      \
    static_assert(offsetof(PhysicsModule, next) >= offsetof(PhysicsModule, prev) + sizeof(prevType),\
                  #prev " must occupy its own span before " #next);                                 \
    static_assert(offsetof(PhysicsModule, next) - offsetof(PhysicsModule, prev)                     \
                      < sizeof(prevType) + 16u,                                                     \
                  "nothing may be inserted between " #prev " and " #next)

        BRNPHYS_ADJACENT(mSimulationModule,   CgsPhysics::PhysicsSimulationModule,      mVehicleManager);
        BRNPHYS_ADJACENT(mVehicleManager,     BrnPhysics::Vehicle::VehicleManager,      mContactData);
        BRNPHYS_ADJACENT(mContactData,        ContactSpy::ContactSpyData,               mDeformationManager);
        BRNPHYS_ADJACENT(mDeformationManager, Deformation::DeformationManager,          mDeformationInput);
        BRNPHYS_ADJACENT(mDeformationInput,   Deformation::DeformationInputInterface,   mDeformationOutput);
        BRNPHYS_ADJACENT(mDeformationOutput,  Deformation::DeformationOutputInterface,  mPropManager);
        BRNPHYS_ADJACENT(mPropManager,        Props::PropManager,                       mePrepareStage);

#undef BRNPHYS_ADJACENT

        // ------------------------------------------------------------------------------------
        // PART 3 -- the width-invariant tail, every member pinned to its X360 seat.
        // ------------------------------------------------------------------------------------
        BRNPHYS_TAIL_SEAT(mePrepareStage,    PhysicsModule::EPrepareStage,           433072);
        BRNPHYS_TAIL_SEAT(meReleaseStage,    PhysicsModule::EReleaseStage,           433076);
        BRNPHYS_TAIL_SEAT(mWorldRigidBodyId, CgsPhysics::RigidBodyId,                433080);
        BRNPHYS_TAIL_SEAT(mWorldEntityId,    CgsSceneManager::EntityId,              433088);
        BRNPHYS_TAIL_SEAT(meCurrentGameMode, BrnGameState::GameStateModuleIO::EGameModeType, 433092);

        // The 27 perf-monitor ids. Each seat below is the slot the monitor's OWN NAME predicts in
        // Construct's asm -- "    Simulation" really does store to 433100, and so on for all 21 of
        // the registered ones; the six miPropManager* ones are the zero-stores.
        BRNPHYS_TAIL_SEAT(miPhysicsPreSceneUpdatePM,                 s32, 433096);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateSimulationPM,               s32, 433100);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateReadUpdatedBodiesPM,        s32, 433104);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateVehiclePhysicsPM,           s32, 433108);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateCrashPredictionPM,          s32, 433112);
        BRNPHYS_TAIL_SEAT(miDeformationMaintenancePM,                s32, 433116);
        BRNPHYS_TAIL_SEAT(miContactSpyListGenerationPM,              s32, 433120);
        BRNPHYS_TAIL_SEAT(miGenerateSceneQueriesPM,                  s32, 433124);
        BRNPHYS_TAIL_SEAT(miPhysicsProcessRaceCarContactsPM,         s32, 433128);
        BRNPHYS_TAIL_SEAT(miDeformationManagerPM,                    s32, 433132);
        BRNPHYS_TAIL_SEAT(miPhysicsBridgeContactsPM,                 s32, 433136);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateContactGenAsyncPM,          s32, 433140);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateDoVehicleContactGenStartPM, s32, 433144);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateDoVehicleContactGenEndPM,   s32, 433148);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateDoPartContactGenStartPM,    s32, 433152);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateDoPartContactGenEndPM,      s32, 433156);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateDoPropContactGenStartPM,    s32, 433160);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateDoPropContactGenEndPM,      s32, 433164);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateValidateRaceCarWorldContactPM, s32, 433168);
        BRNPHYS_TAIL_SEAT(miPropManagerPM,                           s32, 433172);
        BRNPHYS_TAIL_SEAT(miPropManagerPreScenePM,                   s32, 433176);
        BRNPHYS_TAIL_SEAT(miPropManagerWorldContactGenPM,            s32, 433180);
        BRNPHYS_TAIL_SEAT(miPropManagerProcessInputsPM,              s32, 433184);
        BRNPHYS_TAIL_SEAT(miPropManagerReadUpdatedBodiesPM,          s32, 433188);
        BRNPHYS_TAIL_SEAT(miPropManagerOutputUpdatedPropsPM,         s32, 433192);
        BRNPHYS_TAIL_SEAT(miPropManagerApplyShockwavePM,             s32, 433196);
        BRNPHYS_TAIL_SEAT(miPhysicsUpdateFixUpVehContactsPM,         s32, 433200);

        BRNPHYS_TAIL_SEAT(miFramesToForceSuperSlowMotion, s32,  433204);
        BRNPHYS_TAIL_SEAT(mbIsOnlineGameMode,             bool, 433208);

        // The two widths the asm proved directly, asserted as widths so a retype that preserves
        // spacing still fails.
        static_assert(sizeof(PhysicsModule::mWorldRigidBodyId) == 8,
                      "mWorldRigidBodyId is written with a single `stdx` -- eight bytes");
        static_assert(sizeof(PhysicsModule::mbIsOnlineGameMode) == 1,
                      "mbIsOnlineGameMode is written with `stbx` -- one byte");

        // Whole-block span: 433208 - 433072 == 136. Fires if anything is inserted or removed
        // anywhere in the tail, including a change the per-member seats above could not localise.
        static_assert(offsetof(PhysicsModule, mbIsOnlineGameMode) - offsetof(PhysicsModule, mePrepareStage)
                          == KU_OFF_ISONLINEGAMEMODE - KU_OFF_PREPARESTAGE,
                      "PhysicsModule: the trailing state/perf-monitor block must span exactly 136 bytes");
    }

#undef BRNPHYS_TAIL_SEAT
}

// ==================================================================================================
// TAMPER LOG -- 2026-08-03, task #123. Recorded because a gate that has never failed has never
// been tested. THIRTEEN cases, each applied, compiled and reverted by a harness; TWELVE fire.
//
//   FIRES  KU_X360_SIZEOF_CONTACTSPYDATA 122544 -> 122560   (PART 1, breaks the walk at
//          mDeformationManager and every anchor after it -- the cascade signature)
//   FIRES  KU_NUM_PERFMON_IDS 27 -> 21 (the count the brief and the two retired comments gave)
//   FIRES  mWorldRigidBodyId  CgsPhysics::RigidBodyId -> u32   (the type check, the sizeof==8
//          check, and every tail seat from mWorldEntityId onwards)
//   FIRES  delete miPropManagerPreScenePM        (C2039 on the name itself, before the seats)
//   FIRES  miContactSpyListGenerationPM  s32 -> f32  (same width, same seat -- caught ONLY by the
//          is_same type check inside BRNPHYS_TAIL_SEAT; this is the trap that motivated it)
//   FIRES  miPropManagerPM             s32 -> u32     (ditto -- same width, same seat)
//   FIRES  true swap of mDeformationInput / mDeformationOutput   (PART 2 order asserts)
//   FIRES  INSERT an 8th sub-object before mDeformationInput     (the adjacency asserts)
//   FIRES  INSERT a u8[16] between mPropManager and the tail     (the adjacency asserts)
//   FIRES  mContactData ContactSpyData -> u8[122544]  (the anti-slicing check -- exactly the defect
//          this wave was called in to prevent, and the arithmetic alone does NOT see it)
//   FIRES  mDeformationManager  DeformationManager -> DeformationManager*  (type check)
//   FIRES  mbIsOnlineGameMode bool -> s32            (type check + the sizeof==1 assert)
//
//   SILENT append a `bool mbSpare;` AFTER mbIsOnlineGameMode. It lands in the tail's alignment
//          padding, so neither the arithmetic nor the 136-byte span can see it. This is blind
//          spot (c) from the banner, stated rather than hidden: the guard is that the DWARF member
//          list is quoted verbatim in BrnPhysicsModule.h, so an ADDED member has to be added to a
//          quoted list, and a DELETED one fires immediately (see the miPropManagerPreScenePM case).
//
// THE ADJACENCY BLOCK IN PART 2 EXISTS BECAUSE OF THIS LOG. The first run of the harness had a
// badly-built "reorder" case that INSERTED a member instead of swapping two, and it came back
// SILENT -- correctly, because PART 1 never touches a host type and every PART 3 seat is relative
// to mePrepareStage, so both were blind to an inserted eighth sub-object. The adjacency asserts
// were added to close that, and the two INSERT cases above are the regression tests for it.
// Two of the thirteen cases are therefore tamper tests that found a real hole, not confirmations.
// ==================================================================================================
