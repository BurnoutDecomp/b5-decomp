#pragma once

// CgsPhysics::PhysicsSimulationModule (DWARF CgsPhysicsSimulationModule.h:282)
// and its three fixed-capacity slot tables -- RigidBodyData (200), JointData
// (36) and DriveData (1) -- each a struct-of-arrays whose entries are indexed by
// slot.
//
// The rw::physics element payloads (Joint / JointFrames / Drive / DriveFrames)
// are modelled here only as correctly-sized, alignment-faithful opaque spans:
// every byte offset / array stride is X360-attested (off the slot accessors' asm
// and the DWARF field map), but their interior field layout is not consumed by
// the functions homed here, so it is intentionally not invented. JointLimits,
// Inertia and DriveDynamics ARE modelled field-by-field, because the slot
// tables' own constructors (X360 @0x827DB798 / @0x827DB728 and the DriveData
// init inlined into the module constructor @0x827DF1E0) default-initialise each
// entry BY MEMBER. Inertia is modelled in rw/physics/inertia.h and merely ALIASED
// here (task #141); JointLimits and DriveDynamics are still declared below.
//
// X360 byte offsets in the comments are the console's 4-byte-pointer ABI. The
// host gate builds x64, so absolute offsets past a pointer array widen; every
// access here is BY NAME, and the layout gate in the .cpp asserts the
// pointer-width-independent relationships (see _AssertLayout).

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<200>
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // the module base class

// ⭐ THE REAL CgsPhysics::RigidBodyId, by value -- RigidBodyData embeds 200 of them and this
// header no longer carries a copy (task #141). Cheap and cycle-free: CgsRigidBody.h is
// header-only and reaches nothing but types.hpp + CgsEntityId.h.
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"

// ⭐ THE REAL rw::physics::Inertia, by value -- `CgsPhysics::Inertia` is now an alias onto it
// (see the block below) and RigidBodyData embeds 200 of them, so a forward declaration will
// not do. Safe to pull in here where CgsPhysicsSimulationIO_Events.h is not: inertia.h reaches
// only types.hpp, rw/math/vpu/types.h, <cfloat> and <cstddef>, so it drags no CgsPhysics type
// into this header's ~30 includers. (This line used to end "...and cannot make the open
// RigidBodyId ODR fork meet". There is no open RigidBodyId fork any more -- see the block at
// the RigidBodyId note below, retired the same day.)
#include "rw/physics/inertia.h"
// task #143: the DriveFrames/DriveDynamics fork retired below aliases onto these two, so the
// real definitions have to be visible here rather than forward-declared.
#include "vendor/renderware/physics/DriveFrames.hpp"    // rw::physics::DriveFrames   (64B)
#include "vendor/renderware/physics/DriveDynamics.hpp"  // rw::physics::DriveDynamics (32B)
// task #144: same for the JointFrames/JointLimits pair retired below.
#include "vendor/renderware/physics/JointFrames.hpp"    // rw::physics::JointFrames   (80B)
#include "vendor/renderware/physics/JointLimits.hpp"    // rw::physics::JointLimits   (64B)

// rw::physics pointer members of PhysicsSimulationModule. Forward declarations
// only -- the module never dereferences them in the functions homed here. Class
// keys match their real homes (vendor/renderware/include/rw/physics/{simulation,
// pairset}.h), so no ODR/class-key mismatch.
namespace rw { namespace physics { class Simulation; class PairSet; class Joint; class Drive; struct RigidBody; } }

// The per-frame input buffer the nineteen Process*Queue drains read. Forward-declared only:
// CgsPhysicsSimulationModuleIO.h includes CgsPhysicsSimulationIO_Events.h, and this header keeps
// that chain out of its ~30 includers on COMPILE-COST grounds alone. ⚠️ The reason recorded here
// until 2026-08-04 -- "it would drag CgsRigidBody.h's RigidBodyId in and detonate the open ODR
// fork with the struct declared below" -- died with that struct (task #141); this header now
// includes CgsRigidBody.h itself. The .cpp includes the full definition.
// ⚠️ CLASS KEY IS `struct`, matching CgsPhysicsSimulationModuleIO.h.
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InputBuffer; } }

// The abstract resource allocator PhysicsSimulationModule::Prepare carves the simulation,
// its workspace and the three pair sets out of.
// ⚠️ THE CLASS KEY IS `struct`, matching rw/rwcore_structs.h. A `class` here and a `struct`
// there mangles into a different symbol and produces an LNK2019 naming something that looks
// character-for-character identical in the log.
namespace rw { struct IResourceAllocator; }

namespace CgsPhysics
{
    // ---- rw::physics element payloads -------------------------------------
    // Sizes are X360-attested array strides (DriveData/JointData accessor asm +
    // the DWARF field map): JointFrames 80B, DriveFrames 64B, DriveDynamics 32B.
    // Interior layout not consumed here -> opaque sized spans (declared-only
    // element interiors, flagged).

    // ⚠️⚠️ THE **EIGHTH AND NINTH** TYPE FORKS IN THIS SUBSYSTEM WERE RETIRED HERE ON
    // 2026-08-04 (task #144), and both were LIVE. `JointFrames` was declared here as a bare
    // `u8 macOpaque[80]` and `JointLimits` (below) as a second, re-spelled copy of
    // rw::physics::JointLimits carrying its own `mafPprism`/`mfVtwist`/`meSwingf` fields and
    // its own `E_SwingType`/`E_TwistType`. Exactly the DriveFrames/DriveDynamics pair task
    // #143 retired one group over, and they survived for the same reason: nothing had yet
    // moved a VALUE across the seam.
    //
    // ⚠️⚠️ BOTH WERE ALREADY LOAD-BEARING, on the solver's own read path:
    //   * JointData::maFrames[] / maLimits[] hold these types;
    //   * ProcessAddJointQueue points Joint::m_skel / Joint::m_limit straight at those slots;
    //   * JointJacobian_Build.cpp:74-75 then dereferences them as the **rw::physics** types --
    //     `const JointFrames& lrF = *lrJoint.m_skel;` -- every tick the solver runs.
    // Two definitions of two records, the solver reading through one and the table writing the
    // other. Byte-identical, so correct BY LUCK, exactly as the Inertia and Drive forks were.
    //
    // ⚠️⚠️ AND THE SIGNATURE THAT CARRIES THEM HIDES THEM: rw::physics::Simulation::AddJoint
    // takes `void* lpFrames, void* lpLimits`, so a forked pointer converts IMPLICITLY and
    // SILENTLY at the one call site that matters. ⇒ a `void*` in a reconstructed signature is
    // a fork detector switched off; see [[odr-forks-link-silently]].
    //
    // ⭐⭐ WHAT SETTLED IT IS NOT STYLE BUT THE SHIPPED BINARY. ProcessAddJointQueue @0x828A40F0
    // carries ELEVEN validation asserts whose baked literals name the accessors outright --
    // `RwMathVPU::IsValid( lpFrames->GetChildAngularFrame() )`,
    // `RwMathVPU::IsValid( lpLimits->GetAngularVelocityLimit() )`, and nine more. Every one of
    // those accessors exists on the rw::physics classes and NOT ONE exists on the copies that
    // used to be declared here. The DWARF agrees independently: InAddJoint's payload members
    // are typed `NonConstructedClassContainer<rw::physics::JointFrames>` /
    // `<rw::physics::JointLimits>` (CgsPhysicsSimulationModuleIO.h:259-260).
    //
    // The aliases are the whole fix -- every existing `JointFrames`/`JointLimits` spelling
    // inside CgsPhysics keeps working and now names ONE type. The X360-attested 80/64-byte
    // strides are asserted at the definitions they are a claim about
    // (vendor/renderware/physics/JointFrames.hpp, JointLimits.hpp) and re-pinned in JointData's
    // own offsetof block.
    typedef ::rw::physics::JointFrames JointFrames;
    typedef ::rw::physics::JointLimits JointLimits;

