#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // AddMonitor

// The real rw::physics world Prepare/AllocateMemoryAndInitialiseRW build. Only the .cpp
// needs the definitions -- the header keeps them forward-declared, exactly as the console's
// does (its own Prepare is the only member that dereferences mpSimulation).
// The drain side needs the COMPLETE InputBuffer (the header only forward-declares it):
// ProcessAddRigidBodyQueue reads its embedded add-rigid-body queue and the events in it.
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"

#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator, Resource, descriptors
#include "rw/physics/simulation.h"              // rw::physics::Simulation + SpyingFlag
#include "rw/physics/SimulationWorkspace.h"     // rw::physics::SimulationWorkspace
#include "rw/physics/pairset.h"                 // rw::physics::PairSet
#include "vendor/renderware/physics/Drive.hpp"   // rw::physics::Drive -- the drive drains (task #143)

#include <cstddef>   // offsetof (layout gate)
#include <cfloat>    // FLT_MAX  (rw::physics::Inertia's default max velocity/omega)

// CgsPhysics::PhysicsSimulationModule and its slot tables RigidBodyData /
// JointData / DriveData. Reconstructed store-for-store from the X360 asm; the
// "liIndex < knSize" / "mabUsedSlot[liIndex]" tripwires are reproduced via
// CGS_ASSERT (the X360 BeginAssert/FireAssert/EndAssert sequence). The asm file/
// line literals (CgsPhysicsSimulationModule.h:621/622 etc.) name the tripwire;
// CGS_ASSERT supplies its own __FILE__/__LINE__, so only the message string is
// carried through, matching the other reconstructed assert sites.
//
// X360-attested byte offsets, for the record (the slot tables are struct-of-
// arrays indexed by slot): JointData maLimits@0xB40, maRWJoints@0x1440 (5184),
// mabUsedSlot@0x15F0 (5616); DriveData maFrames@0, maScaledDynamics@0x60 (96),
// mabUsedSlot@0x90 (144). These hold on the X360's 32-bit (4-byte) pointer ABI;
// the host gate builds x64 (8-byte pointers), so the *absolute* offsets past the
// pointer-array slots widen and are NOT asserted here. The host-stable element
// strides (which the X360 store widths actually depend on) are locked below;
// member access is by name throughout, so the bodies are semantically faithful
// regardless of host pointer width.

namespace CgsPhysics
{
    // Host-stable element strides (X360-attested, pointer-width-independent).
    static_assert(sizeof(JointLimits)   == 64, "JointLimits stride 64 (h:jointlimits.h)");
    static_assert(sizeof(JointFrames)   == 80, "JointFrames stride 80 (h:jointframes.h)");
    static_assert(sizeof(DriveFrames)   == 64, "DriveFrames stride 64 (h:driveframes.h)");
    static_assert(sizeof(DriveDynamics) == 32, "DriveDynamics stride 32 (h:drivedynamics.h)");

    // ⚠️ THE DriveDynamics INTERIOR PINS MOVED, THEY WERE NOT DROPPED (task #143). They used
    // to sit here and gate the FORKED local copy's `mfSpring`/`meType` spellings. The record
    // now has ONE definition, so the pins live with it, in the file that owns the fields:
    // vendor/renderware/physics/DriveDynamics.hpp -> _rw_physics_DriveDynamics_AssertLayout().
    // Same for the 64-byte DriveFrames interior -> _rw_physics_DriveFrames_AssertLayout().
    // The two strides above are kept here as well, because here they are a claim about
    // DriveData's array layout rather than about the records themselves.
    static_assert(sizeof(DriveDynamics::Params) == 16, "DriveDynamics::Params is 16B (drivedynamics.h:204-207)");

    // ===================== JointData =======================================

