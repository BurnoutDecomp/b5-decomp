// ============================================================================
// GameSource/Director/BrnMainDirector.cpp
//
// BrnDirector::MainDirector -- the top-level cinematic camera director. Compilation
// home for the 21-function MainDirector class TU. The ENGINE source path the X360
// asserts quote is "..\..\..\GameSource\Director/BrnMainDirector.cpp", so this file
// mirrors it.
//
// BODIED here (faithfully reconstructed from the X360 asm; they touch only the attested
// stage scalars / list heads and committed-destructor calls):
//   * MainDirector()  -- ctor: in-place build the owned sub-objects + seed the -1 sentinels
//   * Release()       -- the staged RELEASE state machine
//   * Destruct()      -- null the four bookkeeping list heads + destruct collision-gen + ICE
//   * GetICEWrapper() / GetArbitrator() -- the committed-sub-object accessors
//
// DECLARATION-ONLY + FLAGGED (in the header): the remaining 16 functions -- the Construct
// VMX/LCG seed pipeline, the Update spine and its sub-updates (UpdateArbitrator / UpdateICE /
// UpdateMoments / UpdateAttribSys / UpdateCameraBehaviours* / UpdateDebug* / ProcessInputQueue /
// ProcessNewVehicleEvents / HandlePrepareForModeAction / CalcTrafficLightSpace /
// DebugDisplayCurrentCamera / PreSceneQueryUpdate / PostGuiUpdate / Prepare). Each indexes the
// NOT-HOMED BehaviourManager / AllVehicleData / GameState-action aggregates, paraphrases a VMX
// pipeline, depends on un-dumped rodata, or calls a declaration-only sibling -- the rules forbid
// bodying any of those. They are documented + declared in BrnMainDirector.h and resolve to their
// real bodies when the dependent TUs land.
//
// LAYOUT NOTE: MainDirector is modelled as a sized opaque buffer (see BrnMainDirector.h). The
// bodied functions address their fields at the X360-attested CONSOLE byte offsets through a
// char*/typed view of `this` -- the established convention in the committed BrnDirectorModule.cpp.
// ============================================================================

#include "GameSource/Director/BrnMainDirector.h"

#include "GameSource/Director/BrnDirectorICEWrapper.h"   // BrnDirector::ICEWrapper (embedded sub-object: Destruct)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h" // BrnDirector::Arbitrator (embedded sub-object accessor)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // External sub-object types this TU only references by call (their full homes are
    // separate TUs; the per-TU `cl /c` gate compiles against these minimal externals and the
    // real symbols resolve at link time -- the BrnDirectorModule.cpp convention).
    // ------------------------------------------------------------------------

    // FLAG: CgsSceneManager::CgsCollision::BaseCollisionGenerator has no reconstructed home
    //   layout yet (the committed CgsSceneManagerModule.h forward-declares it only). Destruct /
    //   Release call its Destruct() on the embedded CgsGraphics::Camera / collision-generator
    //   object; declared here as a minimal external with just that member so the call compiles.
    //   Replace with the real home when the collision-generator TU lands.
}

namespace CgsSceneManager { namespace CgsCollision {
    struct BaseCollisionGenerator { void Destruct(); };
} }

namespace BrnDirector
{
    // ICETake, ArbitratorStateContainer, BehaviourManager and CarScoreData are the owned
    // sub-objects the ctor builds in place. Their construction is performed at their attested
    // regions via these minimal external constructors (placement-new), exactly as
    // BrnDirectorModule.cpp builds its owned sub-objects. FLAG: minimal externals -- the real
    // constructors live in each type's own TU.
    namespace ICE_External { struct ICETake { ICETake(); }; }
    struct ICEWrapperBuild           { ICEWrapperBuild(); };          // -> ICEWrapper() at +0x50
    struct ArbitratorStateContainerB { ArbitratorStateContainerB(); }; // -> at +0x12DB4 region
    struct BehaviourManagerBuild     { BehaviourManagerBuild(); };    // -> at +0x1CB10 region
    struct CarScoreDataBuild         { CarScoreDataBuild(); };        // -> at +0x33760 region