    // ⚠️⚠️ THE **SEVENTH** TYPE FORK IN THIS SUBSYSTEM WAS RETIRED HERE ON 2026-08-04
    // (task #143), and it was LIVE. `DriveFrames` and `DriveDynamics` were declared HERE as a
    // second, cut-down copy of the real rw::physics classes -- DriveFrames as a bare
    // `u8 macOpaque[64]`, DriveDynamics with its own re-spelled `mfSpring`/`meType` fields and
    // its own `E_DriveType`. Same shape as the forks tasks #135 (Joint/Drive/RigidBody) and
    // #141 (Inertia) retired, and it had survived only because nothing had yet moved a VALUE
    // across the seam.
    //
    // ⚠️⚠️ IT WAS ALREADY LOAD-BEARING WHEN IT WAS FOUND, and the path is a live one:
    //   * DriveData::maFrames[] / maScaledDynamics[] hold these types;
    //   * ProcessAddDriveQueue points Drive::m_skel / Drive::m_crtl straight at those slots;
    //   * DriveJacobian_Build.cpp:113-114 then dereferences them as the **rw::physics**
    //     types -- `const DriveFrames& lrF = *lrDrive.m_skel;` -- every tick the solver runs.
    // Two definitions of one record, the solver reading through one and the table writing the
    // other. Byte-identical, so it was correct BY LUCK, exactly as the Inertia fork was.
    //
    // ⚠️⚠️ AND THE SIGNATURE THAT CARRIES IT HIDES IT: rw::physics::Simulation::AddDrive takes
    // `void* lpFrames, void* lpDynamics`, so the forked pointer converts IMPLICITLY and
    // SILENTLY at the one call site that matters. The compiler only ever spoke because
    // ProcessUpdateDriveFramesQueue does a whole-object ASSIGNMENT. ⇒ a `void*` in a
    // reconstructed signature is a fork detector switched off; see [[odr-forks-link-silently]].
    //
    // The aliases below are the whole fix -- every existing `DriveFrames`/`DriveDynamics`
    // spelling inside CgsPhysics keeps working and now names ONE type. The X360-attested
    // 64/32-byte strides are asserted at the definitions they are a claim about
    // (vendor/renderware/physics/DriveFrames.hpp, DriveDynamics.hpp) and re-pinned in
    // DriveData's own offsetof block.
    typedef ::rw::physics::DriveFrames   DriveFrames;
    typedef ::rw::physics::DriveDynamics DriveDynamics;

    // ⭐ 2026-08-04 (task #144): `JointFrames` and `JointLimits` -- the two forks this note used
    // to leave open for "the wave that writes ProcessAddJointQueue" -- are RETIRED, by that
    // wave, with the alias + moved-pins mechanism above. Nothing here is still forked.

    // ⚠️ WAS A TYPE FORK UNTIL 2026-08-04 (task #141). This spelled out its OWN copy of
    // rw::physics::Inertia's seven fields (`f32 mafInvTens[4]` + six named scalars), pinned by
    // seven offsetof asserts of its own in the .cpp, and PhysicsSimulationModule::Process-
    // AddRigidBodyQueue reinterpret_cast'ed BETWEEN the two copies in both directions -- one
    // cast into RigidBodyData::AddBody, one back out of GetInertia into Simulation::
    // AddRigidBody, which stores the pointer in RigidBody::mInertia for DynamicUpdate to read
    // every tick. Same shape as the Joint/Drive/RigidBody fork task #135 retired, and by the
    // time it was spotted it was already LOAD-BEARING.
    //
    // ⚠️⚠️ WHY THAT WAS DANGEROUS RATHER THAN MERELY UNTIDY: `Inertia` is a class TYPE, and a
    // mangled name encodes neither a class's layout nor its bases. Two definitions of one
    // record therefore produce call sites and bodies that LINK CLEANLY against each other and
    // disagree about where every field lives. There is no diagnostic -- not from the compiler,
    // not from the linker, not from a per-TU compile gate. One of the forks #135 retired would
    // have allocated 16 bytes for a 2032-byte IO buffer. A SYMBOL RESOLVING CAN ITSELF BE THE
    // BUG. The alias below is the whole fix; the layout pins moved to the definition they gate
    // (rw/physics/inertia.h, _rw_physics_Inertia_AssertLayout).
    //
    // The X360-attested array stride (48; GetInertia @0x8289D0C0 indexes maInertias at
    // 48*(idx+50)) is asserted there, next to the members it is a claim about.
    typedef ::rw::physics::Inertia Inertia;

    // Opaque handle tables -- single u64 ids / single pointers; sized to the
    // DWARF (JointId/DriveId == uint64_t).
    struct JointId { u64 muId; };
    struct DriveId { u64 muId; };

    // ⚠️ THE SECOND TYPE FORK RETIRED ON 2026-08-04 (task #141, right behind Inertia).
    // `CgsPhysics::RigidBodyId` and its `K_INVALID_RIGID_BODY_ID` sentinel were declared HERE
    // as a second, cut-down copy (struct, PUBLIC mId, `extern const` sentinel defined in the
    // .cpp) alongside the real one in CgsRigidBody.h (class, PRIVATE mId, `static const`
    // sentinel). THREE committed headers documented it as open and none could resolve it,
    // because each of them could only see half the problem:
    //   * CgsPhysicsSimulationIO_Events.h (the InAddJoint / InAddRigidBody handle notes) --
    //     "an open ODR fork ... including both is a hard C2011", and typed its own
    //     mID/mParentBodyId/mChildBodyId as raw u64 to dodge it;
    //   * BrnPhysicsModule.h (the mWorldRigidBodyId note) -- deliberately did NOT include
    //     CgsRigidBody.h "because of it", and called PhysicsModule "the closest any TU has
    //     come to making them meet";
    //   * this header (the InputBuffer forward-decl note above) -- must not pull in the
    //     events header for the same reason.
    // ⚠️ Cross-references here are deliberately BY NOTE, not by line number: the previous set
    // of line cites had all drifted, and a stale cite is how the false "JointId is forked"
    // claim (corrected in the same commit) got believed and copied in the first place.
    // The C2011 was never the disease. It was the ONE diagnostic the fork could still produce:
    // unlike Inertia, these two differ in CLASS KEY, so MSVC mangles them apart (U vs V) and a
    // cross-copy call would have been an LNK2019 rather than a silent miscompile. Deleting the
    // copy removes the fork AND the reason nobody could include CgsRigidBody.h.
    //
    // ⚠️ THE COPY WAS NOT LAYOUT-WRONG, SO NOTHING WOULD HAVE CAUGHT IT DRIFTING. Both spell a
    // single u64 and both sentinels are 0xFFFFFFFFFFFFFFFF, so every seat, every `std`, and the
    // sizeof==8 gate agreed -- which is exactly why it survived three headers' worth of review.
    // Reconstruct against the real class from here on; do not re-declare it locally.
    //
    // On the `static const` vs `extern const` question BrnPhysicsModule.h's fork note left open:
    // the console settles it by emitting a PER-TU copy of the sentinel (qword_82F33E18 in the
    // sim-module TU, qword_82F2A3A8 in the vehicle-manager TU), which is what CgsRigidBody.h's
    // header-scope `static const` produces. The `extern const` here was the invention.

    // ⚠️ WAS A TYPE FORK UNTIL 2026-08-04 (task #135). This used to declare THREE OF ITS OWN
    // opaque `CgsPhysics::rw_physics::{Joint,Drive,RigidBody}` structs, distinct from the real
    // rw::physics types the slot arrays actually hold. That was invisible while nothing in
    // this TU ever passed a slot pointer to rw::physics -- and it stopped being invisible the
    // moment AllocateMemoryAndInitialiseRW landed, which hands exactly those pointers to
    // Simulation::RemoveJoint / RemoveDrive / RemoveRigidBody. A namespace ALIAS onto the real
    // types keeps every existing `rw_physics::X` spelling working and removes the fork; the
    // class keys below match their real homes (Joint/Drive are `class`, RigidBody is `struct`),
    // because a key mismatch mangles into a different symbol and produces an LNK2019 naming
    // something that looks character-for-character identical in the log.
    namespace rw_physics = ::rw::physics;