    // X360 @0x827DB798. 36-pass loop default-initialising maLimits[i]: zero the
    // two leading 16-byte vectors and mfVtwist..mfTwista plus both enum slots,
    // and set mfSwingc/mfTwistc to 1.0f. (f0 == 0.0f, f13 == 1.0f.)
    JointData::JointData()
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            JointLimits& lLimit = maLimits[li];
            lLimit.mafPprism[0] = 0.0f;            // stvx128 v0,r0,r11   (mPprism)
            lLimit.mafPprism[1] = 0.0f;
            lLimit.mafPprism[2] = 0.0f;
            lLimit.mafPprism[3] = 0.0f;
            lLimit.mafVprism[0] = 0.0f;            // stvx128 v13,r11,r6  (mVprism)
            lLimit.mafVprism[1] = 0.0f;
            lLimit.mafVprism[2] = 0.0f;
            lLimit.mafVprism[3] = 0.0f;
            lLimit.mfVtwist = 0.0f;                // stfs f0,0x20(r11)
            lLimit.mfVswing = 0.0f;                // stfs f0,0x24(r11)
            lLimit.mfSwinga = 0.0f;                // stfs f0,0x28(r11)
            lLimit.mfTwista = 0.0f;                // stfs f0,0x2C(r11)
            lLimit.mfSwingc = 1.0f;                // stfs f13,0x30(r11)
            lLimit.mfTwistc = 1.0f;                // stfs f13,0x34(r11)
            lLimit.meSwingf = E_SWING_LOCKED;      // stw r10(=0),0x38(r11)
            lLimit.meTwistf = E_TWIST_LOCKED;      // stw r10(=0),0x3C(r11)
        }

        // ---- layout pins (hosted here so offsetof can see the private members) ----
        // X360-exact and host-invariant up to the first pointer array: maLimits at
        // 0xB40 == 36*80, maRWJoints at 0x1440 == 0xB40 + 36*64. Past that the
        // pointer slot widens on x64, so the tail is pinned relative to it.
        static_assert(offsetof(JointData, maFrames)   == 0,    "maFrames @+0");
        static_assert(offsetof(JointData, maLimits)   == 2880, "maLimits @+0xB40 (X360-exact: 36*80)");
        static_assert(offsetof(JointData, maRWJoints) == 5184, "maRWJoints @+0x1440 (X360-exact: 0xB40 + 36*64)");
        static_assert(offsetof(JointData, maGameIDs)  == 5184 + KI_SIZE * sizeof(rw_physics::Joint*),
                      "maGameIDs follows the 36 Joint* slots (X360 +0x14D0 at 4-byte pointers)");
        static_assert(offsetof(JointData, mabUsedSlot) == offsetof(JointData, maGameIDs) + KI_SIZE * sizeof(JointId),
                      "mabUsedSlot follows the 36 JointIds (X360 +0x15F0)");
        // X360 closure: 0x15F0 + 36 == 5652 -> 16-align -> 5664 == 0x1620 == 0x4730 - 0x3110.
        static_assert(sizeof(JointData) == ((offsetof(JointData, mabUsedSlot) + KI_SIZE + 15) / 16) * 16,
                      "sizeof(JointData) is mabUsedSlot + 36, rounded to the 16-byte element alignment");
    }

    // X360: inlined into PhysicsSimulationModule::Construct @0x828A1F38..0x828A1F4C as a
    // 36-pass `stb 0` over this+0x4700 == &mabUsedSlot[0]. Nothing else is touched.
    void JointData::Clear()
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            mabUsedSlot[li] = false;               // stb r11(=0), 0(r10); addi r10, r10, 1
        }
    }

    // X360 @0x8289D168. Checked slot read returning maRWJoints[liIndex].
    rw_physics::Joint* JointData::GetJoint(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:621 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:622 tripwire (bne skips)
        return maRWJoints[liIndex];
    }

    // Raw slot-occupancy probe. Inlined by the console into
    // PhysicsSimulationModule::AllocateMemoryAndInitialiseRW (`lbzx` off &mabUsedSlot[0] at
    // 0x828A2560, then `beq` past the removal). No tripwire -- this IS the tripwire.
    bool JointData::IsSlotUsed(s32 liIndex) const
    {
        return mabUsedSlot[liIndex];
    }

    // ===================== DriveData =======================================

    // Inlined by the console into PhysicsSimulationModule::PhysicsSimulationModule
    // @0x827DF250..0x827DF294 (KI_SIZE == 1, so it is fully unrolled there): the two
    // 32-byte DriveDynamics records at mDriveData+0x40 (maDynamics[0]) and +0x60
    // (maScaledDynamics[0]) are zeroed field by field -- `stfs 0.0f` at +0/+4/+8 and
    // `stw 0` at +0xC for mLinear, then the same at +0x10/+0x14/+0x18/+0x1C for
    // mAngular. f0 is flt_82001CC0 == 0.0f (image bytes 00000000); r9 == 0, and
    // rw::physics::NO_DRIVE == 0. maFrames / maRWDrives / maGameIDs / mabUsedSlot are
    // NOT written by the constructor.
    DriveData::DriveData()
    {
        // ⚠️ THE EIGHT ZERO-STORES PER RECORD MOVED, THEY DID NOT DISAPPEAR (task #143).
        // This loop used to spell them out by hand -- `lDynamics.mLinear.mfSpring = 0.0f;`
        // and seven more, twice -- against the FORKED `CgsPhysics::DriveDynamics` that this
        // header no longer declares. `DriveDynamics` is now the real rw::physics class, whose
        // DWARF-declared `Params()` (drivedynamics.h:32) performs exactly those four stores
        // per Params. So the console's `stfs 0.0f` x3 + `stw 0` at +0/+4/+8/+0xC, twice per
        // record and twice over for the two arrays (@0x827DF250..0x827DF294), is now emitted
        // BY THE TYPE THAT OWNS THE FIELDS -- which is what "default-construct every entry"
        // in the banner above always claimed this was.
        //
        // ⚠️ The explicit re-assignment is kept rather than left to the array's own default
        // construction: maFrames / maRWDrives / maGameIDs / mabUsedSlot are deliberately NOT
        // touched by the console here, and relying on implicit construction of the whole
        // object would be a different program.
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            maDynamics[li]       = DriveDynamics();   // 0x827DF250.. per-Params zero stores
            maScaledDynamics[li] = DriveDynamics();   // 0x827DF274.. the same four again
        }

        // ---- layout pins. Every one of these is X360-EXACT *and* host-invariant:
        // the single Drive* slot at +0x80 is followed by an 8-aligned DriveId, which
        // lands maGameIDs at +0x88 on both ABIs, so nothing here widens.
        static_assert(offsetof(DriveData, maFrames)         == 0,   "maFrames @+0x00");
        static_assert(offsetof(DriveData, maDynamics)       == 64,  "maDynamics @+0x40 (asm addi r11, r10, 0x40)");
        static_assert(offsetof(DriveData, maScaledDynamics) == 96,  "maScaledDynamics @+0x60 (asm addi r10, r10, 0x60)");
        static_assert(offsetof(DriveData, maRWDrives)       == 128, "maRWDrives @+0x80");
        static_assert(offsetof(DriveData, maGameIDs)        == 136, "maGameIDs @+0x88 (8-aligned after the Drive* slot)");
        static_assert(offsetof(DriveData, mabUsedSlot)      == 144, "mabUsedSlot @+0x90 (asm stb 0, 0x47C0(r31) == mDriveData+0x90)");
        static_assert(sizeof(DriveData)                     == 160, "sizeof(DriveData) == 0xA0 == 0x47D0 - 0x4730");
    }

    // X360: inlined into PhysicsSimulationModule::Construct as the single
    // `stb r11(=0), 0x47C0(r31)` (mDriveData + 0x90 == &mabUsedSlot[0]).
    void DriveData::Clear()
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            mabUsedSlot[li] = false;
        }
    }

    // X360 @0x8289D1E8. Checked slot read returning &maFrames[liIndex].
    DriveFrames* DriveData::GetDriveFrames(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:666 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:667 tripwire (bne skips)
        return &maFrames[liIndex];
    }

    // X360 @0x8289D268. Checked slot read returning &maScaledDynamics[liIndex].
    DriveDynamics* DriveData::GetScaledDriveDynamics(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:684 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:685 tripwire (bne skips)
        return &maScaledDynamics[liIndex];
    }

    // DWARF h:241. Inlined by the console into AllocateMemoryAndInitialiseRW as the bare
    // `lwz r4, 0x47B0(r31)` at 0x828A25CC -- maRWDrives[0] with no tripwire of its own,
    // because the mabUsedSlot test two instructions earlier is the guard.
    rw_physics::Drive* DriveData::GetDrive(s32 liIndex)
    {
        return maRWDrives[liIndex];
    }

    // Raw slot-occupancy probe -- the `lbz r11, 0x47C0(r31)` + `beq` at 0x828A25C0.
    bool DriveData::IsSlotUsed(s32 liIndex) const
    {
        return mabUsedSlot[liIndex];
    }

    // DWARF h:249. The UNSCALED sibling of GetScaledDriveDynamics: a DIFFERENT array
    // (maDynamics @+0x40, not maScaledDynamics @+0x60). Inlined into
    // ProcessUpdateDriveDynamicsQueue @0x8289FE44 as `(liIndex+2)<<5` + this, which is
    // 32*liIndex + 0x40 exactly, behind the same two tripwires.
    DriveDynamics* DriveData::GetDriveDynamics(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:675 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:676 tripwire (bne skips)
        return &maDynamics[liIndex];
    }

    // DWARF h:218. No out-of-line symbol: the console inlines this identically into FOUR
    // drains (ProcessUpdateDriveFramesQueue @0x8289FC9C, ProcessRemoveDriveQueue @0x828A0008,
    // ProcessUpdateDriveDynamicsQueue @0x8289FDC8, ProcessSetDriveSpyQueue @0x8289FEF0), and
    // all four copies are instruction-for-instruction the same shape:
    //
    //   lbzx r8, r9, r31    ; mabUsedSlot[i]      (r9 = this + 0x90)
    //   beq  -> next        ; free slot: skip without touching the id
    //   ld   r8, 0(r11)     ; maGameIDs[i]        (r11 = this + 0x88, stepped by 8)
    //   cmpld r8, r10       ; == the event's id
    //   beq  -> found
    //
    // ⚠️ The used-slot test comes FIRST and short-circuits, so a free slot's stale id is
    // never compared. Reordering these two would change which ids can match.
    // ⭐ Both id reads are 8-byte `ld` at an 8-byte stride -- that, not the header, is what
    // pins DriveId at 8 bytes.
    //
    // Unlike RigidBodyData::GetIndexFromGameID below, the miss path here is silent: no
    // assert, no diagnostic stream. The four call sites all guard on `!= -1`.
    s32 DriveData::GetIndexFromGameID(DriveId lId) const
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            if (mabUsedSlot[li] && maGameIDs[li].muId == lId.muId)
                return li;
        }
        return -1;
    }

    // DWARF h:210. X360 @0x828A0330 (62 instructions), sole caller ProcessAddDriveQueue
    // @0x828A4E2C. Claims the first free slot and fills it.
    //
    // ⭐ EVERY ONE OF THE SIX ARRAY BASES IS AN INDEPENDENT WITNESS TO THIS CLASS'S LAYOUT,
    // because the console reaches each through its own `slwi`, and all six reproduce the
    // committed offsets exactly:
    //     i<<6        -> +0x00  maFrames          (i+2)<<5 -> +0x40  maDynamics
    //     (i+3)<<5    -> +0x60  maScaledDynamics  (i+0x20)<<2 -> +0x80  maRWDrives
    //     (i+0x11)<<3 -> +0x88  maGameIDs         stb 1, 0x90(this+i) -> +0x90  mabUsedSlot
    //
    // ⚠️⚠️ `lfTimeStep` IS ACCEPTED AND NEVER READ, and maScaledDynamics gets an UNSCALED
    // copy. The 62-instruction body contains no lfs / stfs / fmuls at all and its only `bl`
    // is the __savegprlr/__restgprlr pair, so ScaleOneDriveForTimeStep is not called from
    // here. The caller genuinely computes and passes the value (`lfs f1, 0xA0(mpSimulation)`
    // at 0x828A4DD8). Two copies of the same 32 bytes is what shipped. ⛔ DO NOT "fix" this
    // into a scale -- inventing a multiply in the drive dynamics is fabricating handling.
    s32 DriveData::AddDrive(rw_physics::Drive* lpDrive, const DriveFrames& lrFrames,
                            const DriveDynamics& lrDynamics, DriveId lId, f32 lfTimeStep)
    {
        (void)lfTimeStep;   // X360 @0x828A0330 never reads it -- see the note above.

        s32 liIndex = 0;
        while (liIndex < KI_SIZE && mabUsedSlot[liIndex])
            ++liIndex;

        if (liIndex >= KI_SIZE)
            return -1;

        maGameIDs[liIndex]        = lId;         // stdx r7  -> (i+0x11)<<3
        maRWDrives[liIndex]       = lpDrive;     // stwx r4  -> (i+0x20)<<2   (NULL at the call site)
        maFrames[liIndex]         = lrFrames;    // 4x lvx128/stvx128 -> i<<6
        maDynamics[liIndex]       = lrDynamics;  // 4x ld/std -> (i+2)<<5
        maScaledDynamics[liIndex] = lrDynamics;  // 4x ld/std from the SAME source -> (i+3)<<5
        mabUsedSlot[liIndex]      = true;        // stb 1, 0x90(this+i)  -- LAST, as the asm does
        return liIndex;
    }

    // DWARF h:214. No out-of-line symbol -- inlined into ProcessRemoveDriveQueue at
    // 0x828A00A8..0x828A00D0: the `liIndex < knSize` tripwire (.cpp:2803) and then the single
    // `stb r25, 0x47C0(r11)` with r11 = this + liIndex, i.e. mabUsedSlot[liIndex] = false.
    //
    // ⚠️ IT CLEARS THE USED FLAG AND NOTHING ELSE. maRWDrives[liIndex] is deliberately left
    // pointing at the drive the caller is about to hand to Simulation::RemoveDrive -- the
    // caller reads the slot BEFORE calling this, and the flag is what makes the slot
    // re-allocatable. Zeroing the pointer "to be tidy" would be inventing a store.
    bool DriveData::RemoveDrive(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");   // .cpp:2803 tripwire (blt skips)
        mabUsedSlot[liIndex] = false;
        return true;
    }

    // [INFERRED NAME] -- the write-back half of GetDrive. See the header for why the write is
    // attested (ProcessAddDriveQueue @0x828A4EB0) even though the DWARF carries no such method.
    void DriveData::SetDrive(s32 liIndex, rw_physics::Drive* lpRWDrive)
    {
        maRWDrives[liIndex] = lpRWDrive;
    }

    // ===================== RigidBodyData ===================================

    // The `const RigidBodyId K_INVALID_RIGID_BODY_ID = { ~0ull };` definition that used to sit
    // here went with the RigidBodyId de-fork (task #141). The sentinel now comes from its real
    // home, CgsRigidBody.h:41, as the header-scope `static const` the console's per-TU copies
    // (qword_82F33E18 here, qword_82F2A3A8 in the vehicle-manager TU) actually attest.

    // Host-stable element strides (X360-attested).
    static_assert(sizeof(Inertia)     == 48, "Inertia stride 48 (GetInertia 48*(idx+50))");
    static_assert(sizeof(RigidBodyId) == 8,  "RigidBodyId is a single u64 (CgsRigidBody.h:48)");

    // The seven Inertia-interior offsetof pins that used to sit here MOVED to
    // rw/physics/inertia.h (_rw_physics_Inertia_AssertLayout) with the de-fork, task #141.
    // They gated `CgsPhysics::Inertia`, which no longer exists as a type of its own; leaving
    // them here would have pinned an alias from a TU that does not own the layout. Their new
    // home also gained the adjacency form of the same six seats.

    // X360 @0x827DB728 (0x70 bytes). ⚠️ ABSENT from the .ida-exports JSON set (an export
    // hole -- 0x827DB720 is CgsDev::Log::LogOutput::Append, a 2-instruction tail jump that
    // ends exactly at 0x827DB728); decoded straight out of BURNOUT_X360_ARTIST.XEX.i64.
    //
    // A 200-pass loop, stride 0x30, over &maInertias[0] (== this + 0x960) -- i.e.
    // rw::physics::Inertia::Inertia() (DWARF inertia.h:80) inlined per entry. It writes
    // ONLY maInertias: maRWBodies and maGameIDs are left untouched by the constructor
    // (maGameIDs is seeded separately by Clear(), from Construct).
    //
    // The three float pool constants were read out of the image as raw bytes, not inferred:
    //   flt_82001C98 = 3f800000 = 1.0f      (f0)
    //   flt_820CD79C = 7f7fffff = FLT_MAX   (f12)
    //   flt_82001CC0 = 00000000 = 0.0f      (f13)
    // The 16-byte stack vector the loop lvx128/stvx128's into mInvTens is built from three
    // `stfs f0` plus one `stw 0`, i.e. {1.0f, 1.0f, 1.0f, 0.0f}.
    //
    // ⚠️⚠️ THIS BODY IS NOT EMPTY AND IT IS NOT A STUB -- READ THIS BEFORE "FIXING" IT.
    // Until the de-fork (task #141) the loop was written out longhand right here, because
    // `CgsPhysics::Inertia` was a local struct with public members. Now that the member array
    // is 200 real `rw::physics::Inertia`, the IMPLICIT member-array construction that runs
    // before this brace IS the console's 200-pass loop: it calls rw::physics::Inertia::Inertia()
    // (DWARF inertia.h:80) once per entry, and that ctor carries the ten stores above,
    // provenance comments and all. Writing the loop out again here would run the whole
    // initialisation TWICE. maRWBodies / maGameIDs stay untouched either way, which is what
    // the console does.
    RigidBodyData::RigidBodyData()
    {
        // ---- layout pins. maRWBodies is a pointer array, so everything past it widens
        // on x64; each offset is therefore pinned to the one before it plus the element
        // count times the element size, which is exactly what the console arithmetic is.
        // X360 closure, for the record: 200*4 + 200*8 + 200*48 == 12000 == 0x3110 - 0x230.
        static_assert(offsetof(RigidBodyData, maRWBodies) == 0, "maRWBodies @+0x00");
        static_assert(offsetof(RigidBodyData, maGameIDs)  == KI_SIZE * sizeof(rw_physics::RigidBody*),
                      "maGameIDs follows the 200 RigidBody* slots (X360 +0x320 at 4-byte pointers)");
        static_assert(offsetof(RigidBodyData, maInertias) == offsetof(RigidBodyData, maGameIDs) + KI_SIZE * sizeof(RigidBodyId),
                      "maInertias follows the 200 RigidBodyIds (X360 +0x960; asm addi r11, r3, 0x960)");
        static_assert(sizeof(RigidBodyData) == offsetof(RigidBodyData, maInertias) + KI_SIZE * sizeof(Inertia),
                      "RigidBodyData ends with the 200 Inertias -- no trailing member");
    }

    // X360: inlined into PhysicsSimulationModule::Construct @0x828A1F0C..0x828A1F34 as a
    // 200-pass `std` of qword_82F33E18 (image bytes ffffffffffffffff) over this+0x550
    // (== mBodyData + 0x320 == &maGameIDs[0]). Nothing else is written -- this table has
    // no mabUsedSlot[]; the sentinel game id IS the free-slot marker.
    void RigidBodyData::Clear()
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            maGameIDs[li] = K_INVALID_RIGID_BODY_ID;   // ld r8, qword_82F33E18; std r8, 0(r9); addi r9, r9, 8
        }
    }

    // X360 @0x8289CF78. Checked slot read returning maGameIDs[liIndex] (by value; the X360 ABI
    // returns the 8-byte RigidBodyId via a hidden sret ptr).
    RigidBodyId RigidBodyData::GetGameID(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");                              // h:585 tripwire (blt skips)
        CGS_ASSERT(!maGameIDs[liIndex].IsInvalid(), "!maGameIDs[liIndex].IsInvalid()"); // h:586 tripwire
        return maGameIDs[liIndex];
    }

    // X360 @0x8289D020. Checked slot read returning maRWBodies[liIndex].
    rw_physics::RigidBody* RigidBodyData::GetRigidBody(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");                              // h:594 tripwire (blt skips)
        CGS_ASSERT(!maGameIDs[liIndex].IsInvalid(), "!maGameIDs[liIndex].IsInvalid()"); // h:595 tripwire
        return maRWBodies[liIndex];
    }

    // X360 @0x8289D0C0. Checked slot read returning &maInertias[liIndex].
    Inertia* RigidBodyData::GetInertia(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");                              // h:603 tripwire (blt skips)
        CGS_ASSERT(!maGameIDs[liIndex].IsInvalid(), "!maGameIDs[liIndex].IsInvalid()"); // h:604 tripwire
        return &maInertias[liIndex];
    }

    // DWARF h:113. Inlined into AllocateMemoryAndInitialiseRW: the console loads the slot's
    // game id and compares it against qword_82F33E18 (K_INVALID_RIGID_BODY_ID) at 0x828A25FC.
    // This table has no mabUsedSlot[] -- the sentinel id IS the free marker.
    bool RigidBodyData::IsSlotUsed(s32 liIndex) const
    {
        return !maGameIDs[liIndex].IsInvalid();
    }

    // DWARF h:108. Inlined into AllocateMemoryAndInitialiseRW as the write-back of the
    // sentinel at 0x828A26C0. Its bounds tripwire is the one whose asm literals name
    // "..\..\..\GameShared\GameClasses\Physics/CgsPhysicsSimulationModule.cpp", 2504 --
    // i.e. it fires from the caller's frame, which is what being inlined there means.
    void RigidBodyData::SetFree(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");
        maGameIDs[liIndex] = K_INVALID_RIGID_BODY_ID;
    }

    // X360 @0x8289D2E8 (62 instructions) -- an .ida-exports HOLE, recovered headless from
    // BURNOUT_X360_ARTIST.XEX.i64 (task #140). Transcribed instruction for instruction.
    //
    // ⚠️ THE WALK IS UNBOUNDED AND THE TWO CHECKS ARE FIRE-AND-CONTINUE. The console emits
    //     r31 = -1 ; do { r10 += 8 ; ++r31 ; } while (*r10 != K_INVALID)
    // with NO index test inside the loop, then two asserts that do not gate anything. Both
    // are kept exactly as they are: the second (`liIndex != -1`) is unreachable in practice
    // because r31 is >= 0 on every exit, but it is in the binary and its rodata string is
    // distinct from the first ("Couldn't find a free rigid body slot" vs "Couldn't find free
    // rigid body slot"), so it is not a decode artefact.
    //
    // ⭐ THE THREE STORES ARE WHY THIS IS WRITTEN BY NAME, NOT BY OFFSET. The console does
    // `stdx` at (i+100)*8, `stwx` at i*4 and a 6x ld/std block copy at (i+50)*48 -- i.e. it
    // folds the three array BASES into the index arithmetic using the CONSOLE's 4-byte
    // pointer stride. maRWBodies[] widens to 8 bytes on x64, so every one of those literals
    // is wrong here; expressed as array subscripts they are right on both.
    s32 RigidBodyData::AddBody(rw_physics::RigidBody* lpRWBody, RigidBodyId lId, const Inertia& lrInertia)
    {
        s32 liIndex = -1;
        do
        {
            ++liIndex;
        }
        while (!maGameIDs[liIndex].IsInvalid());   // `ld r11,0(r10)` ; `cmpld r11,K_INVALID`

        CGS_ASSERT(liIndex < KI_SIZE, "Couldn't find free rigid body slot");    // .cpp:2483
        CGS_ASSERT(liIndex != -1, "Couldn't find a free rigid body slot");      // .cpp:2484

        maGameIDs[liIndex]  = lId;         // stdx  r27 at (i+100)*8  == +0x320 + i*8
        maRWBodies[liIndex] = lpRWBody;    // stwx  r28 at i*4        == +0x000 + i*4 (console stride)
        maInertias[liIndex] = lrInertia;   // 6 x ld/std at (i+50)*48 == +0x960 + i*48

        return liIndex;                    // mr r3, r31
    }

    // X360-attested write, [INFERRED NAME] -- see the declaration. The console open-codes this
    // store inside ProcessAddRigidBodyQueue @0x828A2BA0; here it is a named one-liner so the
    // 4-byte pointer stride the console folds into its index arithmetic cannot leak in.
    void RigidBodyData::SetRigidBody(s32 liIndex, rw_physics::RigidBody* lpRWBody)
    {
        maRWBodies[liIndex] = lpRWBody;
    }

    // DWARF h:96. X360 @0x828A0100 (140 instructions) -- by far the most-called member of this
    // class: TWELVE call sites across the drains (ProcessRemoveRigidBody, ProcessAddContact x2,
    // ProcessUpdateRigidBody, ProcessUpdateExternalBody, ProcessAddJoint x2,
    // ProcessSetRigidBodySpy, ProcessChangeRigidBodyInertia, ProcessAddDrive x2,
    // ProcessApplyForce), which is why it is out-of-line here while the DriveData sibling is
    // inlined everywhere.
    //
    // The search itself is nine instructions (0x828A0118..0x828A014C): a bare linear compare
    // of maGameIDs[] against the handle, `ld` at an 8-byte stride from this+0x320.
    //
    // ⚠️ IT DOES **NOT** TEST A USED-SLOT FLAG, unlike DriveData::GetIndexFromGameID. That is
    // not an omission: RigidBodyData has no mabUsedSlot[] array at all -- a free slot holds
    // the K_INVALID_RIGID_BODY_ID sentinel, so a match on a real id already implies the slot
    // is live. The two post-match asserts below re-check exactly that.
    //
    // The remaining ~120 instructions are three diagnostics. All three are reconstructed as
    // CGS_ASSERTs in the established style of this file (the id that the console formats into
    // the message via StrStreamBase is dropped; the message text is kept verbatim, including
    // its original spelling):
    //   * .cpp:2537 "Bad search"  -- fires ONLY when the miss is on an id whose owner field is
    //     3. The console computes that owner as `srdi r11,r30,32` then `srwi r11,r11,24`,
    //     i.e. bits 56..63 of the 64-bit handle. ⚠️ Recorded precisely because that is NOT
    //     the same as a `>>24` on a 32-bit value -- see the open RigidBodyId item.
    //   * .cpp:2544 "ID's dont match" and .cpp:2545 "Slot not used" -- both on the HIT path.
    //
    // ⚠️ NOT MODELLED, and recorded here rather than left unmentioned: a second, flag-gated debug
    // print at 0x828A01EC..0x828A0220 on the miss path, which re-streams the same "Bad search"
    // text to a global stream object (`off_82F31904`) when bit 0 of `qword_82F31908` is set.
    // It is a log write with no effect on the return value, and modelling it would mean
    // inventing a logging entry point this tree does not have.
    s32 RigidBodyData::GetIndexFromGameID(RigidBodyId lId)
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            if (static_cast<u64>(maGameIDs[li]) == static_cast<u64>(lId))
            {
                // 0x828A0230.. -- the console re-reads the slot and re-checks it, which is
                // only reachable when the table was mutated under it.
                CGS_ASSERT(static_cast<u64>(maGameIDs[li]) == static_cast<u64>(lId),
                           "ID's dont match: Andy H has meesed up the physics: ");   // .cpp:2544
                CGS_ASSERT(!maGameIDs[li].IsInvalid(),
                           "Slot not used: Andy H has meesed up the physics: ");     // .cpp:2545
                return li;
            }
        }

        // 0x828A0150 -- the owner-gated miss diagnostic. `srdi 32` then `srwi 24` == the top
        // byte of the 64-bit handle; the console only complains for owner 3.
        if (static_cast<u32>(static_cast<u64>(lId) >> 56) == 3u)
        {
            CGS_ASSERT(false, "\nPhysics: Bad search for a Rigid body ");            // .cpp:2537
        }
        return -1;
    }

    // ===================== PhysicsSimulationModule =========================

    // X360 @0x827DF1E0 (51 instructions). Store-for-store:
    //     *this = &off_820CE500                       base ModuleSingleBuffered vtable
    //     RWMutex(this+0x10,  0, 1)                   base mInputBuffer's mutex
    //     RWMutex(this+0x118, 0, 1)                   base mOutputBuffer's mutex
    //     *this = off_820CF7D0                        this class's own vtable
    //     RigidBodyData::RigidBodyData(this+0x230)    mBodyData
    //     JointData::JointData(this+0x3110)           mJointData
    //     <DriveData's ctor, inlined>                 mDriveData
    // In C++ every one of those is implicit: the two vtable stamps + the two mutex
    // constructions ARE the base sub-object's own construction, and the three chained
    // sub-constructors are the member-construction order for mBodyData / mJointData /
    // mDriveData, which is the declaration order in the header. So the body is empty --
    // and, importantly, EMPTY IS NOT A STUB HERE: 200 Inertias, 36 JointLimits and 2
    // DriveDynamics really are initialised on construction, because each of those three
    // member types declares a constructor and the compiler chains all three. Take any of
    // those constructors away and the initialisation disappears with no diagnostic.
    //
    // The console does NOT touch mePrepareStage / meReleaseStage / the PairSet pointers /
    // the BitArrays / the counters / the monitor handles here -- those are Construct()'s
    // job, and Construct() is a separate virtual the owner calls explicitly.
    PhysicsSimulationModule::PhysicsSimulationModule()
    {
    }

    // X360 @0x828A1EE8 (86 instructions; 5 calls -- ModuleSingleBuffered::Construct once
    // and PerfMonCpu::AddMonitor four times; no asserts). Console vtable slot 0.
    //
    // ⚠️ Hex-Rays' pseudocode for this function is garbage in two places: it renders the
    // twelve `std r11, ...` (r11 == 0) as `HIDWORD(v3) = 0x82000000` and
    // `HIDWORD(v3) = " liEndColumn)"`. The asm is unambiguous -- r11 is loaded with 0 at
    // 0x828A1F04 and is not written again until 0x828A1FBC, well after the last store, so
    // all twelve are 8-byte ZERO stores. Transcribed from the asm.
    void PhysicsSimulationModule::Construct()
    {
        ModuleSingleBuffered::Construct();               // bl @0x828A1F00

        mePrepareStage = PREPARESTAGE_START;             // stw r11(=0), 0x228(r31)
        meReleaseStage = RELEASESTAGE_DONE;              // stw r8(=2),  0x22C(r31)

        mBodyData.Clear();                               // 200 x std K_INVALID_RIGID_BODY_ID -> +0x550
        mJointData.Clear();                              // 36  x stb 0                       -> +0x4700
        mDriveData.Clear();                              // 1   x stb 0                       -> +0x47C0

        // Twelve `std r11(=0)` at +0x47E8..+0x4840 == three BitArray<200>, four 64-bit
        // fields each. (The DWARF also attests BitArray::Construct(); it and UnSetAll()
        // emit the identical four zero stores, and UnSetAll is the one this tree's
        // template already has.)
        mNeedFreeze.UnSetAll();                          // +0x47E8
        mDone.UnSetAll();                                // +0x4808
        mSeen.UnSetAll();                                // +0x4828

        miNumRigidBodies = 0;                            // stw r11(=0), 0x4848(r31)
        miNumDrives      = 0;                            // stw r11(=0), 0x484C(r31)
        miNumJoints      = 0;                            // stw r11(=0), 0x4850(r31)

        // Four identical AddMonitor calls: r3 = name, r4 = 4 (page), r5 = 0 (minimum),
        // f1 = flt_82004A20 (image bytes 41200000 == 10.0f, the CPU budget), r7 = 1
        // (libperf-tagged). r6 is NEVER written -- on the PPC ABI the float argument
        // consumes that integer slot, which is the whole reason Hex-Rays invents a
        // spurious sixth parameter here. This is the 5-parameter console signature.
        miTimeInSim1 = CgsDev::PerfMonCpu::AddMonitor("Sim setup",    CgsDev::E_PMP_4, false, 10.0f, true);
        miTimeInSim2 = CgsDev::PerfMonCpu::AddMonitor("Sim freezing", CgsDev::E_PMP_4, false, 10.0f, true);
        miTimeInSim3 = CgsDev::PerfMonCpu::AddMonitor("Sim update",   CgsDev::E_PMP_4, false, 10.0f, true);
        miTimeInSim4 = CgsDev::PerfMonCpu::AddMonitor("Sim output",   CgsDev::E_PMP_4, false, 10.0f, true);

        mbIsNewModule = true;                            // stb r11(=1), 4(r31)
    }

    // X360 @0x828A2048 (54 instructions). Console vtable slot 2 -- the release FSM. It is a
    // FALL-THROUGH switch on meReleaseStage: stage 0 advances the cursor and falls into stage 1;
    // stage 1 releases the base and, on success, clears the three bit arrays and the three
    // counters and advances the cursor again, falling into stage 2; stage 2 resets mePrepareStage
    // and reports done. Stage >= 3 is unreachable and trips.
    //
    // ⚠️ The stage-1 EARLY RETURN is the load-bearing part: when ModuleSingleBuffered::Release()
    // returns false the console returns false with the cursor LEFT ON STAGE 1, so the owner
    // re-enters next frame and retries. Returning true there (or clearing the tables anyway)
    // would silently release a module that had not finished releasing.
    bool PhysicsSimulationModule::Release()
    {
        switch (meReleaseStage)
        {
        case RELEASESTAGE_START:                   // blt cr6 (meReleaseStage < 1)
            meReleaseStage++;                      // bl CgsPhysics::operator++ @0x828A20A4
            // fall through -- the console has no branch here
        case RELEASESTAGE_MANAGER:                 // beq cr6 (meReleaseStage == 1)
            if (!ModuleSingleBuffered::Release())  // bl @0x828A20AC; clrlwi/cmplwi/bne
                return false;                      // li r3, 0 @0x828A20BC

            // The same twelve `std 0` + three `stw 0` block Construct writes.
            mNeedFreeze.UnSetAll();                // +0x47E8
            mDone.UnSetAll();                      // +0x4808
            mSeen.UnSetAll();                      // +0x4828
            miNumRigidBodies = 0;                  // stw r30(=0), 0x4848(r31)
            miNumDrives      = 0;                  // stw r30(=0), 0x484C(r31)
            miNumJoints      = 0;                  // stw r30(=0), 0x4850(r31)

            meReleaseStage++;                      // bl CgsPhysics::operator++ @0x828A210C
            // fall through
        case RELEASESTAGE_DONE:                    // blt cr6 (meReleaseStage < 3)
            mePrepareStage = PREPARESTAGE_START;   // stw r30(=0), 0x228(r31)
            return true;                           // li r3, 1 @0x828A2110
        }

        // The console's out-of-range arm fires an assert whose message/file operands are both
        // literal 0 and whose line operand is literal -1 (`li r3,0 ; li r4,0 ; li r5,-1` at
        // 0x828A207C..0x828A2084) -- i.e. it carries no text of its own. CGS_ASSERT supplies its
        // own __FILE__/__LINE__, so the condition is reproduced and the message is left describing
        // the tripwire rather than inventing console text.
        CGS_ASSERT(false, "meReleaseStage out of range");
        return false;                              // li r3, 0 @0x828A2090
    }

    // X360 @0x828A2120. Console vtable slot 3.
    //
    // ⚠️ EXPORT-SET HOLE -- this function is absent from .ida-exports (the JSON for the address
    // does not exist; 0x828A2168 is the next exported symbol, AllocateMemoryAndInitialiseRW).
    // Recovered with headless IDA 9.3 against a COPY of BURNOUT_X360_ARTIST.XEX.i64, same as
    // RigidBodyData::RigidBodyData. It is sixteen instructions: the identical twelve `std 0` +
    // three `stw 0` block, then an unconditional `b` (a tail jump, not a call) to
    // ModuleSingleBuffered::Destruct.
    void PhysicsSimulationModule::Destruct()
    {
        mNeedFreeze.UnSetAll();                    // std 0 x4 at +0x47E8
        mDone.UnSetAll();                          // std 0 x4 at +0x4808
        mSeen.UnSetAll();                          // std 0 x4 at +0x4828
        miNumRigidBodies = 0;                      // stw r11(=0), 0x4848(r3)
        miNumDrives      = 0;                      // stw r11(=0), 0x484C(r3)
        miNumJoints      = 0;                      // stw r11(=0), 0x4850(r3)

        ModuleSingleBuffered::Destruct();          // b @0x828A2160 (tail jump)
    }

    // -------------------------------------------------------------------------------
    // Layout pins for PhysicsSimulationModule. Never called; exists only to host the
    // offsetof asserts (offsetof on a private member must be evaluated in member-function
    // scope). Same pattern as VehicleManager::_AssertLayout.
    //
    // The X360 offsets are 4-byte-pointer offsets and the host builds x64, so the ABSOLUTE
    // console offsets past a pointer member cannot be asserted. What IS asserted is the
    // adjacency chain -- each member sits immediately after the previous one -- which is
    // what actually encodes the console's member order, and which fails if a member is
    // inserted, removed, reordered or mis-sized.
    //
    // X360 closure, for the record (all read off the asm):
    //   +0x228 mePrepareStage, +0x22C meReleaseStage,
    //   +0x230 mBodyData(12000), +0x3110 mJointData(5664), +0x4730 mDriveData(160),
    //   +0x47D0 3xPairSet* + miNeedThaw + miActive + mpiNextIndex (24),
    //   +0x47E8 3xBitArray<200> (96), +0x4848 3 counters, +0x4854 4 monitor handles,
    //   +0x4864 mpSimulation -> pad to 0x4870 == 18544 == 0x4AA0 - 0x230.
    // -------------------------------------------------------------------------------
    void PhysicsSimulationModule::_AssertLayout()
    {
        static_assert(sizeof(CgsContainers::BitArray<KU_NUM_BODIES>) == 32,
                      "BitArray<200> is four 64-bit fields (the 12 std 0 at +0x47E8..+0x4840)");

        // The head: the two stage words are adjacent, and mBodyData starts the 16-aligned run.
        static_assert(offsetof(PhysicsSimulationModule, meReleaseStage) == offsetof(PhysicsSimulationModule, mePrepareStage) + 4,
                      "meReleaseStage immediately follows mePrepareStage (asm 0x228 / 0x22C)");
        static_assert(offsetof(PhysicsSimulationModule, mBodyData) % 16 == 0,
                      "mBodyData must land 16-aligned (its Inertia elements are alignas(16))");

        // The three slot tables are contiguous, in this order, with nothing between them.
        static_assert(offsetof(PhysicsSimulationModule, mJointData) == offsetof(PhysicsSimulationModule, mBodyData) + sizeof(RigidBodyData),
                      "mJointData immediately follows mBodyData (X360 0x3110 == 0x230 + 12000)");
        static_assert(offsetof(PhysicsSimulationModule, mDriveData) == offsetof(PhysicsSimulationModule, mJointData) + sizeof(JointData),
                      "mDriveData immediately follows mJointData (X360 0x4730 == 0x3110 + 5664)");

        // The pointer/scalar block (X360 +0x47D0, 24 bytes at 4-byte pointers).
        static_assert(offsetof(PhysicsSimulationModule, mpJointedPairs) == offsetof(PhysicsSimulationModule, mDriveData) + sizeof(DriveData),
                      "mpJointedPairs immediately follows mDriveData (X360 0x47D0 == 0x4730 + 160)");
        static_assert(offsetof(PhysicsSimulationModule, mpDrivenPairs)  == offsetof(PhysicsSimulationModule, mpJointedPairs) + sizeof(rw::physics::PairSet*),
                      "mpDrivenPairs follows mpJointedPairs");
        static_assert(offsetof(PhysicsSimulationModule, mpContactPairs) == offsetof(PhysicsSimulationModule, mpDrivenPairs) + sizeof(rw::physics::PairSet*),
                      "mpContactPairs follows mpDrivenPairs");
        static_assert(offsetof(PhysicsSimulationModule, miNeedThaw)     == offsetof(PhysicsSimulationModule, mpContactPairs) + sizeof(rw::physics::PairSet*),
                      "miNeedThaw follows the three PairSet pointers");
        static_assert(offsetof(PhysicsSimulationModule, miActive)       == offsetof(PhysicsSimulationModule, miNeedThaw) + 4,
                      "miActive follows miNeedThaw");
        static_assert(offsetof(PhysicsSimulationModule, mpiNextIndex)   >= offsetof(PhysicsSimulationModule, miActive) + 4
                      && offsetof(PhysicsSimulationModule, mpiNextIndex) % sizeof(s32*) == 0,
                      "mpiNextIndex follows miActive at its natural pointer alignment");

        // The three bit arrays -- the twelve zero stores.
        static_assert(offsetof(PhysicsSimulationModule, mNeedFreeze) == offsetof(PhysicsSimulationModule, mpiNextIndex) + sizeof(s32*),
                      "mNeedFreeze follows mpiNextIndex (X360 0x47E8)");
        static_assert(offsetof(PhysicsSimulationModule, mDone) == offsetof(PhysicsSimulationModule, mNeedFreeze) + 32,
                      "mDone follows mNeedFreeze (X360 0x4808)");
        static_assert(offsetof(PhysicsSimulationModule, mSeen) == offsetof(PhysicsSimulationModule, mDone) + 32,
                      "mSeen follows mDone (X360 0x4828)");

        // The three counters -- the three `stw 0`.
        static_assert(offsetof(PhysicsSimulationModule, miNumRigidBodies) == offsetof(PhysicsSimulationModule, mSeen) + 32,
                      "miNumRigidBodies follows mSeen (X360 0x4848)");
        static_assert(offsetof(PhysicsSimulationModule, miNumDrives) == offsetof(PhysicsSimulationModule, miNumRigidBodies) + 4,
                      "miNumDrives follows miNumRigidBodies (X360 0x484C)");
        static_assert(offsetof(PhysicsSimulationModule, miNumJoints) == offsetof(PhysicsSimulationModule, miNumDrives) + 4,
                      "miNumJoints follows miNumDrives (X360 0x4850)");

        // The four AddMonitor handles, in call order.
        static_assert(offsetof(PhysicsSimulationModule, miTimeInSim1) == offsetof(PhysicsSimulationModule, miNumJoints) + 4,
                      "miTimeInSim1 follows miNumJoints (X360 0x4854, the 'Sim setup' handle)");
        static_assert(offsetof(PhysicsSimulationModule, miTimeInSim2) == offsetof(PhysicsSimulationModule, miTimeInSim1) + 4,
                      "miTimeInSim2 (X360 0x4858, 'Sim freezing')");
        static_assert(offsetof(PhysicsSimulationModule, miTimeInSim3) == offsetof(PhysicsSimulationModule, miTimeInSim2) + 4,
                      "miTimeInSim3 (X360 0x485C, 'Sim update')");
        static_assert(offsetof(PhysicsSimulationModule, miTimeInSim4) == offsetof(PhysicsSimulationModule, miTimeInSim3) + 4,
                      "miTimeInSim4 (X360 0x4860, 'Sim output')");

        // The tail.
        static_assert(offsetof(PhysicsSimulationModule, mpSimulation) >= offsetof(PhysicsSimulationModule, miTimeInSim4) + 4
                      && offsetof(PhysicsSimulationModule, mpSimulation) % sizeof(void*) == 0,
                      "mpSimulation is the last member, after the four handles (X360 0x4864)");
        static_assert(sizeof(PhysicsSimulationModule) >= offsetof(PhysicsSimulationModule, mpSimulation) + sizeof(void*),
                      "nothing is declared after mpSimulation");
    }

    // X360 0x8289CF18 (DWARF CgsPhysicsSimulationModule.h:566). Post-increment on
    // PhysicsSimulationModule::EReleaseStage: read old, advance the referenced cursor by one,
    // tripwire that it did not overrun RELEASESTAGE_DONE, return the pre-increment value.
    // Store-for-store: r31=old; *a1=old+1 (unconditional); assert new<=2; return old.
    PhysicsSimulationModule::EReleaseStage
    operator++(PhysicsSimulationModule::EReleaseStage& leEnumIndex, int)
    {
        PhysicsSimulationModule::EReleaseStage leOld = leEnumIndex;
        leEnumIndex = static_cast<PhysicsSimulationModule::EReleaseStage>(
            static_cast<int>(leEnumIndex) + 1);
        CGS_ASSERT(leEnumIndex <= PhysicsSimulationModule::RELEASESTAGE_DONE,
                   "leEnumIndex <= PhysicsSimulationModule::RELEASESTAGE_DONE");
        return leOld;
    }

    // DWARF CgsPhysicsSimulationModule.h:565. The prepare-stage twin of the operator above;
    // the console inlined it into Prepare (`v8 = *(a1+552) + 1; *(a1+552) = v8; if (v8 > 2)
    // <assert>`), which is the same three steps in the same order.
    PhysicsSimulationModule::EPrepareStage
    operator++(PhysicsSimulationModule::EPrepareStage& leEnumIndex, int)
    {
        PhysicsSimulationModule::EPrepareStage leOld = leEnumIndex;
        leEnumIndex = static_cast<PhysicsSimulationModule::EPrepareStage>(
            static_cast<int>(leEnumIndex) + 1);
        CGS_ASSERT(leEnumIndex <= PhysicsSimulationModule::PREPARESTAGE_DONE,
                   "leEnumIndex <= PhysicsSimulationModule::PREPARESTAGE_DONE");
        return leOld;
    }

    // ===================================================================================
    // PhysicsSimulationModule::Prepare @ 0x828A6A08 -- console vtable slot 16.
    //
    // ⭐ THIS IS THE FUNCTION THAT ASSIGNS mpSimulation. Until it landed (2026-08-04, task
    // #135) `rw::physics::Simulation` had exactly one holder in the whole tree and NO
    // assignment anywhere: the reconstructed solver -- DynamicUpdate, BatchIntegrator, both
    // Jacobian::Builds, the 384-byte record -- linked and was unreachable, and /OPT:REF
    // stripped every byte of it back out of the exe.
    //
    // Shape: a fall-through FSM over mePrepareStage, the mirror image of Release().
    //   stage 0 (START)   : the console does `*(a1+552) = 0` then falls into the stage-1 arm
    //                       via the shared cursor bump. Modelled as the bump + fall-through.
    //   stage 1 (MANAGER) : base Prepare first; if it is not finished, return false with the
    //                       cursor LEFT ON 1 so the owner re-enters next frame. On success:
    //                       clear the three bit arrays, (re)build the rw::physics world, clear
    //                       the three slot tables, reset the release cursor, bump to DONE.
    //   stage >= 3        : the console's out-of-range arm fires an assert whose three
    //                       operands are literal 0/0/-1 -- it carries no text of its own, so
    //                       the condition is reproduced and the message describes the tripwire.
    //
    // ⚠️ THE ORDER IS NOT THE ORDER Construct USES. Construct clears the three slot tables
    // FIRST and the bit arrays after; Prepare clears the bit arrays, then rebuilds the world,
    // then clears the slot tables. That matters: AllocateMemoryAndInitialiseRW walks
    // mBodyData/mJointData/mDriveData to decide what to REMOVE from an existing simulation,
    // so clearing them before it ran would silently leak every live body back into the
    // rw::physics free lists' predecessor. Transcribed in the asm's order.
    // ===================================================================================
    bool PhysicsSimulationModule::Prepare(rw::IResourceAllocator* lpAllocator,
                                          const SimulationParams& lrParams)
    {
        switch (mePrepareStage)
        {
        default:
            // `if (v4 >= 3) { BeginAssert; FireAssert(0, 0, -1); EndAssert; return 0; }`
            CGS_ASSERT(false, "mePrepareStage out of range");
            return false;

        case PREPARESTAGE_DONE:
            // The console's `*(a1 + 552) = 0` before it falls into the START arm: a module
            // that already finished preparing restarts from the top.
            mePrepareStage = PREPARESTAGE_START;
            // fall through

        case PREPARESTAGE_START:
            mePrepareStage++;
            // fall through -- the console has no branch here

        case PREPARESTAGE_MANAGER:
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;                       // cursor stays on MANAGER; retried next frame

            // The same twelve `std 0` block Construct and Release write, at +0x47E8.
            mNeedFreeze.UnSetAll();
            mDone.UnSetAll();
            mSeen.UnSetAll();

            AllocateMemoryAndInitialiseRW(lpAllocator, lrParams);

            mBodyData.Clear();                      // 200 x std K_INVALID_RIGID_BODY_ID -> +0x550
            mJointData.Clear();                     // 36  x stb 0                       -> +0x4700
            mDriveData.Clear();                     // 1   x stb 0                       -> +0x47C0

            meReleaseStage = RELEASESTAGE_START;    // stw 0, 0x22C(r31)
            mePrepareStage++;
            return true;
        }
    }

    namespace
    {
        // The console's `rw::IResourceAllocator::AllocateMemoryResource` @0x823FF7D0 and the
        // generic descriptor carve are both INLINES: they build a five-entry serialised
        // descriptor on the stack and tail-call the allocator's DoAllocate slot. The PC
        // rwcore models DoAllocate with the narrower <4> alias, so the descriptor is built as
        // <5> and reinterpret_cast down at the call -- the same idiom
        // CgsSceneManager::TriangleCacheManager::Prepare and rwgpfxtint.cpp already use.
        void* CarveResource(rw::IResourceAllocator* lpAllocator,
                            const rw::BaseResourceDescriptors<5>& lrDescriptor)
        {
            rw::Resource lResource = lpAllocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lrDescriptor), 0);
            return lResource.m_baseResources[0];
        }

        // rw::IResourceAllocator::AllocateMemoryResource(size, alignment) -- a {size,align}
        // pair in entry[0], identity in the rest, then the same carve.
        void* AllocateMemoryResource(rw::IResourceAllocator* lpAllocator, u32 luSize, u32 luAlignment)
        {
            rw::BaseResourceDescriptors<5> lDescriptor;
            for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
            {
                lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
                lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
            }
            lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
            return CarveResource(lpAllocator, lDescriptor);
        }
    }

    // ===================================================================================
    // PhysicsSimulationModule::AllocateMemoryAndInitialiseRW @ 0x828A2168
    //
    // Two paths, chosen on `if (mpSimulation)`:
    //
    //   FIRST PREPARE (mpSimulation == NULL) -- build the whole rw::physics world:
    //     Simulation           : GetResourceDescriptor(200,36,1) -> DoAllocate -> Initialize
    //     SimulationWorkspace  : GetResourceDescriptor(36,1,1024) -> DoAllocate -> Initialize
    //                            -> Simulation::SetWorkspace(ws, 36, 1, 1024)
    //     solver parameters    : spy mode = all three; gravity / freezing energy / iteration
    //                            cap from the caller's SimulationParams
    //     three PairSets       : contact (200,1024), jointed (200,36), driven (200,1)
    //     mpiNextIndex         : 800 bytes / align 4 == 200 x s32
    //
    //   RE-PREPARE (mpSimulation != NULL) -- the allocator is a BUMP allocator and cannot
    //     hand the same block back, so the world is emptied instead of rebuilt: every used
    //     joint slot, the drive slot, and every used body slot are removed from the
    //     simulation and the body slots freed. Nothing is deallocated.
    //
    //   BOTH paths then clear the three pair sets and reset the counters/cursors.
    //
    // ⚠️ THE SPY MODE IS SPY_JOINTS|SPY_DRIVES|SPY_CONTACTS (7), NOT SPY_NOTHING. The console
    // writes literal 7 into +0xB0 here, overriding the SPY_NOTHING that Simulation::Initialize
    // had just written two calls earlier. That is not debug-only leftover: the contact spy
    // stream is how the game learns about collisions at all (AddContactSpiesToOutputQueue ->
    // the crash/deformation modules), so the flags are load-bearing gameplay state.
    //
    // ⚠️ THE COUNTS ARE THE CLASS'S OWN CONSTANTS, and they are NOT all the same constant.
    // The simulation is sized (bodies=200, joints=36, drives=1); the WORKSPACE is sized
    // (joints=36, drives=1, contacts=1024) -- kuNumPotentialContacts, a different member --
    // and the contact pair set is (200, kuNumCollidingPairs=1024). Three different 1024s
    // would have been indistinguishable if they had been written as literals.
    //
    // ⭐ EXECUTION PROVEN, NOT ASSUMED (2026-08-04, task #135). A temporary one-shot witness
    // was compiled into the tail of this body, observed on a default boot_test run, and
    // removed -- the same protocol PhysicsModule::Construct was proved with:
    //   [t135] AllocateMemoryAndInitialiseRW RAN; sim=0x19A66F50 freeRB=200 activeRB=0
    //          freeJT=36 freeDR=1 maxRB=200 grav=-9.810000 freezeE=0.100000 maxIter=2
    //          cool=30 spy=7 rf=0x19A67090 pairs=0x19ABADD0/0x19AC3110/0x19AC38D0
    //          nextIdx=0x19AC3C30 simSize=272 rbSize=240
    // Every field is an independent check, and all of them land:
    //   * freeRB/freeJT/freeDR are written by Simulation::Initialize, so the three intrusive
    //     free lists really were threaded (200/36/1 == the class's three capacities).
    //   * rf - sim == 0x140 == 320 == RoundUp(sizeof(Simulation)=272, 64) -- the block carve
    //     arithmetic is exact, which is the thing the console-stride widening had to get right.
    //   * cool=30 comes from Initialize's own defaults while grav/freezeE/maxIter come from
    //     PhysicsModule::Prepare stage 3's SimulationParams, so BOTH ends of the chain ran.
    //   * three distinct non-null PairSets and a non-null next-index array.
    // ===================================================================================
    void PhysicsSimulationModule::AllocateMemoryAndInitialiseRW(rw::IResourceAllocator* lpAllocator,
                                                                const SimulationParams& lrParams)
    {
        if (mpSimulation != 0)
        {
            // ---- RE-PREPARE: empty the existing world, keep its memory -------------------
            for (s32 liJoint = 0; liJoint < JointData::KI_SIZE; ++liJoint)
            {
                if (mJointData.IsSlotUsed(liJoint))
                    mpSimulation->RemoveJoint(mJointData.GetJoint(liJoint));
            }

            if (mDriveData.IsSlotUsed(0))
                mpSimulation->RemoveDrive(mDriveData.GetDrive(0));

            for (s32 liBody = 0; liBody < RigidBodyData::KI_SIZE; ++liBody)
            {
                if (mBodyData.IsSlotUsed(liBody))
                {
                    mpSimulation->RemoveRigidBody(mBodyData.GetRigidBody(liBody));
                    mBodyData.SetFree(liBody);
                }
            }
        }
        else
        {
            // ---- FIRST PREPARE: carve and initialise ------------------------------------
            rw::BaseResourceDescriptors<5> lDescriptor;

            // The simulation object and its whole node graph, in one block.
            rw::physics::Simulation::GetResourceDescriptor(
                &lDescriptor, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_JOINTS),
                static_cast<int>(KU_NUM_DRIVES));
            void* lpSimulationBlock = CarveResource(lpAllocator, lDescriptor);

            // The per-frame solver scratch arena.
            rw::physics::SimulationWorkspace::GetResourceDescriptor(
                &lDescriptor, static_cast<int>(KU_NUM_JOINTS), static_cast<int>(KU_NUM_DRIVES),
                static_cast<int>(KU_NUM_POTENTIAL_CONTACTS));
            void* lpWorkspaceBlock = CarveResource(lpAllocator, lDescriptor);

            rw::physics::SimulationWorkspace* lpWorkspace =
                rw::physics::SimulationWorkspace::Initialize(
                    &lpWorkspaceBlock, static_cast<int>(KU_NUM_JOINTS), static_cast<int>(KU_NUM_DRIVES),
                    static_cast<int>(KU_NUM_POTENTIAL_CONTACTS));

            mpSimulation = rw::physics::Simulation::Initialize(
                &lpSimulationBlock, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_JOINTS),
                static_cast<int>(KU_NUM_DRIVES));

            // ⚠️ Hex-Rays renders this call as `SetWorkspace()` with NO arguments -- the
            // dropped-argument artefact. The asm has r4..r7 live across it; the arguments are
            // the workspace and the same three counts its sizer was given.
            mpSimulation->SetWorkspace(lpWorkspace, static_cast<int>(KU_NUM_JOINTS),
                                       static_cast<int>(KU_NUM_DRIVES),
                                       static_cast<int>(KU_NUM_POTENTIAL_CONTACTS));

            mpSimulation->SetSpyingMode(static_cast<rw::physics::SpyingFlag>(   // stw 7, 0xB0
                rw::physics::SPY_JOINTS | rw::physics::SPY_DRIVES | rw::physics::SPY_CONTACTS));

            // `lvx128 v0,r0,r29 ; stvx128 v0,r11,144` -- the params' 16-byte gravity lane
            // straight into the simulation's, then the two scalars behind it.
            rw::math::vpu::Vector3 lGravity;
            lGravity.x = lrParams.mafGravity[0];
            lGravity.y = lrParams.mafGravity[1];
            lGravity.z = lrParams.mafGravity[2];
            lGravity.w = lrParams.mafGravity[3];
            mpSimulation->SetGravity(lGravity);                                 // +0x90
            mpSimulation->SetFreezingEnergy(lrParams.mfFreezingEnergy);         // +0xA8 <- params+16
            mpSimulation->SetMaxIteration(lrParams.muMaxIterations);            // +0xAC <- params+24

            // The three pair sets. ⚠️ The ORDER the console builds them in is contact,
            // jointed, driven -- but they are STORED +0x47D8, +0x47D0, +0x47D4, i.e. the
            // build order is not the member order. Kept as the asm has it.
            rw::physics::PairSet::GetResourceDescriptor(
                &lDescriptor, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_COLLIDING_PAIRS));
            void* lpContactPairsBlock = CarveResource(lpAllocator, lDescriptor);
            mpContactPairs = rw::physics::PairSet::Initialize(
                &lpContactPairsBlock, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_COLLIDING_PAIRS));

            rw::physics::PairSet::GetResourceDescriptor(
                &lDescriptor, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_JOINTS));
            void* lpJointedPairsBlock = CarveResource(lpAllocator, lDescriptor);
            mpJointedPairs = rw::physics::PairSet::Initialize(
                &lpJointedPairsBlock, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_JOINTS));

            rw::physics::PairSet::GetResourceDescriptor(
                &lDescriptor, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_DRIVES));
            void* lpDrivenPairsBlock = CarveResource(lpAllocator, lDescriptor);
            mpDrivenPairs = rw::physics::PairSet::Initialize(
                &lpDrivenPairsBlock, static_cast<int>(KU_NUM_BODIES), static_cast<int>(KU_NUM_DRIVES));

            // 800 bytes, 4-aligned == s32[200], one per body slot.
            mpiNextIndex = static_cast<s32*>(
                AllocateMemoryResource(lpAllocator, KU_NUM_BODIES * sizeof(s32), 4u));
        }

        mpContactPairs->ClearAll();
        mpJointedPairs->ClearAll();
        mpDrivenPairs->ClearAll();

        miNumRigidBodies = 0;      // +0x4848
        miNumDrives      = 0;      // +0x484C
        miNumJoints      = 0;      // +0x4850
        miNeedThaw       = -1;     // +0x47DC
        miActive         = -1;     // +0x47E0
    }

    // =====================================================================================
    // PhysicsSimulationModule::ProcessAddRigidBodyQueue @ 0x828A2708  (306 instructions)
    //
    // The first of the nineteen input drains ProcessInputBuffers @0x828A73C0 dispatches to
    // (task #140, 2026-08-04). ⚠️ NOTHING CALLS IT YET -- ProcessInputBuffers is not bodied,
    // and it cannot be bodied without all nineteen or it becomes a [[silent-drop-stubs]]
    // no-op. Stated here rather than implied, because "it links" is not "it runs".
    //
    // ✅ THE `CgsPhysics::Inertia` / `rw::physics::Inertia` TYPE FORK WAS RETIRED HERE ON
    // 2026-08-04 (task #141), the same way task #135 retired Joint/Drive/RigidBody. The console
    // has ONE type, and so does this tree now: `CgsPhysics::Inertia` is a typedef onto
    // `rw::physics::Inertia`, so the two `reinterpret_cast`s that used to sit in the AddBody /
    // AddRigidBody calls below are simply gone, along with the second copy of the layout and
    // its second set of pins.
    //
    // ⚠️⚠️ IT MATTERED MORE THAN IT LOOKED. `Inertia` never appears in a mangled name in a form
    // that encodes its definition, so a body compiled against one copy links CLEANLY against a
    // call site compiled against the other -- no compiler diagnostic, no linker diagnostic, and
    // no per-TU compile gate can see it. The path here is live: GetInertia's pointer goes
    // straight into rw::physics::Simulation::AddRigidBody, which stores it in
    // RigidBody::mInertia, which DynamicUpdate dereferences every tick. It happened to be
    // behaviourally correct (identical layout, same DWARF); "happened to be" is the point.
    //
    // ⚠️ The de-fork did NOT need the `SetSphericalInertia` a previous note here called for.
    // The DWARF declares seven getters and six setters; mSpherical deliberately has no setter
    // because it is written by Inertia::Inertia() (inertia.h:80) and read-only thereafter, and
    // a repo-wide grep confirms nothing else ever writes it. Adding one would have invented an
    // SDK entry point to work around a diagnosis that was wrong.
    //
    // ⚠️ THE TWO DEBUG SCANS ARE KEPT. The console runs an O(n^2) duplicate scan over the
    // queue itself (.cpp:1034) and, per event, a 200-slot scan of the live table (.cpp:1058),
    // both formatting the offending id into the message. A drain that silently accepts a
    // duplicate rigid-body id is exactly the failure this project keeps re-learning.
    // =====================================================================================
    void PhysicsSimulationModule::ProcessAddRigidBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        // `bl sub_8289E408` -- the const GetAddRigidBodyQueue overload (read-lock guarded).
        const PhysicsSimulationIO::InputBuffer::InAddRigidBodyQueue* const lpQueue =
            lpInput->GetAddRigidBodyQueue();

        // ---- debug scan 1: no two requests in this queue may carry the same id ----------
        // 0x828A2738..0x828A28A0. Both loops re-read the length every pass, as the asm does.
        for (s32 liA = 0; liA < lpQueue->GetLength(); ++liA)
        {
            const u64 luIdA = lpQueue->GetEvent(liA).mID;
            for (s32 liB = 0; liB < lpQueue->GetLength(); ++liB)
            {
                if (liA != liB)
                {
                    CGS_ASSERT(luIdA != lpQueue->GetEvent(liB).mID,
                               "Trying to add the same rigid body twice in one frame: ");   // .cpp:1034
                }
            }
        }

        // ---- the drain proper ------------------------------------------------------------
        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InAddRigidBody& lrEvent = lpQueue->GetEvent(li);

            // The console copies the request out of the queue into its own stack frame first
            // (six `lvx128`/`stvx128` for the transform + the two velocities, then a 6-pass
            // ld/std for the inertia) and works from the copy. Kept: AddBody stores the
            // inertia by value, so aliasing the queue element would be a different program if
            // the queue were ever mutated mid-drain.
            const rw::math::vpu::Matrix44Affine lTransform = lrEvent.mRigidBody.mTransform;
            const rw::physics::Inertia          lInertia   = lrEvent.mRigidBody.mInertia;

            // ---- debug scan 2: the id must not already be live in the simulation ----------
            // 0x828A28C0..0x828A2AA4: a full 200-slot walk of maGameIDs (offsets 800..2400).
            for (s32 liSlot = 0; liSlot < RigidBodyData::KI_SIZE; ++liSlot)
            {
                if (mBodyData.IsSlotUsed(liSlot))
                {
                    CGS_ASSERT(liSlot < RigidBodyData::KI_SIZE, "liIndex < knSize");                      // h:585
                    CGS_ASSERT(!mBodyData.GetGameID(liSlot).IsInvalid(), "!maGameIDs[liIndex].IsInvalid()"); // h:586
                    // `.mId` was a reach into the retired local copy's public member; the real
                    // class keeps it private and exposes it through operator u64 (DWARF
                    // CgsRigidBody.h:117), which is what the console's bare `ld` is.
                    CGS_ASSERT(lrEvent.mID != static_cast<u64>(mBodyData.GetGameID(liSlot)),
                               "Trying to add a rigid body with ID that already exists in the simulation: "); // .cpp:1058
                }
            }

            // ⚠️ THE BODY POINTER PASSED HERE IS NULL, DELIBERATELY. `li r4, 0` at 0x828A2AB0:
            // the slot is claimed BEFORE the rw::physics body exists, and the real pointer is
            // written into maRWBodies[] at the bottom of this loop. Passing lpBody here
            // instead would look tidier and would reorder the two writes.
            const s32 liBodyIndex = mBodyData.AddBody(
                nullptr,
                RigidBodyId{ lrEvent.mID },
                lInertia);
            CGS_ASSERT(liBodyIndex != -1, "liBodyIndex != -1");   // .cpp:1065

            // ⚠️ THE POINTER HANDED OVER IS THE TABLE'S OWN SLOT, NOT `lInertia`. AddBody
            // COPIED the stack block into maInertias[liBodyIndex]; the body must point at
            // the copy that outlives this frame, which is exactly what the console does
            // (`bl GetInertia` between the two calls, not a re-use of the stack image).
            rw::physics::RigidBody* const lpBody = mpSimulation->AddRigidBody(
                lTransform,
                mBodyData.GetInertia(liBodyIndex),
                lrEvent.meState);

            // ---- push the request's own state into the fresh body -------------------------
            // 0x828A2B04..0x828A2B94, in the console's order.
            //
            // ⚠️ THE FORCE/TORQUE RESET HERE REPEATS WHAT AddRigidBody ALREADY DID. Both sites
            // inline the DWARF's RigidBody::ResetForces(const Vector3&) (rigidbody.h:273) and
            // both read the gravity through the BODY's own mStasis (`lwz r7,0x4C(r3)` then
            // `lvx128 v13,r7,0x90`), not through this module. The repeat is in the binary.
            lpBody->ResetForces(lpBody->GetSimulation()->GetGravity());

            // `lwz 0x8C` ; `ori 8` ; `stw 0x8C` -- an unconditional SetSpy(true) that the
            // conditional SetSpy(mbSpy) four instructions below immediately overwrites. It is
            // redundant in the original source too; kept because it is what executes.
            lpBody->SetSpy(true);

            lpBody->SetLinearVelocity(lrEvent.mRigidBody.mVelocity);          // mVel   (+0x20) .xyz
            lpBody->SetAngularVelocity(lrEvent.mRigidBody.mAngularVelocity);  // mOmega (+0x30) .xyz

            // `lbz mbSpy` ; `beq` -> `clrlwi r11,r11,29` else `ori r11,r11,8` == SetSpy(bool).
            lpBody->SetSpy(lrEvent.mRigidBody.mbSpy);

            // `stw r31, 0x6C(r3)` -- mTag carries the module's slot index, which is how the
            // output side maps a body back to its RigidBodyData entry.
            lpBody->SetTag(static_cast<u32>(liBodyIndex));

            // `addi r11,r31,0x8C` ; `slwi r11,r11,2` ; `stwx r3,r11,r29` == this + 560 +
            // liBodyIndex*4 == mBodyData.maRWBodies[liBodyIndex] at the CONSOLE's 4-byte
            // pointer stride. ⛔ Never reproduce that arithmetic -- the slot is 8 bytes here.
            mBodyData.SetRigidBody(liBodyIndex, lpBody);

            ++miNumRigidBodies;   // `lwz/addi/stw 0x4848(r29)`
        }
    }

    // =====================================================================================
    // THE DRIVE GROUP -- the five drive drains of the nineteen (task #143, 2026-08-04).
    //
    // Landed together on purpose. `ProcessInputBuffers` is a [[silent-drop-stubs]] no-op
    // unless ALL NINETEEN drains exist, so a "representative sample" would be worse than
    // nothing; a drive subsystem that drains AddDrive but not RemoveDrive would leak slots
    // out of a table with exactly ONE of them. 6 of 19 are bodied after this wave.
    //
    // ⚠️ NOTHING CALLS ANY OF THEM YET, and nothing will until the other thirteen land.
    //
    // ⭐ THE SHARED SKELETON, and the one thing to preserve if these are ever edited: every
    // drain RE-READS q->GetLength() on each pass (`lwz r11, 8(rQ)` at the bottom of each
    // loop, not a cached count) and every one guards its payload on `GetIndexFromGameID(...)
    // != -1`, so an event naming a drive that is not in the table is skipped SILENTLY. That
    // silence is the console's, not a stub's -- there is no assert on that path in any of
    // the five.
    // =====================================================================================

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessAddDriveQueue @ 0x828A4CB8   (136 instructions)
    //
    // The largest of the five, and the only one that touches three subsystems at once:
    // RigidBodyData (to resolve the two body handles), DriveData (to claim a slot),
    // PairSet (to suppress collision between the linked pair) and rw::physics::Simulation
    // (to create the solver drive).
    //
    // ⚠️ THE ORDER OF THE LAST FOUR STEPS IS LOAD-BEARING and is the console's:
    //   1. DriveData::AddDrive claims the slot and COPIES frames+dynamics into the table;
    //   2. PairSet::LinkParts;
    //   3. Simulation::AddDrive is handed the TABLE'S OWN copies (GetDriveFrames /
    //      GetScaledDriveDynamics), never the stack temporaries or the queue element --
    //      the Drive holds those pointers for the lifetime of the drive, so pointing them
    //      at this frame would be a dangling read on the very next tick;
    //   4. only then is the returned Drive* written back into maRWDrives[].
    // Reordering 1 and 3 would look tidier and would hand the solver a dead pointer.
    //
    // ⚠️ `GetScaledDriveDynamics` (+0x60) is what the solver gets, NOT `GetDriveDynamics`
    // (+0x40) -- `bl 0x8289D268` at 0x828A4E80. See DriveData::AddDrive for why the two
    // arrays currently hold the same bytes.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessAddDriveQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InAddDriveQueue* const lpQueue =
            lpInput->GetAddDriveQueue();                                    // `bl sub_8289EBE8`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InAddDrive& lrEvent = lpQueue->GetEvent(li);

            // `ld 8(event)` and `ld 0x10(event)`, in that order.
            const s32 liParentIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mu64ParentBodyId });
            const s32 liChildIndex  = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mu64ChildBodyId });

            // 0x828A4D44/0x828A4D4C -- either handle unresolved and the event is dropped
            // without a word. Both tests are separate branches to the same target.
            if (liParentIndex == -1 || liChildIndex == -1)
                continue;

            rw::physics::RigidBody* const lpParentBody = mBodyData.GetRigidBody(liParentIndex);
            rw::physics::RigidBody* const lpChildBody  = mBodyData.GetRigidBody(liChildIndex);

            CGS_ASSERT(lpParentBody != nullptr, "lpParentBody");   // .cpp:1858 tripwire
            CGS_ASSERT(lpChildBody  != nullptr, "lpChildBody");    // .cpp:1859 tripwire

            // The console copies both payloads out of the queue element into its own frame
            // first (four lvx128 from event+0x20, four ld/std from event+0x60) and passes
            // the copies. Kept: AddDrive takes them by const&, and the copy is what the asm
            // builds at var_E0/var_100.
            const rw::physics::DriveFrames   lFrames   = lrEvent.mDriveFrames;
            const rw::physics::DriveDynamics lDynamics = lrEvent.mDriveDynamics;

            // ⚠️ NULL Drive*, deliberately (`li r4, 0` at 0x828A4DE4) -- the slot is claimed
            // before the solver drive exists, exactly as ProcessAddRigidBodyQueue does.
            // The timestep argument is read from the simulation and then ignored by the
            // callee; see DriveData::AddDrive.
            const s32 liDriveIndex = mDriveData.AddDrive(
                nullptr,
                lFrames,
                lDynamics,
                DriveId{ lrEvent.mu64Id },
                mpSimulation->GetTimeStep());                               // `lfs f1, 0xA0(r9)`
            CGS_ASSERT(liDriveIndex != -1, "liDriveIndex != -1");           // .cpp:1868 tripwire

            // `bl LinkParts` with r4=parent index, r5=child index, r6=0. The two INDICES,
            // not the bodies' tags -- the remove side uses the tags instead, which is an
            // asymmetry in the original, not a transcription slip.
            mpDrivenPairs->LinkParts(liParentIndex, liChildIndex, 0);

            rw::physics::Drive* const lpDrive = mpSimulation->AddDrive(
                lpParentBody,
                lpChildBody,
                mDriveData.GetDriveFrames(liDriveIndex),
                mDriveData.GetScaledDriveDynamics(liDriveIndex));

            lpDrive->SetTag(static_cast<u32>(liDriveIndex));   // `stw r30, 0x18(r3)`
            lpDrive->SetSpy(lrEvent.mbSpy);                    // `stw r10, 0x1C(r3)` -- whole word

            // `addi r11,r30,0x11EC` ; `slwi r11,r11,2` ; `stwx r3,r11,r29` == this + 0x47B0 +
            // liDriveIndex*4 == mDriveData.maRWDrives[liDriveIndex] at the CONSOLE's 4-byte
            // pointer stride. ⛔ Never reproduce that arithmetic -- the slot is 8 bytes here.
            mDriveData.SetDrive(liDriveIndex, lpDrive);

            ++miNumDrives;   // `lwz/addi/stw 0x484C(r29)`
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessRemoveDriveQueue @ 0x8289FF98   (90 instructions)
    //
    // The exact inverse of the above, and it reads the slot BEFORE it frees it.
    //
    // ⚠️ THE UNLINK USES THE BODIES' TAGS, NOT THE TABLE INDICES. The console reaches the two
    // bodies THROUGH the drive (`lwz 0x10` child, `lwz 0x14` parent) and passes
    // `body->mTag` (+0x6C) for each. It cannot do what the add side does, because by this
    // point it only holds a drive id -- there is no body handle in an InRemoveDrive. The
    // (parent, child) argument ORDER is preserved from the add side.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessRemoveDriveQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InRemoveDriveQueue* const lpQueue =
            lpInput->GetRemoveDriveQueue();                                 // `bl sub_8289EC90`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InRemoveDrive& lrEvent = lpQueue->GetEvent(li);

            const s32 liDriveIndex = mDriveData.GetIndexFromGameID(DriveId{ lrEvent.mu64Id });
            if (liDriveIndex == -1)
                continue;

            CGS_ASSERT(liDriveIndex < DriveData::KI_SIZE, "liIndex < knSize");        // h:657
            CGS_ASSERT(mDriveData.IsSlotUsed(liDriveIndex), "mabUsedSlot[liIndex]");  // h:658

            rw::physics::Drive* const lpDrive = mDriveData.GetDrive(liDriveIndex);

            mpDrivenPairs->UnlinkParts(static_cast<int>(lpDrive->GetParent()->GetTag()),
                                       static_cast<int>(lpDrive->GetChild()->GetTag()));

            mDriveData.RemoveDrive(liDriveIndex);   // clears mabUsedSlot only -- see its body
            mpSimulation->RemoveDrive(lpDrive);

            --miNumDrives;   // `lwz/addi -1/stw 0x484C(r28)`
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessUpdateDriveFramesQueue @ 0x8289FC28   (77 instructions)
    //
    // ⭐ THE STRUCTURAL WITNESS FOR THE WHOLE EVENT BLOCK. Its copy loop -- `_R26=16,
    // _R27=32, _R28=48`, base `event + 0x10`, FOUR lvx128/stvx128 pairs, destination stepped
    // by `liIndex << 6` -- is what proved sizeof(DriveFrames)==64 and the payload's +0x10
    // offset independently of the queue-offset chain (task #142). The Joint sibling does
    // FIVE lanes for its extra quaternion. Written here as the whole-object assignment the
    // four lanes are.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessUpdateDriveFramesQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InUpdateDriveFramesQueue* const lpQueue =
            lpInput->GetUpdateDriveFramesQueue();                           // `bl sub_8289ED38`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InUpdateDriveFrames& lrEvent = lpQueue->GetEvent(li);

            const s32 liDriveIndex = mDriveData.GetIndexFromGameID(DriveId{ lrEvent.mu64Id });
            if (liDriveIndex == -1)
                continue;

            *mDriveData.GetDriveFrames(liDriveIndex) = lrEvent.mDriveFrames;
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessUpdateDriveDynamicsQueue @ 0x8289FD60   (74 instructions)
    //
    // ⚠️⚠️ IT WRITES THE UNSCALED ARRAY ONLY. The destination is `(liIndex+2)<<5 + &mDriveData`
    // == maDynamics (+0x40) -- NOT maScaledDynamics (+0x60), which is the array the solver
    // actually reads through Drive::m_crtl. So a mid-life dynamics update does not reach the
    // solver until something re-scales. Nothing in this drain does, and no
    // ScaleOneDriveForTimeStep body exists in the shipped image's call graph from here.
    // Recorded as observed console behaviour; ⛔ NOT "fixed" by also writing the scaled copy,
    // which would be inventing a store and changing the handling.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessUpdateDriveDynamicsQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InUpdateDriveDynamicsQueue* const lpQueue =
            lpInput->GetUpdateDriveDynamicsQueue();                         // `bl sub_8289EDE0`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InUpdateDriveDynamics& lrEvent = lpQueue->GetEvent(li);

            const s32 liDriveIndex = mDriveData.GetIndexFromGameID(DriveId{ lrEvent.mu64Id });
            if (liDriveIndex == -1)
                continue;

            *mDriveData.GetDriveDynamics(liDriveIndex) = lrEvent.mDriveDynamics;
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessSetDriveSpyQueue @ 0x8289FE88   (68 instructions)
    //
    // The nineteenth and last call in ProcessInputBuffers' dispatch. ⚠️ This function is an
    // .ida-exports HOLE -- it is absent from the JSON export set and was recovered by
    // decoding the .i64 directly (task #142 for the address, #143 for the body). Another
    // instance of [[ida-export-set-has-holes]]: missing-from-JSON is not nonexistent.
    //
    // ⚠️ `stw`, not a bitfield edit. The console stores the event's whole byte into
    // Drive::m_spy (+0x1C) as a word. The `ori 8` / `clrlwi ...,29` pair that
    // RigidBody::SetSpy compiles to belongs to a DIFFERENT class whose flag shares a word
    // with its state; importing that shape here would corrupt three unrelated bits.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessSetDriveSpyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InSetDriveSpyQueue* const lpQueue =
            lpInput->GetSetDriveSpyQueue();                                 // `bl sub_8289EE88`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InSetDriveSpy& lrEvent = lpQueue->GetEvent(li);

            const s32 liDriveIndex = mDriveData.GetIndexFromGameID(DriveId{ lrEvent.mu64Id });
            if (liDriveIndex == -1)
                continue;

            CGS_ASSERT(liDriveIndex < DriveData::KI_SIZE, "liIndex < knSize");        // h:657
            CGS_ASSERT(mDriveData.IsSlotUsed(liDriveIndex), "mabUsedSlot[liIndex]");  // h:658

            mDriveData.GetDrive(liDriveIndex)->SetSpy(lrEvent.mbSpy);
        }
    }
}
