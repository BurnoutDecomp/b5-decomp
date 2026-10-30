// Layout check for BrnVehicleManager.h's TUNING BANK (this+171464 .. this+172616).
//
// WHY THIS TU EXISTS -- READ BEFORE DELETING IT.
// BrnVehicleManager.h advertises that its recovered offsets are "pinned by the offsetof asserts in
// _AssertLayout / _AssertLayoutPlayerStats", and that "the gate FAILS if any padding run is wrong,
// which is the signal". That was NOT TRUE as of 2026-08-03: both of those functions live in
// BrnVehicleManager.cpp / BrnVehicleManagerPlayerStats.cpp, and **neither TU is mounted in
// tools/build/build_game_exe.bat**. A static_assert in a TU that is never compiled is not a gate,
// it is a comment -- so every padding run in that ~172 KB class had been carried for many waves
// with nothing checking it. A green build proved exactly nothing about the layout.
//
// This TU closes that hole the cheap way: it is mounted, it includes the header, and it defines a
// third never-called static member (declared in the header alongside the other two) whose body is
// nothing but static_asserts. static_assert fires at COMPILE time, so /OPT:REF discarding the
// (uncalled) function afterwards is irrelevant -- unlike an LNK2019 witness, this one cannot be
// optimised away before it has done its job. Being a static member is what gives it access to the
// private members offsetof has to see.
//
// It pins the span this wave resolved. The pre-existing asserts in the two unmounted TUs are left
// where they are; mounting those TUs is a link-closure question, not a layout one.
//
// PROVENANCE of every offset below: an indexed store in VehicleManager::Construct @0x8263B7C8,
// symbolically resolved from the X360 asm, cross-checked against that function's Hex-Rays and
// against the PS3 DecFIGS build's same function @0x6EB6BC at a uniform shift of Δ = 672.
// Names are the DecFIGS DWARF's (references/DecFIGS/dwarfdump/.../BrnVehicleManager.h:865-1088).

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

// THIS INCLUDE IS ITSELF A GATE, not a
// convenience. BrnPhysicalTrafficManager.h used to define its own `struct VehicleDriver
// { u8 mOpaque[224]; }` at namespace scope in BrnPhysics::Vehicle, while BrnVehicleManager.h
// (above) pulls in the REAL VehicleDriver for maRaceCarDrivers[8] / mPlayerAiDriver. The two
// headers therefore could not be included in the same translation unit -- a hard C2011 -- and
// VehicleManager::Construct @0x8263B7C8 has to call both VehicleDriver::Construct and
// PhysicalTrafficManager::Construct. That fork is retired; this line is what keeps it retired,
// because BrnPhysicalTrafficManager.h's only other includer (BrnPhysicalTrafficManager.cpp) is
// NOT mounted, so nothing else in the build would ever notice a re-fork.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
// The two sub-object classes whose real x64 size decides whether Construct can call their
// constructors at all. Included so the size verdicts below are compiled facts.
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"
// also a gate, not a convenience. sizeof(PotentialContact)
// is the one load-bearing INPUT to the backward derivation of the X360 sizeof(PhysicalTrafficManager)
// (see BrnPhysicalTrafficManager.h finding (4), derivation B). If that record ever gets a different
// committed layout, the derivation it feeds has to be redone -- so it is asserted here rather than
// left as a number in a comment.
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
// The per-car physics DebugComponent. BrnVehicleManager.h now embeds an array of these by value,
// so it pulls this header in itself; the line is kept because the size asserts at the bottom are
// about THIS type, and an include that documents what a gate measures is worth its line.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"