    // ⚠️ THE `E_SwingType` / `E_TwistType` / `JointLimits` TRIO THAT USED TO STAND HERE WAS THE
    // NINTH TYPE FORK -- see the retirement block above. The enums were a re-spelling of
    // rw::physics::SwingType / TwistType (identical values; the 5-way and 3-way dispatches at
    // X360 0x82BC470C / on `lwz 0x3C(r28)` ARE those enums), and the struct a re-spelling of
    // rw::physics::JointLimits with `mafPprism[4]` in place of a Vector3. Both now alias the
    // real definitions in vendor/renderware/physics/JointLimits.hpp, where the layout pins live.
    typedef ::rw::physics::SwingType E_SwingType;
    typedef ::rw::physics::TwistType E_TwistType;

    // ---- JointData (DWARF CgsPhysicsSimulationModule.h:132) ----------------
    // knSize == 36. Struct-of-arrays slot table; the accessors index it by slot.
    class JointData
    {
    public:
        static const s32 KI_SIZE = 36;

        // Default-construct every maLimits[] entry (X360 @0x827DB798): a 36-pass
        // loop over &maLimits[0] (== this+0xB40) that, per 64-byte record, zeroes
        // mPprism, mVprism, mfVtwist..mfTwista and the two enum slots, and sets
        // mfSwingc/mfTwistc to 1.0f. No other slot array is touched.
        JointData();

        // DWARF CgsPhysicsSimulationModule.h:137. No out-of-line symbol exists --
        // the console inlines it; its one call site is PhysicsSimulationModule::
        // Construct @0x828A1EE8, whose 36-pass `stb 0` loop over this+0x4700
        // (== mJointData + 0x15F0 == &mabUsedSlot[0]) IS this body, and touches
        // nothing else.
        void Clear();

        // Checked slot accessor (X360 @0x8289D168): asserts liIndex < knSize and
        // mabUsedSlot[liIndex], then returns maRWJoints[liIndex].
        rw_physics::Joint* GetJoint(s32 liIndex);

        // The raw slot-occupancy probe. X360-ATTESTED READ, [INFERRED NAME]: the console
        // inlines it into PhysicsSimulationModule::AllocateMemoryAndInitialiseRW as
        // `if (*(mJointData_mabUsedSlot + i))` at 0x828A2560, so the read is not in doubt;
        // only the spelling is, and it is taken from the DWARF-attested sibling on
        // RigidBodyData (CgsPhysicsSimulationModule.h:113).
        bool IsSlotUsed(s32 liIndex) const;

        // ---- task #144: THIS CLASS WAS A HOLLOW SHELL TOO --------------------------------
        // ⚠️ The DWARF (CgsPhysicsSimulationModule.h:137..:177) declares TEN methods on
        // JointData; this tree declared THREE of them, and the seven missing were -- exactly as
        // with DriveData one wave earlier -- the write path plus the two slot accessors the
        // joint drains call. ⚠️ `grep -c "JointData::"` scored **6**, so the "a shell scores 1"
        // heuristic does not fire here either. What catches it is diffing the class against the
        // DWARF METHOD LIST.
        //
        // Still deliberately absent, because nothing in scope calls them and no body is decoded:
        // GetIndexFromJoint (h:156), SetTimeStep (h:161). Declaring them without bodies would
        // trade one hollow shell for another. (GetGameID left this list on 2026-08-05 -- see
        // its declaration below; the rigid-body remove drain decoded its inlined body.)

        // DWARF h:165. ⭐ ADDED 2026-08-05 (the rigid-body drain group): a decoded body and a
        // caller now exist -- ProcessRemoveRigidBodyQueue's "Removing jointed body" diagnostic
        // inlines it at 0x828A30B0..0x828A3108 as the checked maGameIDs[liIndex] read behind
        // asserts h:612 (`liIndex < knSize`, `cmpwi r24,0x24`) / h:613 (`mabUsedSlot[liIndex]`,
        // `lbz 0x15F0`). ⭐ Those two lines complete the accessor-assert arithmetic this class
        // already recorded -- 612/613, then GetJoint 621/622, GetJointFrames 630/631,
        // GetJointLimits 639/640: four checked accessors, nine header lines apart each.
        JointId GetGameID(s32 liIndex);

        // DWARF h:144. X360 @0x8289D3E0 (98 instructions), sole caller ProcessAddJointQueue.
        // ⭐ NOT the DriveData::AddDrive shape, despite the symmetry everywhere else: this one
        // opens with a full 36-slot DUPLICATE-DETECTION pre-pass that asserts, for every live
        // slot, `maGameIDs[li] != lGameID` and `maRWJoints[li] != lpJoint`. AddDrive (62 insn)
        // has no such pass -- that is most of the 98-vs-62 gap. Mirroring the drive twin would
        // have dropped it silently.
        // ⚠️ FOUR parameters, no timestep. DriveData::AddDrive takes a fifth `float32_t` that it
        // accepts and never reads (the defect flagged in task #143); the joint side never had it.
        s32 AddJoint(rw_physics::Joint* lpJoint, const JointFrames& lrFrames,
                     const JointLimits& lrLimits, JointId lGameID);

        // DWARF h:148. No out-of-line symbol -- inlined into ProcessRemoveJointQueue at
        // 0x8289FBE4 as the assert at .cpp:2696 followed by the single `stb 0, 0x4700(r15+idx)`
        // (== mJointData + 0x15F0 == &mabUsedSlot[idx]). It frees the SLOT only; the rw Joint
        // is returned to the simulation's own free list by Simulation::RemoveJoint separately.
        bool RemoveJoint(s32 liIndex);

        // DWARF h:152. No out-of-line symbol -- the console inlines it into all five joint
        // drains as the same linear search, and all five copies agree: walk k = 0..35, skip
        // unless mabUsedSlot[k], compare maGameIDs[k] to the id with a 64-bit `cmpld`, return k
        // on a hit and -1 on exhaustion.
        s32 GetIndexFromGameID(JointId lGameID) const;

        // Checked slot accessors. No out-of-line symbols -- both are inlined at every call site,
        // but their asserts identify them unambiguously by line: GetJointFrames carries h:630 /
        // h:631 and GetJointLimits h:639 / h:640, against GetJoint's h:621 / h:622 (which the
        // out-of-line GetJoint @0x8289D168 confirms directly).
        JointFrames* GetJointFrames(s32 liIndex);
        JointLimits* GetJointLimits(s32 liIndex);

        // [INFERRED NAME] -- the write-back half of GetJoint, on exactly the same footing as
        // DriveData::SetDrive (task #143) and RigidBodyData::SetRigidBody before it: the STORE
        // is X360-attested (ProcessAddJointQueue @0x828A497C, `stwx r3,(idx+0x1154)<<2,r23`,
        // 4*0x1154 == 0x4550 == mJointData + 0x1440 == &maRWJoints[idx]) even though the DWARF
        // carries no such method. Named here rather than reaching into the array from outside.
        void SetJoint(s32 liIndex, rw_physics::Joint* lpJoint);

        // Offsets below are X360-ABI (4-byte pointer) byte offsets; on the x64
        // host gate the pointer-array slots widen, but member access is by name.
    private:
        JointFrames        maFrames[36];     // X360 @+0x0000 (stride 80)
        JointLimits        maLimits[36];     // X360 @+0x0B40 (stride 64)
        rw_physics::Joint* maRWJoints[36];   // X360 @+0x1440 (4B ptr)  -> 5184
        JointId            maGameIDs[36];    // X360 @+0x14D0 (stride 8)
        bool               mabUsedSlot[36];  // X360 @+0x15F0            -> 5616
    };

    // ---- DriveData (DWARF CgsPhysicsSimulationModule.h:197) ----------------
    // knSize == 1. Struct-of-arrays slot table; the accessors index it by slot.
    class DriveData
    {
    public:
        static const s32 KI_SIZE = 1;

