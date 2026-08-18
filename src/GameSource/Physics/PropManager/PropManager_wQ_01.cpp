// GameSource/Physics/PropManager/PropManager_wQ_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props keystone wave (waveQ, 2026-08-18), group 1.
// Part-file of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp; the rest of
// this class's bodies live in BrnPropManager.cpp beside it. The block banner, the full member
// map and the retired park list are there and are NOT repeated here.
//
//     Release()   @ 0x825BAC88   (9 instructions + one `.long 0` pad, no callees) -- DONE below
//     Destruct()  @ 0x825E3398   (25 instructions, 0x825E3398..0x825E33F8)         -- DONE below
//
// (Both counts were wrong in round 1 -- "10" counted the pad word and "20" was simply short by
//  five. RE-COUNTED 2026-08-18 from the `assembly` arrays: (0x825E33F8-0x825E3398)/4+1 == 25.)
//
// Both bodies are the same reading: THE COMPILER DELETED A LOOP OVER mpaPropInstances BECAUSE
// THE CALLEE IS EMPTY. That is not a story told to explain a short function -- the DecFIGS
// dwarfdump for THIS .cpp names the loops' own locals and their source lines:
//
//     // BrnPropManager.cpp:243
//     void BrnPhysics::Props::PropManager::Release() {
//         { // BrnPropManager.cpp:245   uint32_t luIndex;
//           // BrnPropManager.cpp:246   bool     lbSuccess;  }
//     }
//     // BrnPropManager.cpp:273
//     void BrnPhysics::Props::PropManager::Destruct() {
//         { // BrnPropManager.cpp:275   uint32_t luIndex; }
//     }
//
// A `luIndex` in a function whose emission contains no loop is a loop the optimiser removed,
// and the two callees it looped over -- PropInstance::Release() / PropInstance::Destruct() --
// are already committed (PropPhysics/BrnPropInstance.h) as constant-true and empty, measured
// from exactly these two emissions. Nothing here is fabricated: the loop bounds, the array and
// the fold are each pinned below against the raw ARTIST asm.
//
// ⚠️ NOTE ON THE `bl BaseCollisionGenerator::Destruct` IN Destruct'S TAIL -- DO NOT RESURRECT
//    THE BASE-CLASS THEORY. PropManager has no base (the dwarfdump prints base classes and
//    prints none for it; Construct @0x82627390 calls PropDebugComponent::Construct with
//    r3 == r4 == this, so mDebugComponent is AT +0x00). 0x8284CB38 is an ICF-folded empty
//    `void f(T*)` body that THREE different empty bodies in this one subsystem resolve to --
//    PropDebugComponent::Construct @0x825BAD74 (where CgsDev::DebugComponent::Construct
//    belongs), PropDebugComponent::OnRegister @0x822A9750 (a bare `b` to it, where
//    DebugComponent::OnRegister belongs), and this tail. Here it is the base-class call at the
//    end of PropDebugComponent::Destruct, which is INLINED into this function -- the committed
//    PropDebugComponent::Destruct body (BrnPropDebugComponent.cpp) is exactly
//    `CGS_ASSERT(mpPropManager, ...); mpPropManager = NULL; CgsDev::DebugComponent::Destruct();`
//    and that STATEMENT RUN is what 0x825E3398 emits.
//
// ⚠️⚠️ CORRECTED 2026-08-18 (round 2) -- THE OLD WORDING HERE SAID "that is, instruction for
//    instruction, the whole of what 0x825E3398 emits", AND THAT IS FALSE IN THE COMMITTED TREE.
//    Measured, both halves:
//      * On the CONSOLE the fold target really is empty: 0x8284CB38 is a single `blr` (plus a
//        `.long 0` pad), with 193 xrefs_to. So the console's CgsDev::DebugComponent::Destruct()
//        emits nothing, and 0x825E3398's last state-changing instruction is `stw r11, 0xC(r31)`.
//      * In THIS TREE it is not empty. CgsDev::DebugComponent::Destruct()
//        (GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.cpp:57-60)
//        executes `mbActive = false;`. So on the host this function performs one store into the
//        DebugComponent base sub-object that the X360 emission does not perform.
//    THE CALL BELOW IS STILL CORRECT AND MUST STAY -- the console really does `bl` the base
//    Destruct from here; what diverges is the CALLEE's body, in a file this TU does not own.
//    The same divergence sits on the Construct side and is visible from this subsystem's own
//    asm: CgsDebugComponent.cpp:49-53 gives DebugComponent::Construct() two stores (mbActive,
//    mpDebugLinkedListNext), yet PropDebugComponent::Construct @0x825BAD58 stores ONLY +0xC,
//    +0x10 and +0x11 and `bl`s the same bare `blr` -- nothing lands in +0x00..+0x0B. Four
//    sibling teardowns agree (0x82586578 is literally `li r11,0 ; stw r11,0xC(r3) ;
//    b 0x8284CB38`; 0x825852C0 / 0x824EC9D0 / 0x825864FC likewise store only their own members
//    at >=0xC), and progress/identity.json contains no CgsDev::DebugComponent::Construct or
//    ::Destruct symbol at all. That foreign body's own comment admits it is inference ("the X360
//    inlines them; bodies reconstructed from the attested member set"); five emissions refute it.
//    CONDUCTOR ITEM (not landable from here -- foreign TU): empty CgsDev::DebugComponent::
//    Construct() and ::Destruct(), leaving the member seeding where the tree already also does
//    it, in the real C++ constructor at CgsDebugComponent.cpp:34-38.
//
//    Corroborating that PropManager has no base: BeginPropWorldContactGeneration @0x82628CB0
//    calls the generator's Prepare on its SECOND DECLARED PARAMETER, lpCollisionGenerator --
//    which arrives in r5, the third GPR slot, because `this` occupies r3 (0x82628CC4 `mr r30,r5`;
//    0x82628CD4 `mr r31,r3`; 0x82628CE0 `mr r3,r30`; 0x82628CE8 `bl BaseCollisionGenerator::
//    Prepare`) -- and NOT on `this`. Round 1 called it "the THIRD ARGUMENT", which reads as a
//    parameter position and is wrong: register number != parameter position in this repo (the
//    VecFloat lvfTimeStep rides v1 and consumes no GPR at all, so r6 is lpLinearMalloc).
//
// No console offset, stride or object size from the asm is used as a host value anywhere in
// this file -- every X360 immediate quoted below is in a comment, and the C++ reaches its data
// by member name only.

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/BrnPropDebugComponent.h"          // PropDebugComponent::Destruct
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"    // PropInstance::Release / ::Destruct