    // ------------------------------------------------------------------------
    // small typed-view helpers (the BrnDirectorModule.cpp idiom: address the attested
    // CONSOLE offsets through a char* view of `this`; never a host-layout cast).
    // ------------------------------------------------------------------------
    static inline char* lpByteView(MainDirector* lpThis)
    {
        return reinterpret_cast<char*>(lpThis);
    }
    static inline s32& lrStageWord(MainDirector* lpThis)
    {
        return *reinterpret_cast<s32*>(lpByteView(lpThis) + MainDirector::KU_OFF_STAGE);
    }
    static inline s32& lrStageCounter(MainDirector* lpThis)
    {
        return *reinterpret_cast<s32*>(lpByteView(lpThis) + MainDirector::KU_OFF_STAGE_COUNTER);
    }

    // ------------------------------------------------------------------------
    // GetICEWrapper / GetArbitrator -- the embedded committed sub-objects, reached at their
    // attested CONSOLE regions (parity by name; the offset is provenance).
    // ------------------------------------------------------------------------
    ICEWrapper& MainDirector::GetICEWrapper()
    {
        return *reinterpret_cast<ICEWrapper*>(lpByteView(this) + KU_OFF_ICE_WRAPPER);
    }

    Arbitrator& MainDirector::GetArbitrator()
    {
        // The embedded Arbitrator region (CONSOLE +0x12DC0: the ctor builds the state container
        // at +0x12DC0+0x310 and seeds the arbitrator vtables there -- BrnDirectorArbitrator.h).
        return *reinterpret_cast<Arbitrator*>(lpByteView(this) + 0x12DC0);
    }

    // ------------------------------------------------------------------------
    // MainDirector (ctor)  @ X360 0x827E4AB8  (EXECUTED in goal trace)
    //
    // Build the owned sub-objects in place and seed the -1 sentinel index fields the asm
    // stores. The asm sequence (provenance):
    //   ICEWrapper()                    at +0x50
    //   word -1 -> +0x122B8/+0x12384/+0x12450   (three -1 sentinel indices, r11=+0x121F0)
    //   ICETake()                       at +0x124F0
    //   word -1 -> +0x12DB4                      (r10=+0x12DB4)
    //   ArbitratorStateContainer()      at +0x130D0   (arbitrator region +0x12DC0 + 0x310)
    //   ...embedded arbitrator special-state vtable installs at +0x3910/+0x41A0/+0x4340 of the
    //      arbitrator region (NOT reproduced here; they belong to the embedded Arbitrator)
    //   BehaviourManager()              at +0x1CB10   (r3 = +0x1CB10)
    //   word -1 -> +0x34974                      (r9, stwx)
    //   CarScoreData()                  at +0x33760
    //   word -1 -> +0x34DC0                      (0x250 off r11=+0x34B70)
    //   a 16-entry table of -1 from +0x34DF8 (stride 0x2C)   -- the per-rival index table
    //   word -1 -> +0x35090 (0x2C0 off r8=+0x34DD0) / +0x353A8 (0x838 off r11=+0x34B70)
    //
    // BODIED FAITHFULLY for the parts that touch only the attested sentinel fields + the owned
    // sub-object placement-builds. The owned sub-objects are constructed via minimal external
    // builders (placement-new at their attested regions) -- the BrnDirectorModule.cpp pattern.
    //
    // FLAG: the embedded BehaviourManager and the arbitrator special-state vtable installs are
    //   un-homed; the BehaviourManager build is performed by its external builder (its real ctor
    //   resolves at link), and the per-rival -1 index table is seeded by attested offset/stride.
    //   The vtable-pointer installs the asm makes into the arbitrator region are NOT reproduced
    //   here (they belong to the embedded Arbitrator's own construction, performed in its region).
    // ------------------------------------------------------------------------
    MainDirector::MainDirector()
    {
        char* lpBase = lpByteView(this);

        // Owned sub-objects, built in place at their attested regions.
        new (reinterpret_cast<void*>(lpBase + KU_OFF_ICE_WRAPPER)) ICEWrapperBuild();      // +0x50
        new (reinterpret_cast<void*>(lpBase + 0x124F0))           ICE_External::ICETake(); // +0x124F0
        new (reinterpret_cast<void*>(lpBase + 0x130D0))           ArbitratorStateContainerB(); // +0x130D0
        new (reinterpret_cast<void*>(lpBase + 0x1CB10))           BehaviourManagerBuild();  // +0x1CB10
        new (reinterpret_cast<void*>(lpBase + 0x33B50))           CarScoreDataBuild();      // +0x33B50 (asm: addis r3,r30,3 -> +0x30000, addi r3,r3,0x3B50)

        // -1 sentinel index fields (asm word stores, offsets read straight from the asm).
        *reinterpret_cast<s32*>(lpBase + 0x122B8) = -1;   // stw r31,0xC8(r11=+0x121F0)
        *reinterpret_cast<s32*>(lpBase + 0x12384) = -1;   // stw r31,0x194(r11)
        *reinterpret_cast<s32*>(lpBase + 0x12450) = -1;   // stw r31,0x260(r11)
        *reinterpret_cast<s32*>(lpBase + 0x12DB4) = -1;   // stwx r31,r30,(r10=+0x12DB4)
        *reinterpret_cast<s32*>(lpBase + 0x34974) = -1;   // stwx r31,r30,(r9=+0x34974)
        *reinterpret_cast<s32*>(lpBase + 0x34DC0) = -1;   // stw r31,0x250(r11=+0x34B70)

        // The 16-entry per-rival index table seeded to -1: first store at +0x34DF8 (r9 = r8+0x28,
        // r8 = +0x34DD0), stride 0x2C bytes, 16 iterations (the asm's r10 = 0xF..-1 down-count).
        char* lpTable = lpBase + 0x34DF8;
        for (s32 liEntry = 15; liEntry >= 0; --liEntry)
        {
            *reinterpret_cast<s32*>(lpTable) = -1;
            lpTable += 0x2C;
        }

        *reinterpret_cast<s32*>(lpBase + 0x35090) = -1;   // stw r31,0x2C0(r8=+0x34DD0)
        *reinterpret_cast<s32*>(lpBase + 0x353A8) = -1;   // stw r31,0x838(r11=+0x34B70)
    }