        // Default-construct every maDynamics[]/maScaledDynamics[] entry. Inlined by
        // the console into PhysicsSimulationModule::PhysicsSimulationModule
        // @0x827DF1E0 (0x827DF250..0x827DF294), which zeroes the two 32-byte
        // records at this+0x4770 and this+0x4790 -- i.e. mDriveData+0x40 and +0x60
        // -- field by field: three `stfs 0.0f` + one `stw 0` per Params, twice per
        // record. maFrames / maRWDrives / maGameIDs / mabUsedSlot are NOT touched.
        DriveData();

        // DWARF CgsPhysicsSimulationModule.h:202. No out-of-line symbol -- inlined
        // into PhysicsSimulationModule::Construct @0x828A1EE8 as the single
        // `stb 0, 0x47C0(r31)` (== mDriveData + 0x90 == &mabUsedSlot[0]).
        void Clear();

        // Checked slot accessor (X360 @0x8289D1E8): asserts liIndex < knSize and
        // mabUsedSlot[liIndex], then returns &maFrames[liIndex].
        DriveFrames* GetDriveFrames(s32 liIndex);

        // Checked slot accessor (X360 @0x8289D268): asserts liIndex < knSize and
        // mabUsedSlot[liIndex], then returns &maScaledDynamics[liIndex].
        DriveDynamics* GetScaledDriveDynamics(s32 liIndex);

        // DWARF CgsPhysicsSimulationModule.h:241. The console inlines it into
        // AllocateMemoryAndInitialiseRW as the bare `lwz` of maRWDrives[0] at 0x828A25CC
        // (no assert -- the slot-used test guarding it IS the tripwire), so the body is
        // the raw slot read.
        rw_physics::Drive* GetDrive(s32 liIndex);

        // Same provenance as JointData::IsSlotUsed above -- X360-attested read
        // (`if (*(mDriveData_mabUsedSlot))` at 0x828A25C0), name from RigidBodyData.
        bool IsSlotUsed(s32 liIndex) const;


        // ---- task #143: THIS CLASS WAS A HOLLOW SHELL -------------------------------------
        // ⚠️ The DWARF (CgsPhysicsSimulationModule.h:197..:255) declares TWELVE methods on
        // DriveData; this tree declared SIX, and the six missing were exactly the write path --
        // i.e. exactly what the five drive drains call. That is invisible to every per-TU
        // compile gate, because a class that declares nothing is not a class that is wrong;
        // it only fails when someone tries to WRITE the drain. Caught by diffing the class
        // against the DWARF method list, NOT by `grep -c` (which scores 6 here, not 1).
        //
        // Still deliberately absent, because nothing in scope calls them and their bodies are
        // not decoded: GetIndexFromDrive (h:222), SetTimeStep (h:227),
        // ScaleOneDriveForTimeStep (h:233), GetGameID (h:237). Declaring them without bodies
        // would trade one hollow shell for another.

        // DWARF h:210. X360 @0x828A0330 (62 instructions). Claims the first free slot and
        // fills it; returns the slot index, or -1 when every slot is in use. The `Drive*`
        // argument is NULL at its only call site -- the slot is claimed BEFORE the
        // rw::physics drive exists, exactly as RigidBodyData::AddBody does.
        //
        // ⚠️⚠️ THE `lfTimeStep` PARAMETER IS ACCEPTED AND NEVER READ. There is not one lfs /
        // stfs / fmuls in the 62-instruction body and the only `bl` is the save/restore pair,
        // so ScaleOneDriveForTimeStep is NOT called and maScaledDynamics receives an
        // UNSCALED copy of lrDynamics. The caller does compute the value
        // (`lfs f1, 0xA0(mpSimulation)` at 0x828A4DD8) and passes it. This is reconstructed
        // as it shipped; inventing the multiply the name implies would be fabricating
        // physics, which is the one thing this subsystem must not do.
        s32 AddDrive(rw_physics::Drive* lpDrive, const DriveFrames& lrFrames,
                     const DriveDynamics& lrDynamics, DriveId lId, f32 lfTimeStep);

        // DWARF h:214. No out-of-line symbol -- the console inlines it into
        // ProcessRemoveDriveQueue @0x828A00A8..0x828A00D0 as the `liIndex < knSize` assert
        // (.cpp:2803 -- note the RELATIVE path string, which is how you tell a .cpp-defined
        // inline from a header one here) followed by the single `stb 0` at this+0x47C0
        // (== mDriveData + 0x90 == &mabUsedSlot[0]). It touches nothing else: the Drive*
        // slot is deliberately left dangling, as the console leaves it.
        bool RemoveDrive(s32 liIndex);

        // DWARF h:218. No out-of-line symbol -- inlined identically into FOUR drains
        // (@0x8289FC9C, @0x828A0008, @0x8289FDC8, @0x8289FEF0). Linear scan of the slot
        // table returning the first index whose slot is in use AND whose game id matches,
        // or -1. Both id reads are 8-byte `ld`, which is what pins DriveId at 8 bytes.
        s32 GetIndexFromGameID(DriveId lId) const;

        // DWARF h:249. Checked slot accessor for the UNSCALED dynamics -- the sibling of
        // GetScaledDriveDynamics above, and a different array (+0x40, not +0x60).
        // Inlined into ProcessUpdateDriveDynamicsQueue @0x8289FE44 as `(liIndex+2)<<5`
        // (== mDriveData + 0x40 + 32*liIndex) behind the same two asserts, at h:675/676.
        DriveDynamics* GetDriveDynamics(s32 liIndex);

        // [INFERRED NAME], on exactly the same footing as RigidBodyData::SetRigidBody below
        // and for the same reason. ProcessAddDriveQueue @0x828A4EB0 writes the live drive
        // pointer into this table itself (`addi r11,r30,0x11EC` ; `slwi r11,r11,2` ;
        // `stwx r3,r11,r29` == this + 0x47B0 + 4*liIndex == mDriveData.maRWDrives[liIndex] at
        // the console's 4-byte stride), so the WRITE is not in doubt; only its spelling is.
        // The DWARF's method list does not carry it, which is what a fully-inlined one-line
        // setter looks like. ⛔ Do NOT reproduce the index arithmetic: the slot is 8 bytes here.
        void SetDrive(s32 liIndex, rw_physics::Drive* lpRWDrive);

        // Offsets below are X360-ABI (4-byte pointer) byte offsets; on the x64
        // host gate the pointer-array slot widens, but member access is by name.
    private:
        DriveFrames        maFrames[1];          // X360 @+0x00 (stride 64)
        DriveDynamics      maDynamics[1];        // X360 @+0x40 (stride 32)
        DriveDynamics      maScaledDynamics[1];  // X360 @+0x60 (stride 32) -> 96
        rw_physics::Drive* maRWDrives[1];        // X360 @+0x80 (4B ptr)
        DriveId            maGameIDs[1];         // X360 @+0x88 (8-aligned)
        bool               mabUsedSlot[1];       // X360 @+0x90            -> 144
    };

    // ---- RigidBodyData (DWARF CgsPhysicsSimulationModule.h:64) --------------
    // knSize == 200. Struct-of-arrays slot table indexed by slot. Unlike
    // JointData/DriveData there is NO mabUsedSlot[] array: the "slot in use"
    // tripwire is `!maGameIDs[liIndex].IsInvalid()` (the X360 asm compares the
    // slot's GameID to K_INVALID_RIGID_BODY_ID). Member order + array element
    // types are DWARF-attested (CgsPhysicsSimulationModule.h:126-128).
    class RigidBodyData
    {
    public:
        static const s32 KI_SIZE = 200;                 // DWARF knSize (X360 cmpwi 0xC8)