namespace BrnPhysics
{
namespace Props
{

// =========================================================================================
// BrnPhysics::Props::PropManager::Release @ 0x825BAC88   (DWARF BrnPropManager.h:132,
// definition at BrnPropManager.cpp:243).  THE WHOLE SHIPPED FUNCTION, VERBATIM:
//
//     0x825BAC88  lwz     r11, 0x88(r3)        r11 = muNumberOfPropInstances
//     0x825BAC8C  li      r3, 1                lbSuccess = true
//     0x825BAC90  cmplwi  cr6, r11, 0
//     0x825BAC94  beqlr   cr6                  zero instances -> return true
//     0x825BAC98  addi    r11, r11, -1     <-+ the loop, run BACKWARDS as a count-down
//     0x825BAC9C  clrlwi  r3, r3, 31         | r3 &= 1   <- the only surviving loop BODY
//     0x825BACA0  cmplwi  cr6, r11, 0        |
//     0x825BACA4  bne     cr6, 0x825BAC98  --+
//     0x825BACA8  blr                          return lbSuccess
//
// Reading, term by term (measured, not inferred):
//   * `+0x88` is muNumberOfPropInstances -- the trip count, so the loop bound.
//   * the count-down is the optimiser reversing a loop whose induction variable is unused in
//     the body; `luIndex` (DWARF :245) is therefore gone from the emission, which is exactly
//     why the DWARF is the evidence for it and the asm is not.
//   * `clrlwi r3,r3,31` == `r3 = r3 & 1`. A `&= 1` per iteration, with no load and no call, is
//     `lbSuccess &= <constant true>` -- i.e. the per-element call was folded to the literal 1.
//     That fold is the measurement behind the committed `PropInstance::Release() { return true; }`
//     (see BrnPropInstance.h); the two facts are one fact and must stay in sync.
//   * mpaPropInstances (+0x7C) is consequently never LOADED here. Naming it in the C++ below is
//     INFERENCE from the DWARF loop shape, not from this emission -- stated plainly because a
//     later sweep must not "discover" the missing load and delete the indexing.
//
// ⚠️ LANDING-ORDER HAZARD, MEASURED 2026-08-18 (round 2) -- for the conductor, not a code change.
//    Both loops in this file iterate on muNumberOfPropInstances (+0x88), and NOTHING WRITES IT.
//    That is now measured rather than suspected: a scan of every BrnPhysics::Props::PropManager
//    function in the ARTIST export set for a store to +0x88 or +0x98 returns ZERO hits, and in
//    particular PropManager::Prepare @0x8260EE18 -- the only plausible initialiser, and now
//    correctly addressed in BrnPropManager.h -- stores to exactly four `this` offsets (+0x80
//    mUsedProps, +0x90 mUsedParts, +0x7C mpaPropInstances, +0x8C mpaPartInstances) and neither
//    counter is among them. Prepare is ALSO still an inert boot gate (WorldLinkStubs.cpp:516),
//    so on a real boot mpaPropInstances is null as well.
//    Release is faithful either way (the X360 spins the same garbage count), but Destruct's loop
//    is one the console emits ZERO instructions for, so an UNOPTIMISED host build gets a
//    shutdown spin of up to 2^32 empty iterations that the console does not have. An optimised
//    build deletes it (PropInstance::Destruct is `{}` inline). Sequence Prepare before anything
//    calls PhysicsModule::Release/Destruct on a real boot.
//
// Sole caller: BrnPhysics::PhysicsModule::Release @0x8259C1C0.
// =========================================================================================
bool PropManager::Release()
{
    bool lbSuccess = true;

    for (u32 luIndex = 0; luIndex < muNumberOfPropInstances; ++luIndex)
    {
        lbSuccess &= mpaPropInstances[luIndex].Release();
    }

    return lbSuccess;
}

// =========================================================================================
// BrnPhysics::Props::PropManager::Destruct @ 0x825E3398   (DWARF BrnPropManager.h:136,
// definition at BrnPropManager.cpp:273).  THE WHOLE SHIPPED BODY, prologue/epilogue elided:
//
//     0x825E33AC  lwz     r11, 0xC(r31)                 mDebugComponent.mpPropManager
//     0x825E33B0  cmplwi  cr6, r11, 0
//     0x825E33B4  bne     cr6, 0x825E33D8
//     0x825E33B8  bl      CgsDev::Assert::BeginAssert
//     0x825E33BC  lis     r11, aDP4B5MainBurno_205@ha   (the @ha half of the file-string addr)
//     0x825E33C0  li      r5, 0x45                      == line 69
//     0x825E33C4  addi    r4, r11, ...@l  "d:\p4\b5_main\burnout\main\code\gamesource\unity\
//                                          ../Physics/PropManager/BrnPropDebugComponent.cpp"
//     0x825E33C8  lis     r11, aMppropmanagerN@ha
//     0x825E33CC  addi    r3, r11, ...@l  "mpPropManager != NULL"
//     0x825E33D0  bl      CgsDev::Assert::FireAssert
//     0x825E33D4  bl      CgsDev::Assert::EndAssert
//     0x825E33D8  li      r11, 0
//     0x825E33DC  mr      r3, r31                       this  (== &mDebugComponent, +0x00)
//     0x825E33E0  stw     r11, 0xC(r31)                 mpPropManager = NULL
//     0x825E33E4  bl      <0x8284CB38>                  the ICF-folded body -- see the banner;
//                                                       EMPTY on the console, NOT empty in this
//                                                       tree (one added mbActive store)
//
// (Transcript order CORRECTED 2026-08-18: round 1 printed 0x825E33C4 before 0x825E33C0 and
//  dropped both `lis ...@ha` setups, which made the two `addi r4/r3, r11, ...` read as if r11
//  were still live from the earlier `lwz r11, 0xC(r31)`. It is not -- each `addi` has its own
//  `lis` immediately above it. Prologue/epilogue (mflr/stw/std/stwu/mr and the four-instruction
//  restore + blr) are the remaining 9 of the 25 and are elided as boilerplate.)
//
// The FILE AND LINE baked into the assert are BrnPropDebugComponent.cpp:69, not
// BrnPropManager.cpp: this whole run is PropDebugComponent::Destruct INLINED, and it is
// reproduced here as the one call `mDebugComponent.Destruct()` rather than re-open-coded, so
// the assert keeps its real owner. The committed body of that callee matches the run exactly.
//
// The per-instance loop is DELETED FROM THE EMISSION -- not one instruction of it survives,
// because PropInstance::Destruct() is empty (measured from this very absence; the +0x88 load
// that Release keeps is not even present here). Its existence rests entirely on the DWARF
// local `luIndex` at BrnPropManager.cpp:275, and it is written back below because the DWARF
// is authoritative for source shape. It is a no-op at run time either way.
//
// ⚠️ THE RELATIVE ORDER OF THE TWO STATEMENTS IS NOT RECOVERABLE, and is stated rather than
//    smoothed over: one of them emitted nothing at all, so the emission cannot order it
//    against the other. `mDebugComponent.Destruct()` is written FIRST here only because it is
//    the statement the image actually shows, at the top of the body. Both are side-effect-free
//    with respect to each other (the deleted loop touches neither mDebugComponent nor
//    mpPropManager), so the choice is behaviourally immaterial.
//
// ⚠️ Nothing else is destructed here -- not mUpdatedProps, not mUpdatedJointedProps, and the
//    mpDebugWorldContacts block Construct allocated is NOT freed. That is the shipped image's
//    behaviour (the emission has no other call), and it is left alone rather than "completed".
//
// Sole caller: BrnPhysics::PhysicsModule::Destruct @0x8259C310.
// =========================================================================================
void PropManager::Destruct()
{
    mDebugComponent.Destruct();

    for (u32 luIndex = 0; luIndex < muNumberOfPropInstances; ++luIndex)
    {
        mpaPropInstances[luIndex].Destruct();
    }
}

}
}