#include <cstddef>   // offsetof

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::_AssertLayoutTuningBank()
    {

        // ==========================================================================================
        // THE `VehicleManager::Construct` SUB-CONSTRUCTOR BLOCKER TABLE, as compiled asserts.
        //
        // These are not decoration. Construct @0x8263B7C8 calls six sub-constructors; three of them
        // take a `this` that VehicleManager cannot supply, because the member is an X360-sized
        // opaque span and the real x64 class is bigger. The header records the measured numbers;
        // these asserts make the build fail the day any of them changes, in EITHER direction --
        // including the good direction, which is the point: when a wave shrinks
        // VehicleManagerDebugComponent to 1296 or re-seats the traffic manager, one of these fires
        // and says "the blocker you were told about is gone".
        //
        // Deliberately written as `!=` / `==` against the MEASURED values rather than as
        // `<= span`, so they are tripwires, not permissions.
        // ==========================================================================================

        // READY -- fits exactly, so `VehicleDriver::Construct` is callable by name today.
        CGS_STATIC_ASSERT((sizeof(VehicleDriver) == 224),
                      "VehicleDriver fits maRaceCarDrivers[8] (stride 0xE0) and mPlayerAiDriver");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarDrivers) == 8 * 224),
                      "the eight-car driver array is 1792 bytes: 64 + 1792 == 1856 == maRaceCarVehicles");

        // READY -- the ONE contained sub-object whose real class fits its X360 span on x64.
        CGS_STATIC_ASSERT((sizeof(BrnPhysics::StuntOffencesManager) == 464),
                      "StuntOffencesManager fits this+44240..44704 exactly (no pointer members; "
                      "last member ends at 0x1C4 == 452, padded to 464 at align 16 on both ISAs)");
        CGS_STATIC_ASSERT((__alignof(BrnPhysics::StuntOffencesManager) == 16 &&
                      offsetof(VehicleManager, mStuntOffencesManager) % 16 == 0),
                      "and its 16-byte alignment is satisfied at 44240, so typing it moved nothing");

        // READY -- the discarded-contact queue fits because the pointer widening lands in the
        // padding the X360 header already carried: {T* , s32, s32} is 12->16 there and 8+4+4 == 16
        // here, then 16 + 20*64 == 1296 either way.
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mDiscardedContacts) == 1296),
                      "EventQueue<DiscardedContact,20> fits this+160672..161968 exactly");
        CGS_STATIC_ASSERT((sizeof(BrnPhysics::ContactSpy::DiscardedContact) == 64),
                      "and the 64-byte entry is what makes 16 + 20*64 == 1296 close");

        // ==========================================================================================
        // UN-BLOCKED 2026-08-03 -- the manager's own debug component, embedded BY NAME.
        //
        // This block read "BLOCKED -- grows 32 bytes on x64 ... CANNOT be called by name until
        // this class is 1296 or VehicleManager stops being byte-pinned". The second disjunct is what
        // happened. Note the difference from the traffic manager: THAT span was wrong, this one is
        // right -- 161968 and 163264 are both asm-literal -- so the +32 is a genuine host/console
        // width difference and is carried as the second drift term, not derived away.
        // ==========================================================================================
        CGS_STATIC_ASSERT((__alignof(VehicleManagerDebugComponent) == 16 &&
                      offsetof(VehicleManager, mDebugComponent) % 16 == 0),
                      "the component holds alignas(16) OutContactSpy/PotentialContact members, and "
                      "the drift is a multiple of 16 (asserted above), so embedding it inserted no "
                      "leading pad");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mDiscardedContacts)
                          + sizeof(((VehicleManager*)0)->mDiscardedContacts)
                          == offsetof(VehicleManager, mDebugComponent)),
                      "and the discarded-contact queue abuts it with no gap, exactly as on the X360");

        // ==========================================================================================
        // UN-BLOCKED 2026-08-03 -- the contained traffic manager, embedded BY NAME.
        //
        // This block used to read "BLOCKED -- grows 2480 bytes on x64", measured against a
        // 103360-byte opaque span. The SPAN was wrong, not the class: the X360 size is 105648
        // (BrnPhysicalTrafficManager.h finding (4), derived twice) and the host DRIFT is -3968
        // (2026-08-03, once maFullTrafficPhysics became the real TrafficPhysics[20]; it was +192
        // while that array was a byte-pinned stand-in), which BrnVehicleManager.h carries as
        // KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER.
        //
        // The three numbers are tied together here so none of them can drift alone.
        // ==========================================================================================
        // REBASED 2026-08-11 (the create-drain wave), and this assert FIRED, exactly as designed.
        // It used to read `44768 + KU_HOST_DRIFT_AFTER_RACECAR_ARRAY`, which was correct only while
        // maRaceCarModelHandles / maRaceCarGraphicsModelHandles were the opaque `mPadAA60[128]`.
        // Splitting them into the real CgsResource::ResourceHandle[8] pair (see BrnVehicleManager.h)
        // widened 8 -> 16 per element, 16 elements, == +128, and everything from +43744 on moved
        // with it. The correct term for this member is now KU_HOST_DRIFT_AFTER_MODEL_HANDLES.

        // ---- the model-handle split itself, pinned at both ends ------------------------------------
        // The two `stdx` in ProcessCreateEvents are the only writers, and they are at
        // this + 8*(idx + 0x154C) and this + 8*(idx + 0x1554) -- 43616 and 43680 on the console.
        // Nothing before +43616 moved, so those two seats hold verbatim on the host.
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarGraphicsModelHandles)
                          == offsetof(VehicleManager, maRaceCarModelHandles) + 8 * sizeof(CgsResource::ResourceHandle)),
                      "asm @0x826169E0/A60/A68: addi r8,r27,0x1554 ; slwi r9,r8,3 ; stdx r10,r9,r24 -- "
                      "the graphics array abuts the model array, one 8-element run later");
        // this used to read
        // `KU_X360_SIZEOF_... + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER`, which was right only while
        // that term was the FIRST drift zone. It is now the SECOND (the race-car array's -1664 sits
        // ahead of it), so what belongs here is the ZONE STEP -- the difference between the term
        // after this sub-object and the term before it. That is a strictly stronger assert: it fires
        // if either term moves independently, which the absolute form could not see.
        // THIS FIRED FOR REAL when the third term landed, and the wrong fix would have been to
        // bump the literal. The right one is here.
        CGS_STATIC_ASSERT((__alignof(PhysicalTrafficManager) == 16 &&
                      offsetof(VehicleManager, mPhysicalTrafficManager) % 16 == 0),
                      "44768 % 16 == 0, so embedding the real type inserted no leading pad");

        // ---- the traffic manager's own head, which IS pointer-free and so DOES reproduce ---------
        // These are the first compiled asserts this class has ever had. Everything up to (but not
        // including) maTrafficCarModelHandles is free of ResourceHandle/pointer widening, so the X360
        // offsets survive verbatim on the host -- and the one that matters is maTrafficEntityIDs,
        // because VehicleManager used to model it as a sibling of its own called
        // `maRaceCarEntityIdRemap[8]` at class +148128.
        // REWRITTEN 2026-08-03 (the TrafficPhysics de-fork). What stood here was
        //     static_assert(sizeof(TrafficPhysics) == 5168, "asm: mulli r11, r29, 0x1430 ...");
        // -- a HOST sizeof gate wearing a console literal. It held only because TrafficPhysics was a
        // byte-pinned `u8[5168]` stand-in, i.e. it asserted that the stand-in was still a stand-in.
        // Now that the member is the real class, the console 0x1430 is asserted where it belongs:
        // as CONSOLE ARITHMETIC over the recovered member seats, in TrafficPhysics_layout_check.cpp,
        // whose chain closes on this very 5168. Here what is checked is the thing this TU can see --
        // that the array is exactly 20 elements with no pad behind it, and that the console
        // identification still holds as console arithmetic.
        CGS_STATIC_ASSERT((X360Layout::KU_TP_SIZEOF == 5168u),
                      "asm: mulli r11, r29, 0x1430 -- the 20-element per-vehicle walk in "
                      "PhysicalTrafficManager::Construct @0x82636CA8");
        CGS_STATIC_ASSERT((20u * X360Layout::KU_TP_SIZEOF + 44768u == 148128u),
                      "⭐ THE IDENTIFICATION, as console arithmetic: 44768 + 20*0x1430 == 148128, the "
                      "exact class offset the old `EntityId maRaceCarEntityIdRemap[8]` sibling "
                      "claimed. Same address, real name, and the real bound is 20 (a TRAFFIC index), "
                      "not 8.");
        CGS_STATIC_ASSERT((offsetof(PhysicalTrafficManager, maTrafficEntityIDs)
                          == 20u * sizeof(TrafficPhysics)),
                      "the entity-id array must abut maFullTrafficPhysics[20] with no pad -- this is "
                      "what fires if the array bound changes or TrafficPhysics grows an alignment "
                      "that inserts one");
        CGS_STATIC_ASSERT((sizeof(((PhysicalTrafficManager*)0)->maTrafficEntityIDs) == 20 * 4),
                      "EntityId[20] -- SetRaceCarCrashing's owner==2 branch indexes it with a traffic "
                      "index, so the old [8] model was an out-of-bounds read for slots 8..19");
        CGS_STATIC_ASSERT((sizeof(((PhysicalTrafficManager*)0)->mArticulatedJointPool) == 832),
                      "asm: pool at this+103616, next written member at this+104448");
        CGS_STATIC_ASSERT((sizeof(((PhysicalTrafficManager*)0)->mu8GlobalToPhysicalEntityIndexMap) == 600),
                      "asm: cmplwi 0x258 in ValidateAndFixUpTrafficTrafficContact (the DWARF says 601; "
                      "the asm is authoritative and the 16-alignment that follows absorbs the byte)");

        // ---- the BACKWARD derivation of the traffic manager's X360 size, as a compiled fact ------
        // 44768 + 105648 + 128*sizeof(PotentialContact) + 4, rounded up to mDiscardedContacts'
        // 16-alignment, must equal the asm-pinned 160672. sizeof(PotentialContact) == 80 is the one
        // input that comes from a committed record rather than from the asm, so it is pinned.
        CGS_STATIC_ASSERT((sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) == 80),
                      "3 x Vector3 + 2 VolumeInstanceId + 2 uint32 + 2 uint16, 16-aligned (DWARF "
                      "CgsPotentialContact.h:60-68). This is derivation B's only non-asm input.");
        // REBASED 2026-08-06 (big-five #2): the old opaque mPadNonPhysicalContacts span was
        // PROMOTED to the DWARF's real maNonPhysicalContacts[128] + miNonPhysicalContactCount
        // (+ 12 tail-pad bytes). The derivation closure is unchanged -- it is now stated over
        // the real members' combined span.
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maNonPhysicalContacts)
                          + sizeof(((VehicleManager*)0)->miNonPhysicalContactCount)
                          + sizeof(((VehicleManager*)0)->mPadAfterNonPhysicalContacts)
                          == 128 * sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) + 4 + 12),
                      "maNonPhysicalContacts[128] + miNonPhysicalContactCount + 12 bytes of align pad "
                      "== 10256 == 160672 - 150416: the closure that forces X360 sizeof"
                      "(PhysicalTrafficManager) == 105648 and no other value");
        CGS_STATIC_ASSERT((44768 + KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER
                          + sizeof(((VehicleManager*)0)->maNonPhysicalContacts)
                          + sizeof(((VehicleManager*)0)->miNonPhysicalContactCount)
                          + sizeof(((VehicleManager*)0)->mPadAfterNonPhysicalContacts) == 160672),
                      "and it closes on the X360's own mDiscardedContacts seat");

        // ---- the two master gates (DWARF :865/:866) -------------------------------------------

        // ---- the 44-float tuning run (DWARF :868..:920) ----------------------------------------
        // Head, tail, the two retyped/renamed seats in the middle, and the closure. The closure is
        // the real test: 171468 + 44*4 == 171644 only holds if EVERY float in between is declared
        // and none of the old padding runs survived.

        // ---- per-car impact bookkeeping (DWARF :923..:934) -------------------------------------
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mafNoImpactTimeSeconds) == 32),
                      "8 x f32: HandleRaceCarRaceCarContact seeds it with lfsx/stfsx, so it is a FLOAT array");

        // ---- the per-car vulnerability / grinding arrays (DWARF :937..:951) --------------------
        // These two bases are what demote the committed scalars `mfGrindingThresholdA` (@171868)
        // and `mfGrindingThresholdB` (@171900) to ELEMENT 7 of two per-car arrays.

        // ---- the spare AI driver and the run that closes onto mePlayerActiveRaceCarIndex -------

        // ---- the six world/traffic crash thresholds (DWARF :962..:968) -------------------------

        CGS_STATIC_ASSERT((offsetof(VehicleManager, mCameraMatrix) % 16 == 0),
                      "stvx128 requires 16-alignment -- a compiler-inserted pad here would fault at runtime");

        // ---- the 16 gameplay/debug bools (DWARF :972..:988) ------------------------------------

        // ---- the player/car stat block (DWARF :993..:1007) -------------------------------------
        // The store opcodes are the type proof: stfsx for the two floats, stwx for the five words.

        // ---- the eight car-stat strength scalars (DWARF :1015..:1023) --------------------------

        // ---- the cached car-vs-car prediction (DWARF :1026..:1029) -----------------------------
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mCachedCarCarPredictionNormal) % 16 == 0),
                      "loaded/stored with lvx128/stvx128 -- must be 16-aligned");
        // The pointer-width decision this span depends on. If muCachedCarA/BSlot are ever widened
        // to real 8-byte pointers, the assert above is what will catch it -- do not "fix" it by
        // moving the normal.

        // ---- the tail (DWARF :1032..:1088) ------------------------------------------------------
        // the two contact-generation
        // pointers carved from the head of the old +172465..+172580 opaque span (DWARF :1045/:1046;
        // asm seats FreeAllocations @0x8261BAE0 / StartVehicleTractionLineTests @0x82629CE0). The
        // extra +4/+8 terms are the HOST-ONLY widening inside the carve: 4 alignment bytes so the
        // 8-byte pointers seat on an 8 boundary, then each pointer is 4 bytes wider than the
        // console's. The growth is absorbed by the REMAINING opaque run of the same span, which is
        // why miContactStreamCounterA below keeps its seat -- that pair of asserts is the carve's
        // whole tripwire.
        // REBASED 2026-08-06 (big-five #2): the +172476..+172592 span is CARVED IN FULL now
        // (see the header banner over the block) -- five real PrimitivePairListBuilders, nine
        // pointers, the BitArray<64> and the two renamed tail members (miContactStreamCounterA/B's
        // role-neutral FLAG retired: the DWARF sequence pins +172580 as
        // miNumTrafficSphereWorldTests and +172584 as the console POINTER
        // mpTractionLineStreamProducer). The block's host growth is one more named drift term.
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mpTractionLineStreamProducer)
                          >  offsetof(VehicleManager, miNumTrafficSphereWorldTests)
                      && offsetof(VehicleManager, miNumSPUTractionLineTests)
                          >  offsetof(VehicleManager, mpTractionLineStreamProducer)),
                      "the DWARF :1072/:1075/:1076 tail order");

        // ---- shape guards the offsets alone cannot catch ---------------------------------------
        // `sizeof` is permutation-blind and tail padding can absorb a grown array, so pin the END of
        // the data region as well as the seats inside it (the standing rule from the wave that lost
        // a 4->8 array growth to tail padding).
        // AND PIN THE END EXACTLY, not just `>=`. The standing rule from the wave that lost a
        // 4 -> 8 array growth to tail padding: a `>=` on sizeof cannot see a trailing member that
        // grew into slack. 172616 + (-1440) == 171176, rounded up to this class's 16-alignment
        // == 171184 (MEASURED).
        // THIS ASSERT HAS NOW EARNED ITS KEEP TWICE. It was written as 172816 (the one-drift-term
        // number) and was the ONLY thing that fired when the debug component was un-pinned; it was
        // 172848 (the two-term number) and was one of two things that fired when the race-car array
        // was folded to the real RaceCarPhysics. Both times the correct response was the named drift
        // term above, not a bigger literal here -- and both times the literal moved only after the
        // term did.
        // THREE TIMES NOW: 172816 -> 172848 -> 171184 -> 167024, and every move followed a named
        // drift term, never the other way round. This one is the TrafficPhysics de-fork.
        // FOUR: 167024 -> 167104 (2026-08-06, the contact-generation block carve's +76 term;
        // 172616 + (-5600) + 76 == 167092 -> 16-aligned 167104).
        // FIVE: 167104 -> 172928 (2026-08-09, the 240-byte SimpleVehicleAttribs landed and BOTH
        // negative array terms vanished -- see BrnVehicleManager.h; 172616 + 300 == 172916 ->
        // 16-aligned 172928).
        // SIX: 172928 -> 173056 (2026-08-11, the create-drain wave). `mPadAA60[128]` became the
        // two real `CgsResource::ResourceHandle[8]` arrays the DWARF names there; the host handle is
        // 16 bytes against the console's 8, so 2*8*16 == 256 replaces 2*8*8 == 128 and every drift
        // term from the handle arrays on picks up +128 (KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK is
        // +428 once it is rebased on the split; 172616 + 428 == 173044 -> 16-aligned 173056). Same
        // rule as the five before it: the literal moved only AFTER
        // KU_HOST_DRIFT_AFTER_MODEL_HANDLES did -- and it fired the moment the split landed,
        // exactly as intended. Six moves, six named drift terms, never a bumped literal.
        // SEVEN: 173056 -> 173440 (2026-09-11, the per-car debug-component wave).
        // `maRaceCarDebugComponent[8][1024]` became the real `DebugComponent[8]`; the component is
        // 1072 bytes on the host against the console's 1024, so 8 * 48 == +384 and every term from
        // the array on picks it up (KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK is +812 once it is rebased
        // on the new term; 172616 + 812 == 173428 -> 16-aligned 173440). Seventh move, seventh named
        // drift term.
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maeImpactType) == 32), "EImpactType[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mauImpactScore) == 8), "uint8[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mafPlayerGrindingOtherDurationSeconds) == 32), "f32[8] -- NOT a scalar threshold");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mafOtherGrindingPlayerDurationSeconds) == 32), "f32[8] -- NOT a scalar threshold");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mStuckInCollisionTestCacheSphere) == 16), "one stvx128 == 16 bytes");

        // =========================================================================================
        // THE RECORD IS GONE -- FOLDED 2026-08-03. `maRaceCarVehicles` is
        // `BrnPhysics::Vehicle::RaceCarPhysics[8]`, the real class.
        //
        // WHAT STOOD HERE, and why it could not be kept. This block held ten
        // `offsetof(RaceCarVehicleRecord, ...) == <X360 in-record seat>` asserts -- the byte-pinning
        // gate for the stand-in. A host class does not reproduce console offsets, so after the fold
        // every one of those ten would be FALSE. Re-basing them to whatever the host happens to
        // produce would be the worst of both: a green gate that measures nothing. They are deleted,
        // and the seats they guarded are guarded elsewhere:
        //     RaceCarPhysics_layout_check.cpp   the +0x13F0..+0x1460 own block, closing on 5216
        //     VehiclePhysics_layout_check.cpp   +0x130..+0x720 and +0x720..+0x13F0
        // Both are MOUNTED, both assert CONSOLE ARITHMETIC over X360Layout literals, and between
        // them every one of the ten former record fields is additionally named in a
        // `(void)offsetof(...)` existence check -- so a rename or a deletion still fails the build.
        //
        // WHAT IS STILL THIS FILE'S JOB is the claim the fold actually makes about THIS class: that
        // the element is the real type, that its host size is the number the drift term is derived
        // from, and that the array still starts on the asm-literal +1856 at 16 alignment. Those are
        // checkable and they are checked. Tamper-tested: changing either constant fires.
        // =========================================================================================
        // RE-STATED 2026-08-11 (create-drain wave). This assert used to read
        //     8 * (5216 - sizeof(RaceCarPhysics)) == -KU_HOST_DRIFT_AFTER_RACECAR_ARRAY
        // i.e. it assumed the race-car array was the ONLY thing in that drift term's range. It is
        // not any more: the two resource-handle arrays at +43616 landed the same day. They are a
        // SEPARATE, independently-derived component, so they get their own term
        // (KU_HOST_DRIFT_AFTER_MODEL_HANDLES) and their own assert -- see the model-handle block
        // higher up this file -- and what is checked HERE is the race-car array's own contribution
        // in isolation. Two components, two constants, two gates: neither can absorb an error in
        // the other, and a compensating pair cannot pass.
        // The two components, separately, so a compensating pair of errors cannot pass the sum.
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarModelHandles) == 43616),
                      "⭐ the split must NOT move the first handle array: 43616 == 8 * 0x154C is "
                      "ProcessCreateEvents' own asm literal, and nothing before it drifts");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarGraphicsModelHandles)
                          == offsetof(VehicleManager, maRaceCarModelHandles)
                             + sizeof(((VehicleManager*)0)->maRaceCarModelHandles)),
                      "the two arrays abut (console 8 * 0x154C then 8 * 0x1554, 64 bytes apart)");
        CGS_STATIC_ASSERT((__alignof(RaceCarPhysics) == 16),
                      "the array element must stay 16-aligned so element 0 keeps the asm-literal "
                      "+1856 base (1856 % 16 == 0)");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarVehicles) == 1856),
                      "⭐ the fold must NOT move element 0: 1856 is the asm literal "
                      "VehicleManager::Construct hands VehiclePhysics::Construct "
                      "(`addi r3, r29, -0x140D`)");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarEntityIDs)
                          == offsetof(VehicleManager, maRaceCarVehicles) + 8 * sizeof(RaceCarPhysics)),
                      "the eight-car array must abut maRaceCarEntityIDs with no slack -- the same "
                      "closure the X360's 1856 + 8*5216 == 43584 has");
        CGS_STATIC_ASSERT((sizeof(RaceCarCrashData) == 12), "RaceCarCrashData stride (asm: 12)");

        // The eight-car loop bound VehicleManager::Construct uses must BE the width of every
        // per-car array it walks. The 8 is asm-literal (`li r23, 8`); this pins that the constant
        // and the arrays cannot drift apart, which is the only way that loop could run off an end.
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarVehicles) / sizeof(RaceCarPhysics)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "maRaceCarVehicles[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarDrivers) / sizeof(VehicleDriver)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "maRaceCarDrivers[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarEntityIDs) / sizeof(EntityId)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "maRaceCarEntityIDs[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarHandlingBodyIDs) / sizeof(u64)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "maRaceCarHandlingBodyIDs[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maeRaceCarTypes) / sizeof(BrnWorld::ERaceCarType)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "maeRaceCarTypes[8]");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mauNetworkCarHiddenFramesRemaining) / sizeof(u32)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "mauNetworkCarHiddenFrames[8]");
        // AND the per-car debug components. There is no span and no cast any more: this is a real
        // array of the real class, so what is checkable here is the count, and that the element's
        // host size is exactly the number the array's drift term is derived from. Change either and
        // this fires -- which is how the term is kept from going stale.
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarDebugComponent) / sizeof(DebugComponent)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS), "maRaceCarDebugComponent[8]");
    }
}
}