        // X360 @0x827DB728 (0x70 bytes). ⚠️ This function is ABSENT from the
        // .ida-exports JSON set -- an export hole (0x827DB720 is LogOutput::Append,
        // whose 2-instruction body ends exactly here). Recovered by decoding the
        // .i64 directly. 200-pass loop, stride 48, over &maInertias[0]
        // (== this+0x960): it is rw::physics::Inertia::Inertia() (DWARF
        // inertia.h:80) inlined per entry, and it touches maRWBodies / maGameIDs
        // not at all.
        RigidBodyData();

        // DWARF CgsPhysicsSimulationModule.h:68. No out-of-line symbol -- inlined
        // into PhysicsSimulationModule::Construct @0x828A1EE8 as the 200-pass
        // `std K_INVALID_RIGID_BODY_ID` loop over this+0x550 (== mBodyData+0x320
        // == &maGameIDs[0]). Nothing else is written: there is no mabUsedSlot[]
        // here, the "slot in use" tripwire IS the game-id sentinel.
        void Clear();

        // Checked slot read of maGameIDs[liIndex] (X360 @0x8289CF78). Returned by
        // value; the X360 ABI passes a hidden return pointer for the 8-byte id.
        RigidBodyId GetGameID(s32 liIndex);

        // Checked slot read of maRWBodies[liIndex] (X360 @0x8289D020).
        rw_physics::RigidBody* GetRigidBody(s32 liIndex);

        // Checked slot read of &maInertias[liIndex] (X360 @0x8289D0C0).
        Inertia* GetInertia(s32 liIndex);

        // X360 @0x8289D2E8 (62 instructions). ⚠️ ABSENT from .ida-exports -- recovered from
        // BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3 (task #140), the same route that
        // produced RigidBodyData::RigidBodyData and PhysicsSimulationModule::Destruct.
        //
        // Claim the first free slot (the first maGameIDs[] entry still holding the invalid
        // sentinel), store the caller's live rw::physics body pointer and game id into it,
        // COPY the caller's inertia block into maInertias[slot], and return the slot index.
        // Its two callers are ProcessAddRigidBodyQueue @0x828A2AB8 and 0x821DBBC0.
        //
        // ⚠️ THE FREE-SLOT WALK HAS NO BOUND. The console's loop is
        //     for (i = 0; !maGameIDs[i].IsInvalid(); ++i) ;
        // with the `i < 200` test AFTER it, as a fire-and-continue assert -- so a full table
        // walks off the end of maGameIDs and into maInertias before anything complains. Kept
        // faithful; do not "fix" it into a bounded loop, that changes which slot is returned.
        // ⚠️ THE INERTIA IS TAKEN BY REFERENCE, NOT BY POINTER -- DWARF
        // CgsPhysicsSimulationModule.h:74 spells it `int32_t AddBody(RigidBody*, RigidBodyId,
        // const Inertia&)`, and the body copies it, so the reference is the whole point.
        s32 AddBody(rw_physics::RigidBody* lpRWBody, RigidBodyId lId, const Inertia& lrInertia);

        // X360-ATTESTED WRITE, [INFERRED NAME] -- same provenance convention as
        // JointData::IsSlotUsed above. ProcessAddRigidBodyQueue @0x828A2BA0 writes the live
        // body pointer into this table itself (`addi r11,r31,0x8C` ; `slwi r11,r11,2` ;
        // `stwx r3,r11,r29` == mBodyData.maRWBodies[liIndex] at the console's 4-byte stride),
        // so the WRITE is not in doubt; only its spelling is. The DWARF's method list for
        // RigidBodyData does not carry it, which is consistent with a fully-inlined one-line
        // setter. ⛔ Do NOT reproduce the console's index arithmetic: the slot is 8 bytes here.
        void SetRigidBody(s32 liIndex, rw_physics::RigidBody* lpRWBody);

        // DWARF CgsPhysicsSimulationModule.h:113 / :108. Both are inlined by the console
        // into AllocateMemoryAndInitialiseRW: IsSlotUsed is the `maGameIDs[i] !=
        // K_INVALID_RIGID_BODY_ID` test at 0x828A25FC, SetFree the write-back of the
        // sentinel at 0x828A26C0 (whose bounds tripwire carries the .cpp:2504 line, i.e.
        // it fires from inside this function's frame, which is what "inlined" means).
        bool IsSlotUsed(s32 liIndex) const;
        void SetFree(s32 liIndex);

        // DWARF h:82. X360 @0x828A0100 (140 instructions) -- the busiest member of this class,
        // with twelve call sites across the input drains (the t150 dump's xref set reproduces
        // the twelve exactly). Linear scan of maGameIDs[] for the handle; -1 on a miss. See the
        // .cpp for the three diagnostics and for why there is no used-slot test (this class has
        // no mabUsedSlot[]; a free slot holds the sentinel).
        //
        // ⚠️ Still absent, deliberately: RemoveBody (DWARF h:78), GetIndexFromRigidBody (h:86),
        // SetTimeStep (h:92) -- nothing in scope calls them and no body is decoded. ⭐ Re-proved
        // 2026-08-05 against the asm, not just re-cited: ProcessRemoveRigidBodyQueue and
        // ProcessRemoveAllRigidBodiesQueue DO NOT call RemoveBody -- both inline SetFree (the
        // .cpp:2504 assert + the sentinel store) after Simulation::RemoveRigidBody, so the
        // remove drains close with zero new RigidBodyData surface. ⚠️ The line numbers this
        // note used to carry (h:80/h:100/h:104, and h:96 above) were ALL drifted; the set here
        // is re-read from the DWARF dump directly (RemoveBody :78, GetIndexFromGameID :82,
        // GetIndexFromRigidBody :86, SetTimeStep :92, GetGameID :96, GetRigidBody :100,
        // GetInertia :104, SetFree :108, IsSlotUsed :113).
        s32 GetIndexFromGameID(RigidBodyId lId);

        // Offsets below are X360-ABI (4-byte pointer) byte offsets; on the x64
        // host the pointer-array slot widens, but member access is by name.
    private:
        rw_physics::RigidBody* maRWBodies[200];   // X360 @+0x0000 (4B ptr)  -> 800
        RigidBodyId            maGameIDs[200];     // X360 @+0x0320 (stride 8) -> 2400
        Inertia                maInertias[200];    // X360 @+0x0960 (stride 48)
    };