    // ------------------------------------------------------------------------
    // Release  @ X360 0x82236EB0
    //
    // The staged RELEASE state machine. The stage word (CONSOLE +0x35424) selects the case;
    // each case advances it and the cases fall through (1->2->3->5). On the first (stage 0)
    // it destructs the collision generator; on the last (stage 5) it clears the stage counter
    // (+0x35420 -> 0) and reports completion. An out-of-range stage asserts and reports failure.
    //
    // (The asm "case 4" is the default branch -- there is no case 4 in the jump table.)
    // Faithful to the asm: NO added control flow; the fall-through chain matches the jump table.
    // ------------------------------------------------------------------------
    bool MainDirector::Release()
    {
        s32& lrStage = lrStageWord(this);

        switch (lrStage)
        {
        case 0:
            // Release case-0 tears down the object at the ICE-wrapper region (CONSOLE +0x50):
            // asm 0x82236F0C `addi r3,r30,0x50` -> BaseCollisionGenerator::Destruct(this+0x50).
            // (The FULL Destruct() below tears down the collision generator at +0x349D0; these
            //  are different call sites and must not be conflated.)
            reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(
                lpByteView(this) + KU_OFF_ICE_WRAPPER)->Destruct();
            // fall through
        case 1:
            lrStage = 1;
            // fall through
        case 2:
            lrStage = 2;
            // fall through
        case 3:
            lrStage = 3;
            // fall through
        case 5:
            lrStage = 5;
            lrStageCounter(this) = 0;
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Destruct  @ X360 0x8224FCC0
    //
    // Null the four per-frame bookkeeping list heads (64-bit zero stores), destruct the embedded
    // CgsGraphics::Camera / collision-generator object, then destruct the embedded ICE wrapper.
    // Faithful to the asm store-for-store.
    // ------------------------------------------------------------------------
    void MainDirector::Destruct()
    {
        char* lpBase = lpByteView(this);

        // Four list/tree heads nulled (the asm stores a 64-bit zero into each).
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_A) = 0;   // +0x1CAF8
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_B) = 0;   // +0x24808
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_C) = 0;   // +0x2C638
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_D) = 0;   // +0x2F038

        // Destruct the embedded collision generator (CONSOLE +0x349D0).
        reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(
            lpBase + KU_OFF_COLLISION_GENERATOR)->Destruct();

        // Destruct the embedded ICE wrapper (CONSOLE +0x50).
        GetICEWrapper().Destruct();
    }
}
