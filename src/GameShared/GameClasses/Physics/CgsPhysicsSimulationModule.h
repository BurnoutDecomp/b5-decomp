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

// ⭐ THE REAL rw::physics::Inertia, by value -- `CgsPhysics::Inertia` is now an alias onto it
// (see the block below) and RigidBodyData embeds 200 of them, so a forward declaration will
// not do. Safe to pull in here where CgsPhysicsSimulationIO_Events.h is not: inertia.h reaches
// only types.hpp, rw/math/vpu/types.h, <cfloat> and <cstddef>, so it drags no CgsPhysics type
// into this header's ~30 includers and cannot make the open RigidBodyId ODR fork meet.
#include "rw/physics/inertia.h"

// rw::physics pointer members of PhysicsSimulationModule. Forward declarations
// only -- the module never dereferences them in the functions homed here. Class
// keys match their real homes (vendor/renderware/include/rw/physics/{simulation,
// pairset}.h), so no ODR/class-key mismatch.
namespace rw { namespace physics { class Simulation; class PairSet; class Joint; class Drive; struct RigidBody; } }

// The per-frame input buffer the nineteen Process*Queue drains read. Forward-declared only:
// CgsPhysicsSimulationModuleIO.h includes CgsPhysicsSimulationIO_Events.h, which this header
// must NOT pull in (it would drag CgsRigidBody.h's RigidBodyId into every TU that sees this
// one and detonate the open ODR fork with the struct declared below). The .cpp includes the
// full definition. ⚠️ CLASS KEY IS `struct`, matching CgsPhysicsSimulationModuleIO.h.
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

    // 5 quaternion/vector slots: Quaternion(16)+Vector3(16)+Quaternion(16)+
    // Vector3(16)+Quaternion(16) == 80 bytes (DWARF jointframes.h).
    struct alignas(16) JointFrames { u8 macOpaque[80]; };

    // Quaternion(16)+Vector3(16)+Quaternion(16)+Vector3(16) == 64 bytes
    // (DWARF driveframes.h).
    struct alignas(16) DriveFrames { u8 macOpaque[64]; };

    // rw::physics::DriveType (DWARF drivedynamics.h:66). Named E_-prefixed per the
    // project convention, matching the E_SwingType/E_TwistType precedent below.
    enum E_DriveType : s32 { E_NO_DRIVE = 0, E_SOFT_DRIVE = 1, E_HARD_DRIVE = 2 };

    // rw::physics::DriveDynamics (DWARF drivedynamics.h:95) == two Params
    // sub-records, mLinear @+0x00 and mAngular @+0x10 (drivedynamics.h:261/262),
    // each {f32 mSpring, mDamping, mStrength; DriveType mType} (:204..:207) == 16
    // bytes -> 32-byte stride, which is the X360-attested DriveData stride.
    //
    // PROMOTED from an opaque 32-byte span (was: u8 macOpaque[32]). The module
    // constructor @0x827DF1E0 inlines DriveData's default-initialisation of both
    // maDynamics[0] and maScaledDynamics[0] as, per record, three `stfs 0.0f` at
    // +0/+4/+8 and a `stw 0` at +0xC, then the same four again at +0x10..+0x1C --
    // i.e. it writes these fields BY MEMBER, so they cannot stay opaque.
    struct alignas(16) DriveDynamics
    {
        struct Params
        {
            f32         mfSpring;    // @+0x00 (drivedynamics.h:204)
            f32         mfDamping;   // @+0x04 (:205)
            f32         mfStrength;  // @+0x08 (:206)
            E_DriveType meType;      // @+0x0C (:207)
        };

        Params mLinear;              // @+0x00 (:261)
        Params mAngular;             // @+0x10 (:262)
    };

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

    // CgsPhysics::RigidBodyId (CgsRigidBody.h:48): a single uint64_t id (DWARF
    // + Feb-2007 source both name the member `mId`). IsInvalid() compares mId
    // against the module-global sentinel K_INVALID_RIGID_BODY_ID (X360 static
    // qword_82F33E18; value 0xFFFFFFFFFFFFFFFF per CgsRigidBody.h:87). Only the
    // RigidBodyData slot tripwires consume it, so just the id + IsInvalid() are
    // modelled.
    struct RigidBodyId
    {
        u64 mId;                                    // CgsRigidBody.h:84/120
        bool IsInvalid() const;
    };
    // CgsRigidBody.h:87: static const RigidBodyId K_INVALID_RIGID_BODY_ID = ~0ull.
    extern const RigidBodyId K_INVALID_RIGID_BODY_ID;
    inline bool RigidBodyId::IsInvalid() const { return mId == K_INVALID_RIGID_BODY_ID.mId; }

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

    // JointLimits IS modelled field-by-field: JointData's constructor writes its
    // members by name. 64-byte record (DWARF jointlimits.h). The two trailing
    // enum slots default to 0 (SWING_LOCKED / TWIST_LOCKED).
    enum E_SwingType : s32 { E_SWING_LOCKED = 0, E_SWING_CONE = 1, E_SWING_HINGE = 2, E_SWING_AXLE = 3, E_SWING_FREE = 4 };
    enum E_TwistType : s32 { E_TWIST_LOCKED = 0, E_TWIST_ARC = 1, E_TWIST_FREE = 2 };

    struct alignas(16) JointLimits
    {
        f32         mafPprism[4];   // Vector3 mPprism  (@+0x00, 16B incl. pad)
        f32         mafVprism[4];   // Vector3 mVprism  (@+0x10, 16B incl. pad)
        f32         mfVtwist;       // @+0x20
        f32         mfVswing;       // @+0x24
        f32         mfSwinga;       // @+0x28
        f32         mfTwista;       // @+0x2C
        f32         mfSwingc;       // @+0x30
        f32         mfTwistc;       // @+0x34
        E_SwingType meSwingf;       // @+0x38
        E_TwistType meTwistf;       // @+0x3C
    };

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
    //     ProcessSetDriveSpyQueue             (final call, at 0x828A74B4)
    // ⚠️ A drain over an empty queue is a no-op, which makes every one of these a
    // perfect candidate for a silent-drop stub. Do not stub them --
    // see [[silent-drop-stubs]].
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