    // ---- PhysicsSimulationModule (DWARF CgsPhysicsSimulationModule.h:282) ------
    // GROWN IN PLACE 2026-08-03 from the enum-only shell this class used to be.
    //
    // MEMBER SET + ORDER are DWARF-attested (h:501..:538) and every one of the
    // offsets below was re-derived here from TWO independent X360 functions --
    // Construct @0x828A1EE8's own stores, and the C++ constructor @0x827DF1E0
    // (which chains RigidBodyData::RigidBodyData(this+0x230) and
    // JointData::JointData(this+0x3110) and inlines the DriveData init at
    // this+0x4730). The two agree and close to the byte:
    //     0x4864 (mpSimulation) + 4 -> pad to 0x4870 == 18544 == 0x4AA0 - 0x230,
    // which is exactly the span BrnPhysicsModule.h reserved for this member.
    //
    // BASE. The console vtable off_820CF7D0 was read slot-by-slot: slot 0 is
    // Construct (the console CgsModule::Module has NO virtual destructor),
    // slots 1..15 are ModuleSingleBuffered's, and this class's OWN new virtuals
    // start at slot 16 -- which is exactly the `lwz r11, 0x40(r11)` that
    // PhysicsModule::Prepare dispatches through. That corroborates the divergence
    // banner in CgsModule.h; nothing here indexes a vtable numerically, so the
    // tree's extra `virtual ~Module()` stays invisible.
    //
    // ⭐ Release (slot 2, @0x828A2048) and Destruct (slot 3, @0x828A2120) LANDED 2026-08-03,
    // with their bodies -- Destruct pulled out of the .i64 with headless IDA because it is an
    // export-set hole.
    //
    // ⭐⭐ SLOT 16, Prepare(rw::IResourceAllocator*, const SimulationParams&) @0x828A6A08,
    // LANDED 2026-08-04 (task #135) WITH ITS BODY AND ITS CALLEE. It is THE function that
    // assigns mpSimulation, and until it existed `rw::physics::Simulation` had no constructor
    // anywhere in the tree: six reconstructed solver objects linked and not one byte of them
    // could ever execute. Note this class was a HOLLOW SHELL in the precise sense -- the DWARF
    // declares SIX virtuals (Construct/Prepare/Release/Destruct/Update/ProcessInput) and the
    // committed header declared THREE, so the two that drive the simulation silently bound to
    // base defaults. Two are still missing:
    //     Update(IOBufferStack*, IOBufferStack*, const InputBuffer*, OutputBuffer*) @0x828A74D0
    //     ProcessInput(const InputBuffer*) @0x828A76D0
    // Declaring a virtual with no body while a constructor is defined materialises
    // the vtable and turns straight into LNK2019, so they land WITH their bodies,
    // not before. Until then those slots keep the base's behaviour.
    //
    // =================================================================================
    // ⭐⭐ 2026-08-04 (task #138) -- THE MEASURED MAP OF WHAT "STEPPING A CAR" COSTS.
    // Recorded here because successive briefs have described this as a small remaining
    // gap. It is not. Every count is an instruction count from the IDA export set, or
    // measured off the .i64 headless where noted.
    //
    // Update @0x828A74D0 is 127 instructions and calls, in order:
    //     IOBuffer::LockForRead                                50
    //     ProcessInputBuffers                    @0x828A73C0   68   ⚠️ export hole
    //     sub_8289E260 (the new time step)                     54
    //     InputBuffer::GetMaxIterations          @0x8289E338   52
    //     IOBuffer::UnlockForRead / LockForWrite               51 / 104
    //     IOBufferStack::CreateIOBuffer<IslandGenerator>       (template)
    //     QuerySimulationToSetFlags              @0x828A0428  248
    //     ActiveSetClosure                       @0x828A0808 1184
    //     ActivateAndFreezeAsNeeded              @0x828A6DD0  249
    //     rw::physics::PairSet::ClearAll                       BODIED
    //     rw::physics::Simulation::SimulationUpdate            79   (+1,805 of stages)
    //     OutputBuffer::SetTimeStepUsed / SetMaxIterationsUsed 44 / 42
    //     AddActiveBodiesToOutputQueue           @0x828A6CC8   65
    //     AddContactSpiesToOutputQueue           @0x828A4ED8  641
    //     AddJointSpiesToOutputQueue             @0x828A58E0  267  ⚠️ export hole
    //     AddDriveSpiesToOutputQueue             @0x828A5D10   72
    //     IOBuffer::UnlockForWrite                             51
    //     IOBufferStack::DestroyIOBuffer<IslandGenerator>      (template)
    //
    // ⚠️ TWO of those are ABSENT FROM .ida-exports and were recovered headless out of
    // BURNOUT_X360_ARTIST.XEX.i64 (IDA Pro 9.3 ships `idat.exe`, there is no
    // `idat64.exe`) -- the same route that produced PairSet::ClearAll and Destruct:
    //     ProcessInputBuffers        @0x828A73C0  (xrefs: Update, ProcessInput)
    //     AddJointSpiesToOutputQueue @0x828A58E0  (xref: Update)
    // Missing-from-JSON is not nonexistent -- see [[ida-export-set-has-holes]].
    //
    // ⭐ ProcessInputBuffers is only 68 instructions and is pure dispatch: NINETEEN
    // drain calls in a fixed order, each `(this, lpInput)`. That ORDER is load-bearing
    // (removes before adds; bodies, then joints, then drives) and is recorded in no
    // other source:
    //     ProcessAddContactQueue              @0x828A3458   363
    //     ProcessRemoveDriveQueue             @0x8289FF98    90
    //     ProcessRemoveJointQueue             @0x8289F970   174
    //     ProcessRemoveRigidBodyQueue         @0x828A2BD0   546
    //     ProcessRemoveAllRigidBodiesQueue    @0x8289F1D8    69
    //     ProcessAddRigidBodyQueue            @0x828A2708   306  <- calls AddRigidBody
    //     ProcessUpdateExternalBodyQueue      @0x828A3B30   368
    //     ProcessUpdateRigidBodyQueue         @0x828A3A08    74
    //     ProcessApplyForceQueue              @0x828A6B80    82
    //     ProcessSetRigidBodySpyQueue         @0x828A49A8    51
    //     ProcessChangeRigidBodyInertiaQueue  @0x828A4A78   143
    //     ProcessAddJointQueue                @0x828A40F0   557
    //     ProcessUpdateJointFramesQueue       @0x8289F2F0   149
    //     ProcessUpdateJointLimitsQueue       @0x8289F548   136
    //     ProcessSetJointSpyQueue             @0x8289F768   129
    //     ProcessAddDriveQueue                @0x828A4CB8   136
    //     ProcessUpdateDriveFramesQueue       @0x8289FC28    77
    //     ProcessUpdateDriveDynamicsQueue     @0x8289FD60    74
    //     ProcessSetDriveSpyQueue             @0x8289FE88    68  ⚠️ export hole
    // ⚠️ A drain over an empty queue is a no-op, which makes every one of these a
    // perfect candidate for a silent-drop stub. Do not stub them --
    // see [[silent-drop-stubs]].
    //
    // ⭐ 2026-08-04 (task #142) -- THE LIST ABOVE IS NOW COMPLETE AND VERIFIED CALL-FOR-CALL.
    // Two corrections to what this note used to say:
    //   * the last entry read "(final call, at 0x828A74B4)". 0x828A74B4 is the CALL SITE inside
    //     ProcessInputBuffers, not the target. The target is `ProcessSetDriveSpyQueue`
    //     @0x8289FE88, 68 instructions -- itself an .ida-exports hole, recovered headless.
    //     (Found via an `xrefs_to` entry sitting inside 0x8289DF80.json, i.e. it was reachable
    //     from the export set all along by looking at who calls what.)
    //   * so the nineteen drains are **3,592** instructions, not 3,524. The old figure was the
    //     sum of the eighteen whose sizes were banked.
    // The ORDER was re-derived from scratch by reading all nineteen `bl` targets out of
    // ProcessInputBuffers itself rather than trusting this note, and it reproduces exactly.
    // The body is pure dispatch: `mr r4,r30 / mr r3,r31 / bl` x19, no branches, no return value.
    //
    // ⭐⭐ AND THE ACTUAL PREREQUISITE IS NOT THE INSTRUCTION COUNT. Until task #142, thirteen of
    // the nineteen input queues were unmodelled byte gaps in PhysicsSimulationIO::InputBuffer
    // (138,960 bytes of `maQueueGap0..3`), so thirteen of these drains could not be written at
    // all regardless of how cheap their bodies are. All nineteen queues are now named at
    // X360-attested offsets and each has its const accessor bodied -- see
    // CgsPhysicsSimulationModuleIO.h. Landing a drain is now purely about the drain.
    //
    // AND ABOVE ALL OF IT: BrnPhysics::PhysicsModule::Update @0x825B0640 is itself
    // 1,999 instructions and is STILL A LINK STUB (GameSource/World/WorldLinkStubs.cpp,
    // "PhysicsModule::Update: inert [FLAG PC boot gate]"). Its depth-1 closure is ~50
    // functions / ~15,000 instructions, including BridgeContactsToSimulation 1,671,
    // StartVehicleContactGeneration 1,229, FixUpVehicleContacts 1,067,
    // UpdateVehiclePhysics 1,038, DeformationManager::Update 1,021,
    // DoCrashPrediction 814.
    //
    // RUNNING TOTAL for "a car moves under its own physics": ~25,000 X360 instructions,
    // VMX-heavy, plus RaceCarPhysics.cpp which is still unmounted. Consistent with
    // [[vehicle-physics-is-the-wall]]'s 6-9 wave estimate; NOT a one-wave gap.
    // =================================================================================
    class PhysicsSimulationModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // DWARF h:294.
        enum EPrepareStage
        {
            PREPARESTAGE_START   = 0,
            PREPARESTAGE_MANAGER = 1,
            PREPARESTAGE_DONE    = 2,
        };

        // DWARF h:301.
        enum EReleaseStage
        {
            RELEASESTAGE_START   = 0,
            RELEASESTAGE_MANAGER = 1,
            RELEASESTAGE_DONE    = 2,
        };

        // DWARF h:286. Built on the stack by BrnPhysics::PhysicsModule::Prepare
        // @0x825ADB68 case 3 and handed to Prepare(alloc, params). Declared here
        // (it is this class's nested type) but not consumed yet.
        struct SimulationParams
        {
            f32 mafGravity[4];      // Vector3 mGravity   (h:288)
            f32 mfFreezingEnergy;   // (h:289)
            f32 mfTimeStep;         // (h:290)
            u32 muMaxIterations;    // (h:291)
        };

        // X360 @0x827DF1E0. Stamps the two vtables + constructs the base's two
        // EA::Thread::RWMutex members (all of that is the base sub-object's own
        // construction in C++), then chains mBodyData's and mJointData's
        // constructors and inlines mDriveData's.
        PhysicsSimulationModule();

        // X360 @0x828A1EE8 (86 instructions, 5 call sites, no asserts). Console
        // vtable slot 0.
        void Construct() override;

        // X360 @0x828A6A08. Console vtable slot 16 -- the FIRST of this class's own new
        // virtuals, and the one BrnPhysics::PhysicsModule::Prepare stage 3 dispatches
        // through (`lwz r11,0x40(r11)` == slot 16 on the 4-byte console vtable).
        //
        // A two-stage fall-through FSM over mePrepareStage, exactly the shape Release()
        // has: stage 0 advances the cursor and falls into stage 1; stage 1 runs the base's
        // Prepare and, on success, clears the three bit arrays, builds/refreshes the whole
        // rw::physics world through AllocateMemoryAndInitialiseRW, clears the three slot
        // tables and advances again.
        //
        // ⚠️ NOT an `override`: the base has a NO-ARGUMENT Prepare(). This is a new virtual
        // with its own signature, which is why the console gives it a fresh slot rather than
        // reusing slot 1. It therefore HIDES the base name; every in-class use of the base's
        // Prepare is explicitly qualified, as the console's own body is.
        virtual bool Prepare(rw::IResourceAllocator* lpAllocator, const SimulationParams& lrParams);

        // X360 @0x828A2048 (54 instructions). Console vtable slot 2. A fall-through
        // release FSM over meReleaseStage; returns false (cursor left on stage 1) when
        // the base's Release has not finished, so the owner re-enters next frame.
        bool Release() override;

        // X360 @0x828A2120 (16 instructions). Console vtable slot 3. ⚠️ Absent from
        // .ida-exports -- recovered from BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3
        // (the same export-set hole RigidBodyData::RigidBodyData was).
        void Destruct() override;

    private:
        // X360 @0x828A2168 (DWARF CgsPhysicsSimulationModule.cpp:266). NON-virtual; the only
        // caller is Prepare above. Builds or REBUILDS the rw::physics world:
        //   * first call (mpSimulation == NULL): size + carve the Simulation, its
        //     SimulationWorkspace and the three PairSets out of lpAllocator, initialise each,
        //     push the caller's gravity / freezing energy / iteration cap into the simulation,
        //     enable all three jacobian spies, and allocate the 200-entry next-index array.
        //   * every later call: tear the world's CONTENTS down instead -- remove every live
        //     joint, the drive, and every rigid body whose slot is in use -- keeping the
        //     allocation. (The console can only bump-allocate, so a re-Prepare must reuse it.)
        // Both paths then clear the three pair sets and reset the counters.
        //
        // ⚠️ EXPORT-SET NEIGHBOUR: this symbol is the one immediately after Destruct
        // @0x828A2120, which is itself an export hole -- see Destruct's note above.
        void AllocateMemoryAndInitialiseRW(rw::IResourceAllocator* lpAllocator,
                                           const SimulationParams& lrParams);

        // ---------------------------------------------------------------------------------
        // ⭐ THE FIRST OF THE NINETEEN INPUT DRAINS TO BE BODIED (task #140, 2026-08-04).
        // X360 @0x828A2708 (306 instructions). Sole caller: ProcessInputBuffers @0x828A73C0,
        // which is not bodied yet -- so this is currently reached by nothing at runtime, and
        // that is stated plainly rather than implied. What it BUYS is that
        // rw::physics::Simulation::AddRigidBody, the function the whole vehicle-physics
        // campaign is built on top of, now lands WITH A CALLER instead of as an orphan; task
        // #138 decoded it and correctly refused to write it while it had none.
        //
        // Drain the input buffer's add-rigid-body queue: for each request, claim a
        // RigidBodyData slot, create the rw::physics body, then push the requested velocities
        // and spy flag into it and record the slot.
        //
        // ⚠️ THE TWO DEBUG SCANS ARE O(n^2) AND O(n*200) AND THEY ARE REAL. The console runs
        // a duplicate-id scan over the queue (.cpp:1034) and, per event, a full 200-slot scan
        // for an id already in the simulation (.cpp:1058), both firing asserts with the
        // offending id formatted in. They are kept: a drain that silently accepts a duplicate
        // body id is precisely the [[silent-drop-stubs]] shape.
        void ProcessAddRigidBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);

        // ---- THE DRIVE GROUP, all five (task #143, 2026-08-04) ---------------------------
        // Landed as a COMPLETE group, not a sampler: these five are the whole drive subsystem
        // of the nineteen, and a partly-drained queue is the [[silent-drop-stubs]] shape.
        // ⚠️ NOTHING CALLS THEM YET. ProcessInputBuffers is still not bodied and must not be
        // until all nineteen exist -- 6 of 19 are now bodied (AddRigidBody + these five).
        //
        // All five share one skeleton, read off the asm and identical in each:
        //     q = lpInput->Get<X>Queue();
        //     for (i = 0; i < q->GetLength(); ++i)            // length RE-READ every pass
        //         e = q->GetEvent(i);
        //         idx = mDriveData.GetIndexFromGameID(e.mId); // inlined in the console
        //         if (idx != -1) { ...payload... }
        void ProcessAddDriveQueue(const PhysicsSimulationIO::InputBuffer* lpInput);            // @0x828A4CB8  136
        void ProcessRemoveDriveQueue(const PhysicsSimulationIO::InputBuffer* lpInput);         // @0x8289FF98   90
        void ProcessUpdateDriveFramesQueue(const PhysicsSimulationIO::InputBuffer* lpInput);   // @0x8289FC28   77
        void ProcessUpdateDriveDynamicsQueue(const PhysicsSimulationIO::InputBuffer* lpInput); // @0x8289FD60   74
        void ProcessSetDriveSpyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);         // @0x8289FE88   68

        // ---- THE JOINT GROUP -- five more of the nineteen (task #144) ---------------------
        // Same skeleton, with mJointData in place of mDriveData and a 36-slot table instead of
        // a 1-slot one. ⚠️ ONE STRUCTURAL DIFFERENCE, and it is not cosmetic: where the drive
        // five skip an unresolvable id SILENTLY, all five of these ASSERT first and then skip.
        // Both are transcribed as shipped; do not harmonise them.
        void ProcessAddJointQueue(const PhysicsSimulationIO::InputBuffer* lpInput);            // @0x828A40F0  557
        void ProcessRemoveJointQueue(const PhysicsSimulationIO::InputBuffer* lpInput);         // @0x8289F970  174
        void ProcessUpdateJointFramesQueue(const PhysicsSimulationIO::InputBuffer* lpInput);   // @0x8289F2F0  149
        void ProcessUpdateJointLimitsQueue(const PhysicsSimulationIO::InputBuffer* lpInput);   // @0x8289F548  136
        void ProcessSetJointSpyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);         // @0x8289F768  129

        // ---- THE RIGID-BODY GROUP -- seven more of the nineteen (2026-08-05) ---------------
        // Landed as the complete rigid-body-side set, same rule as the two groups above.
        // (The "18 of 19 / ProcessInputBuffers stays unbodied" note that stood here is
        // retired: drain 19 and ProcessInputBuffers landed later the same day -- see them
        // below. ⚠️ ProcessInputBuffers itself still has no caller; the two virtuals that
        // call it, Update @0x828A74D0 / ProcessInput @0x828A76D0, remain unbodied.)
        // Shared skeleton as the drive/joint groups (length RE-READ per pass; miss on
        // GetIndexFromGameID(id) == -1). ⚠️ THE MISS POLICY IS PER-DRAIN, READ OFF THE ASM,
        // NOT INHERITED: the five update-side drains skip a miss SILENTLY (drive-style);
        // ProcessRemoveRigidBodyQueue asserts on a miss ONLY when the event's
        // mbFailIfRigidBodyNotFound is set; ProcessRemoveAllRigidBodiesQueue has no id at all
        // (it sweeps by owner byte).
        void ProcessRemoveRigidBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);      // @0x828A2BD0  546
        void ProcessRemoveAllRigidBodiesQueue(const PhysicsSimulationIO::InputBuffer* lpInput); // @0x8289F1D8   69
        // ⚠️ DWARF ACCESSIBILITY: this ONE drain is PUBLIC in the DWARF (declared alongside
        // GetDefaultParams / AddActiveBodiesToOutputQueue, before the private block that holds
        // its eighteen siblings) -- some out-of-module caller existed on the console. Kept
        // public accordingly; the specifier flips back right below it.
    public:
        void ProcessUpdateExternalBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);   // @0x828A3B30  368
    private:
        void ProcessUpdateRigidBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);      // @0x828A3A08   74
        void ProcessApplyForceQueue(const PhysicsSimulationIO::InputBuffer* lpInput);           // @0x828A6B80   82
        void ProcessSetRigidBodySpyQueue(const PhysicsSimulationIO::InputBuffer* lpInput);      // @0x828A49A8   51
        void ProcessChangeRigidBodyInertiaQueue(const PhysicsSimulationIO::InputBuffer* lpInput); // @0x828A4A78 143

        // ---- DRAIN 19 -- THE CONTACT DRAIN, and the dispatcher (2026-08-05) ----------------
        // The last of the nineteen. It was decoded in full one wave earlier and deliberately
        // NOT written, because Contact::mBodyA/mBodyB (+0x0C/+0x1C) are populated only by the
        // full-row stvx of the event's mPointOnA/mPointOnB w lanes -- a producer-side contract
        // nobody had read. THAT CONTRACT IS NOW READ AND CLOSED from the CONSUMER:
        // rw::physics::Simulation::ContactBatchBuild @0x82BC14C0 overwrites both lanes
        // (vsel mask {-1,-1,-1,0} @0x82181660) with the snapshot mCom.w == RigidBody::mId
        // before anything reads them -- the event w lanes are DEAD CARGO. Full write-up in
        // vendor's contact.h banner. DWARF h: CgsPhysicsSimulationModule.h:458 (.cpp:1224).
        void ProcessAddContactQueue(const PhysicsSimulationIO::InputBuffer* lpInput);           // @0x828A3458  363

        // ProcessInputBuffers -- the pure 19-call dispatcher (DWARF h:440, .cpp:978; an
        // .ida-exports hole, recovered headless). ALL NINETEEN drains exist, so the standing
        // "stays unbodied until all nineteen" rule is SATISFIED, not bent. The call ORDER is
        // load-bearing (removes before adds; bodies, then joints, then drives) and was
        // re-derived from the function's own `bl` targets twice -- see the banner above the
        // class. ⚠️ STILL UNREACHED AT RUNTIME: its two callers on the console, the Update /
        // ProcessInput virtuals, are not bodied (declaring them without bodies materialises
        // the vtable into LNK2019 -- see the slot-16 note above the class).
        void ProcessInputBuffers(const PhysicsSimulationIO::InputBuffer* lpInput);              // @0x828A73C0   68
        // ---------------------------------------------------------------------------------

        // DWARF h:532..:536. Static, so they take no space.
        static const u32 KU_NUM_BODIES             = 200;   // kuNumBodies
        static const u32 KU_NUM_JOINTS             = 36;    // kuNumJoints
        static const u32 KU_NUM_DRIVES             = 1;     // kuNumDrives
        static const u32 KU_NUM_POTENTIAL_CONTACTS = 1024;  // kuNumPotentialContacts
        static const u32 KU_NUM_COLLIDING_PAIRS    = 1024;  // kuNumCollidingPairs

        // ⚠️ mePrepareStage / meReleaseStage intentionally shadow the base's
        // private members of the same name -- the console really does carry both
        // pairs (base @+0x08/+0x0C, derived @+0x228/+0x22C). The base's are
        // private, so only the base's own code can see them; every use inside this
        // class binds to the derived pair, which is what the asm writes.
        EPrepareStage            mePrepareStage;   // X360 +0x228 (h:501)
        EReleaseStage            meReleaseStage;   // X360 +0x22C (h:502)

        RigidBodyData            mBodyData;        // X360 +0x230  (12000) (h:504)
        JointData                mJointData;       // X360 +0x3110 (5664)  (h:505)
        DriveData                mDriveData;       // X360 +0x4730 (160)   (h:506)

        rw::physics::PairSet*    mpJointedPairs;   // X360 +0x47D0 (h:509)
        rw::physics::PairSet*    mpDrivenPairs;    // X360 +0x47D4 (h:510)
        rw::physics::PairSet*    mpContactPairs;   // X360 +0x47D8 (h:511)
        s32                      miNeedThaw;       // X360 +0x47DC (h:512)
        s32                      miActive;         // X360 +0x47E0 (h:513)
        s32*                     mpiNextIndex;     // X360 +0x47E4 (h:514)

        CgsContainers::BitArray<KU_NUM_BODIES> mNeedFreeze;  // X360 +0x47E8 (h:515)
        CgsContainers::BitArray<KU_NUM_BODIES> mDone;        // X360 +0x4808 (h:516)
        CgsContainers::BitArray<KU_NUM_BODIES> mSeen;        // X360 +0x4828 (h:517)

        s32                      miNumRigidBodies; // X360 +0x4848 (h:519)
        s32                      miNumDrives;      // X360 +0x484C (h:520)
        s32                      miNumJoints;      // X360 +0x4850 (h:521)

        s32                      miTimeInSim1;     // X360 +0x4854 (h:523) "Sim setup"
        s32                      miTimeInSim2;     // X360 +0x4858 (h:524) "Sim freezing"
        s32                      miTimeInSim3;     // X360 +0x485C (h:525) "Sim update"
        s32                      miTimeInSim4;     // X360 +0x4860 (h:526) "Sim output"

        rw::physics::Simulation* mpSimulation;     // X360 +0x4864 (h:538)

        // Pin the layout. Never called -- exists only so offsetof can see the
        // private members (offsetof on a private member needs member-function
        // scope). Same pattern as VehicleManager::_AssertLayout.
        static void _AssertLayout();
    };

    // X360 0x8289CF18 (DWARF CgsPhysicsSimulationModule.h:566). Post-increment on
    // the release-stage cursor.
    PhysicsSimulationModule::EReleaseStage
    operator++(PhysicsSimulationModule::EReleaseStage& leEnumIndex, int);

    // DWARF CgsPhysicsSimulationModule.h:565 -- the prepare-stage twin. The console INLINED
    // it into Prepare @0x828A6A08 (the `+1`, the store and the `> 2` tripwire are open-coded
    // there, unlike Release which keeps the out-of-line call), but the source declares it and
    // the assert text it carries names it: "leEnumIndex <= PhysicsSimulationModule::
    // PREPARESTAGE_DONE", CgsPhysicsSimulationModule.h:565.
    PhysicsSimulationModule::EPrepareStage
    operator++(PhysicsSimulationModule::EPrepareStage& leEnumIndex, int);
}
