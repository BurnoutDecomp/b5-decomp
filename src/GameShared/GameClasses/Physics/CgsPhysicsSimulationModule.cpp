#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // AddMonitor

// The real rw::physics world Prepare/AllocateMemoryAndInitialiseRW build. Only the .cpp
// needs the definitions -- the header keeps them forward-declared, exactly as the console's
// does (its own Prepare is the only member that dereferences mpSimulation).
// The drain side needs the COMPLETE InputBuffer (the header only forward-declares it):
// ProcessAddRigidBodyQueue reads its embedded add-rigid-body queue and the events in it.
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"

// The Update-side closure/spy wave (2026-08-06): the tick's IslandGenerator scratch type, the
// per-frame IOBufferStack it is carved from, and the debug-print stream the contact-spy
// diagnostic dump streams to.
#include "GameShared/GameClasses/Physics/CgsIslandGenerator.h"     // CgsPhysics::IslandGenerator (type only)
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"        // CgsModule::IOBufferStack (Create/DestroyIOBuffer<T>)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator, Resource, descriptors
#include "rw/physics/simulation.h"              // rw::physics::Simulation + SpyingFlag
#include "rw/physics/SimulationWorkspace.h"     // rw::physics::SimulationWorkspace
#include "rw/physics/pairset.h"                 // rw::physics::PairSet
#include "rw/physics/contact.h"                 // rw::physics::Contact -- drain 19 (2026-08-05)
#include "vendor/renderware/physics/Drive.hpp"   // rw::physics::Drive -- the drive drains (task #143)
#include "vendor/renderware/physics/Joint.hpp"   // rw::physics::Joint -- the joint drains (task #144)
// The three IsValid overloads ProcessAddJointQueue's eleven inlined tripwires need, one per
// width: 4-lane Quaternion, 3-lane Vector3, scalar f32.
#include "rw/math/vpu/vector4_operation.h"       // rw::math::vpu::IsValid(const Quaternion&)
#include "rw/math/vpu/vector3_operation.h"       // rw::math::vpu::IsValid(const Vector3&)
#include "rw/math/fpu/scalar_operation.h"        // rw::math::fpu::IsValid(float)

#include <cstddef>   // offsetof (layout gate)
#include <stdlib.h>  // getenv (the opt-in [extbody] miss witness, host only)
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
    // two leading 16-byte vectors and mVtwist..mTwista plus both enum slots,
    // and set mSwingc/mTwistc to 1.0f. (f0 == 0.0f, f13 == 1.0f.)
    //
    // ⭐ 2026-08-04 (task #144): THIS BODY IS NOW EMPTY, AND THAT IS THE FAITHFUL SPELLING.
    // It used to be an explicit 36-pass loop writing those sixteen values by hand against the
    // FORKED CgsPhysics::JointLimits. With the fork retired, `maLimits` is an array of the real
    // rw::physics::JointLimits, whose own default constructor carries exactly that store
    // sequence -- so the implicit member initialisation of maLimits[36] IS the console's pass,
    // and re-writing the same values afterwards would be a second pass the console does not
    // make. See rw::physics::JointLimits::JointLimits() for why those stores provably belong to
    // it: mSwingc/mTwistc have no DWARF setter and only a const-ref getter, so no caller
    // outside the class can write the console's 1.0f.
    //
    // ⚠️ maFrames[36] IS ALSO DEFAULT-CONSTRUCTED NOW, and that is likewise correct: the DWARF
    // declares JointFrames::JointFrames() (jointframes.h:76) and its body is EMPTY, so nothing
    // is written -- matching the console, whose ctor loop touches maLimits and nothing else.
    // ⚠️ It is not free, though: JointFrames' user-provided (DWARF-attested) ctor/dtor make
    // JointData non-trivial, so the compiler emits the array loops. DriveFrames is trivial,
    // which is why the drive wave was byte-neutral and this one is not. Flagged, not "fixed" --
    // deleting an attested constructor to save code size would be trading truth for bytes.
    JointData::JointData()
    {

        // ---- layout pins (hosted here so offsetof can see the private members) ----
        // X360-exact and host-invariant up to the first pointer array: maLimits at
        // 0xB40 == 36*80, maRWJoints at 0x1440 == 0xB40 + 36*64. Past that the
        // pointer slot widens on x64, so the tail is pinned relative to it.
        static_assert(offsetof(JointData, maFrames)   == 0,    "maFrames @+0");
        static_assert(offsetof(JointData, maLimits)   == 2880, "maLimits @+0xB40 (X360-exact: 36*80)");
        static_assert(offsetof(JointData, maRWJoints) == 5184, "maRWJoints @+0x1440 (X360-exact: 0xB40 + 36*64)");

        // ---- task #144: the two element STRIDES, spelled as the MEMBER, not as a literal ----
        // The two lines above are 2880 and 5184 because the elements are 80 and 64 bytes wide.
        // Spelled that way they would still pass if JointFrames were re-typed to something 80
        // bytes long by accident, and they say nothing about WHICH record sits in the array.
        // These restate the same two offsets in terms of `sizeof(JointData::maFrames[0])`, so a
        // re-typing of either element -- the exact failure the JointFrames/JointLimits forks
        // were -- breaks the gate. Both strides are the drains' own arithmetic:
        // ProcessUpdateJointFramesQueue steps maFrames by i*80 (five lvx128 lanes) and
        // ProcessUpdateJointLimitsQueue steps maLimits by (i+45)*64 (eight ld/std pairs).
        static_assert(sizeof(JointData::maFrames[0]) == 80,
                      "JointData element stride 80 (drain @0x8289F4E8 `slwi/add/slwi` == i*5*16)");
        static_assert(sizeof(JointData::maLimits[0]) == 64,
                      "JointData element stride 64 (drain @0x8289F724 `addi r10,r31,0x2D; slwi r10,r10,6`)");
        static_assert(offsetof(JointData, maLimits)   == KI_SIZE * sizeof(JointData::maFrames[0]),
                      "maLimits follows the 36 JointFrames");
        static_assert(offsetof(JointData, maRWJoints) == offsetof(JointData, maLimits) + KI_SIZE * sizeof(JointData::maLimits[0]),
                      "maRWJoints follows the 36 JointLimits");
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

    // DWARF h:165. ⭐ ADDED 2026-08-05 (the rigid-body drain group). No out-of-line symbol --
    // the console inlines it into ProcessRemoveRigidBodyQueue's "Removing jointed body"
    // diagnostic at 0x828A30B0..0x828A3108: asserts h:612 (`cmpwi r24,0x24` == liIndex <
    // knSize) and h:613 (`lbz 0x15F0(this+liIndex)` == mabUsedSlot), then the 8-byte
    // `ldx` off maGameIDs (+0x14D0). Same checked-accessor shape as its three siblings; the
    // four assert-line pairs run 612/613, 621/622, 630/631, 639/640 -- nine header lines
    // apart each, which is the arithmetic that identifies every one of them.
    JointId JointData::GetGameID(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:612 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:613 tripwire (bne skips)
        return maGameIDs[liIndex];
    }

    // Raw slot-occupancy probe. Inlined by the console into
    // PhysicsSimulationModule::AllocateMemoryAndInitialiseRW (`lbzx` off &mabUsedSlot[0] at
    // 0x828A2560, then `beq` past the removal). No tripwire -- this IS the tripwire.
    bool JointData::IsSlotUsed(s32 liIndex) const
    {
        return mabUsedSlot[liIndex];
    }

    // DWARF h:152. No out-of-line symbol -- the console inlines it into all FIVE joint drains,
    // and all five copies are the same linear search, which is the control on this body:
    // `lbzx` mabUsedSlot[k] -> skip if clear; `ld` maGameIDs[k] and a 64-bit `cmpld` against the
    // event's id -> return k on a hit; `cmpwi r31,0x24` bounds the walk at 36; exhaustion falls
    // through to -1. ⭐ Two committed offsets fall out of every copy: `addi r11,r21,0x14D0` for
    // maGameIDs and `addi r9,r21,0x15F0` for mabUsedSlot. The 8-byte `ld`/`cmpld` pair is also
    // what pins JointId at 8 bytes.
    //
    // ⚠️ UNLIKE THE DRIVE TWIN, THE MISS IS NOT SILENT AT THE CALL SITES. DriveData's four
    // callers just guard on `!= -1`; every joint drain instead fires an assert on -1 before
    // skipping the event. The difference lives in the callers, not here -- this body has no
    // diagnostic of its own, exactly as DriveData::GetIndexFromGameID has none.
    s32 JointData::GetIndexFromGameID(JointId lGameID) const
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            if (mabUsedSlot[li] && maGameIDs[li].muId == lGameID.muId)
                return li;
        }
        return -1;
    }

    // DWARF h:144. X360 @0x8289D3E0 (98 instructions), sole caller ProcessAddJointQueue
    // @0x828A4358. Claims the first free slot and fills it.
    //
    // ⭐⭐ THIS IS **NOT** DriveData::AddDrive WITH THE NAMES CHANGED, and mirroring that body
    // would have lost a whole phase with no diagnostic. The console opens with a full 36-slot
    // DUPLICATE-DETECTION pre-pass (0x8289D428..0x8289D48C) that walks every LIVE slot and
    // asserts the incoming id and joint pointer are not already registered. AddDrive (62
    // instructions) has no such pass; that pre-pass is most of the 98-vs-62 gap.
    //
    // ⭐ EVERY ONE OF THE FIVE ARRAY BASES IS AN INDEPENDENT WITNESS TO THIS CLASS'S LAYOUT,
    // because the console reaches each through its own shift, and all five reproduce the
    // committed offsets exactly:
    //     (i + i*4)<<4 -> +0x0000  maFrames      (i+0x2D)<<6  -> +0x0B40  maLimits
    //     (i+0x510)<<2 -> +0x1440  maRWJoints    (i+0x29A)<<3 -> +0x14D0  maGameIDs
    //     stb 1, 0x15F0(this+i) -> +0x15F0  mabUsedSlot
    //
    // ⚠️ FOUR PARAMETERS, NO TIMESTEP. DriveData::AddDrive takes a fifth `float32_t` it accepts
    // and never reads (the defect flagged in task #143); the joint side never had it, and the
    // DWARF signature (h:144) confirms four.
    s32 JointData::AddJoint(rw_physics::Joint* lpJoint, const JointFrames& lrFrames,
                            const JointLimits& lrLimits, JointId lGameID)
    {
        // The duplicate-detection pre-pass. Asserts only -- it does not return early, and it
        // does not skip the add. Transcribed as shipped.
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            if (mabUsedSlot[li])
            {
                CGS_ASSERT(maGameIDs[li].muId != lGameID.muId, "maGameIDs[liIndex] != lGameID");   // .cpp:2661
                CGS_ASSERT(maRWJoints[li] != lpJoint, "maRWJoints[liIndex] != lpJoint");           // .cpp:2662
            }
        }

        s32 liIndex = 0;
        while (liIndex < KI_SIZE && mabUsedSlot[liIndex])
            ++liIndex;

        if (liIndex >= KI_SIZE)
            return -1;

        maGameIDs[liIndex]   = lGameID;    // stdx r24 -> (i+0x29A)<<3
        maRWJoints[liIndex]  = lpJoint;    // stwx r25 -> (i+0x510)<<2   (NULL at the call site)
        maFrames[liIndex]    = lrFrames;   // 5x lvx128/stvx128 -> i*80
        maLimits[liIndex]    = lrLimits;   // 8x ld/std -> (i+0x2D)<<6
        mabUsedSlot[liIndex] = true;       // stb 1, 0x15F0(this+i)  -- LAST, as the asm does
        return liIndex;
    }

    // DWARF h:148. No out-of-line symbol -- inlined into ProcessRemoveJointQueue at
    // 0x8289FBC4..0x8289FBEC: the `liIndex < knSize` tripwire (.cpp:2696) and then the single
    // `stb r20(=0), 0x4700(r11)` with r11 = this + liIndex, i.e. mabUsedSlot[liIndex] = false
    // (0x4700 == mJointData 0x3110 + mabUsedSlot 0x15F0).
    //
    // ⚠️ IT CLEARS THE USED FLAG AND NOTHING ELSE, exactly as DriveData::RemoveDrive does.
    // maRWJoints[liIndex] is deliberately left pointing at the joint the caller is about to
    // hand to Simulation::RemoveJoint -- the caller reads the slot BEFORE calling this, and the
    // flag is what makes the slot re-allocatable. Zeroing the pointer would invent a store.
    bool JointData::RemoveJoint(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");   // .cpp:2696 tripwire (blt skips)
        mabUsedSlot[liIndex] = false;
        return true;
    }

    // Checked slot accessors. No out-of-line symbols -- inlined at every call site -- but their
    // asserts identify them unambiguously: these two carry h:630/631 and h:639/640 respectively,
    // against GetJoint's h:621/622 (which the out-of-line GetJoint @0x8289D168 confirms).
    JointFrames* JointData::GetJointFrames(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:630 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:631 tripwire (bne skips)
        return &maFrames[liIndex];
    }

    JointLimits* JointData::GetJointLimits(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:639 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:640 tripwire (bne skips)
        return &maLimits[liIndex];
    }

    // [INFERRED NAME] -- the write-back half of GetJoint. See the header for why the write is
    // attested (ProcessAddJointQueue @0x828A497C) even though the DWARF carries no such method.
    void JointData::SetJoint(s32 liIndex, rw_physics::Joint* lpRWJoint)
    {
        maRWJoints[liIndex] = lpRWJoint;
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

    // DWARF h:237. ⭐ ADDED 2026-08-06 (the spy wave). No out-of-line symbol -- the console
    // inlines it into AddDriveSpiesToOutputQueue @0x828A5D84..0x828A5E08 as the checked
    // maGameIDs[liIndex] read behind asserts h:648 (`cmpwi r30,1` == liIndex < knSize) /
    // h:649 (`lbz 0x90(this+liIndex)` == mabUsedSlot), closing with the 8-byte
    // `ldx (liIndex+0x11)<<3` off maGameIDs. The JointData::GetGameID shape exactly.
    DriveId DriveData::GetGameID(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:648 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:649 tripwire (bne skips)
        return maGameIDs[liIndex];
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
    // The DWARF declares seven getters and six setters; mSpherical has no setter of its own,
    // and adding one would have invented an SDK entry point to work around a diagnosis that
    // was wrong. ⚠️ AMENDED 2026-08-05: the reason given here ("written by Inertia::Inertia()
    // and read-only thereafter; a repo-wide grep confirms nothing else ever writes it")
    // half-expired when ProcessChangeRigidBodyInertiaQueue was decoded -- the console DOES
    // rewrite mSpherical on every diagonal change, inside SetInverseInertia, where the
    // derived-value maintenance now lives (see inertia.h). Still no independent setter.
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

    // =====================================================================================
    // THE JOINT GROUP -- five of the nineteen input drains  (task #144, 2026-08-04)
    //
    // Same skeleton as the drive five: fetch the queue, walk mCount events, resolve the
    // event's 64-bit game id to a slot through JointData::GetIndexFromGameID (which the
    // console inlines into every one of them), act on the slot.
    //
    // ⚠️⚠️ ONE STRUCTURAL DIFFERENCE FROM THE DRIVE GROUP, AND IT IS NOT COSMETIC: a joint
    // id that resolves to -1 is an ERROR here. The drive drains skip such an event silently
    // -- "that silence is the console's, not a stub's", as the block above records. Every
    // joint drain instead fires an assert first and then skips. Both behaviours are
    // transcribed as shipped; do not harmonise them.
    //
    // ⚠️ WHAT IS DELIBERATELY NOT RECONSTRUCTED, AND WHY. The console builds those miss
    // messages by STREAMING: `CgsDev::StrStreamBase::AppendFormat("0x%08X")` on the high word
    // of the id followed by `AppendFormat("%08X")` on the low word, into
    // `CgsDev::Assert::gpcMessageBuffer`, and ProcessRemoveJointQueue additionally guards a
    // debug-TTY print on a global flag (`ld 0x1908(r11)` & 1 at 0x8289FA74) before firing.
    // This tree has only `CGS_ASSERT(cond, "literal")`, and CgsAssert.h already records why:
    // "The original streamed the message into the assert buffer via StrStream; the call sites
    // all pass a plain string". The console's own message TEXT and the exact control flow
    // (assert, then skip the event) are kept; the formatting machinery belongs to whoever
    // reconstructs CgsDev::StrStreamBase. ⭐ Incidentally the two-halves format is itself a
    // third witness that JointId is 64 bits wide.
    //
    // ⭐ THE FIVE QUEUE ACCESSORS ARE A 5-FOR-5 CONTROL on the queue table landed in task #142:
    // every drain's opening `bl` resolves to exactly the address CgsPhysicsSimulationModuleIO.h
    // records -- 0x8289E8A0 / E948 / E9F0 / EA98 / EB40.
    // =====================================================================================

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessAddJointQueue @ 0x828A40F0   (557 instructions)
    //
    // ⭐ THE LARGEST DRAIN IN THE WHOLE NINETEEN, and 557 instructions is misleading: roughly
    // 310 of them are the ELEVEN inlined `RwMathVPU::IsValid` tripwires, each a per-lane
    // `vspltw` + `vcmpeqfp.` self-comparison. The actual work is the same four steps as
    // ProcessAddDriveQueue.
    //
    // ⭐⭐ THOSE ELEVEN ASSERTS ARE WHY THE JointFrames/JointLimits TYPE FORKS ARE RETIRED.
    // Their baked literals name the accessors outright -- "RwMathVPU::IsValid(
    // lpFrames->GetChildAngularFrame() )", "... lpLimits->GetAngularVelocityLimit() )" and nine
    // more -- and every one of those accessors exists on rw::physics::JointFrames /
    // rw::physics::JointLimits and on NEITHER of the opaque CgsPhysics copies that used to
    // shadow them. The shipped binary states the parameter types. See the retirement block in
    // CgsPhysicsSimulationModule.h.
    //
    // ⚠️ THE ORDER OF THE LAST FOUR STEPS IS LOAD-BEARING and is the console's, identical to
    // the drive twin:
    //   1. JointData::AddJoint claims the slot and COPIES frames+limits into the table;
    //   2. PairSet::LinkParts on mpJointedPairs (+0x47D0 -- NOT mpDrivenPairs);
    //   3. Simulation::AddJoint is handed the TABLE'S OWN copies (GetJointFrames /
    //      GetJointLimits), never the stack temporaries or the queue element -- the Joint
    //      holds those pointers for the lifetime of the joint, so pointing them at this
    //      frame would be a dangling read on the very next tick;
    //   4. only then is the returned Joint* written back into maRWJoints[].
    //
    // ⚠️ `GetChildPosition()` is the ONE JointFrames slot the console does NOT validate -- four
    // of the five are checked. Transcribed as shipped rather than "completed".
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessAddJointQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InAddJointQueue* const lpQueue =
            lpInput->GetAddJointQueue();                                    // `bl sub_8289E8A0`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InAddJoint& lrEvent = lpQueue->GetEvent(li);

            const s32 liParentBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mu64ParentBodyId });
            const s32 liChildBodyIndex  = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mu64ChildBodyId });
            CGS_ASSERT(liParentBodyIndex != -1, "liParentBodyIndex != -1");   // .cpp:1506
            CGS_ASSERT(liChildBodyIndex  != -1, "liChildBodyIndex != -1");    // .cpp:1507

            rw_physics::RigidBody* const lpParentBody = mBodyData.GetRigidBody(liParentBodyIndex);
            rw_physics::RigidBody* const lpChildBody  = mBodyData.GetRigidBody(liChildBodyIndex);
            CGS_ASSERT(lpParentBody != nullptr, "lpParentBody");             // .cpp:1516
            CGS_ASSERT(lpChildBody  != nullptr, "lpChildBody");              // .cpp:1517

            // The console copies both payloads out of the queue element onto the stack first
            // (8x ld/std from event+0x70, then 5x lvx128 from event+0x20) and passes those
            // temporaries by const reference. AddJoint copies them again into the table.
            const JointFrames lFrames = lrEvent.mJointFrames;
            const JointLimits lLimits = lrEvent.mJointLimits;

            const s32 liJointIndex = mJointData.AddJoint(nullptr, lFrames, lLimits,
                                                         JointId{ lrEvent.mu64Id });
            CGS_ASSERT(liJointIndex != -1, "liJointIndex != -1");            // .cpp:1521

            // ⚠️ mpJointedPairs (+0x47D0), not mpDrivenPairs (+0x47D4). The console reads
            // `lwz r3, 0x47D0(r23)` here and the same slot again in ProcessRemoveJointQueue.
            mpJointedPairs->LinkParts(liParentBodyIndex, liChildBodyIndex, 0);

            JointFrames* const lpFrames = mJointData.GetJointFrames(liJointIndex);  // h:630/631
            JointLimits* const lpLimits = mJointData.GetJointLimits(liJointIndex);  // h:639/640

            // The eleven validation tripwires, in the console's own order. The inlined lane
            // counts type each one: four `vspltw` for a Quaternion, three for a Vector3, one
            // for an f32.
            CGS_ASSERT(rw::math::vpu::IsValid(lpFrames->GetChildAngularFrame()),   "RwMathVPU::IsValid( lpFrames->GetChildAngularFrame() )");   // .cpp:1529
            CGS_ASSERT(rw::math::vpu::IsValid(lpFrames->GetParentAngularFrame()),  "RwMathVPU::IsValid( lpFrames->GetParentAngularFrame() )");  // .cpp:1530
            CGS_ASSERT(rw::math::vpu::IsValid(lpFrames->GetParentLinearFrame()),   "RwMathVPU::IsValid( lpFrames->GetParentLinearFrame() )");   // .cpp:1531
            CGS_ASSERT(rw::math::vpu::IsValid(lpFrames->GetParentPosition()),      "RwMathVPU::IsValid( lpFrames->GetParentPosition() )");      // .cpp:1532
            CGS_ASSERT(rw::math::vpu::IsValid(lpLimits->GetAngularVelocityLimit()), "RwMathVPU::IsValid( lpLimits->GetAngularVelocityLimit() )"); // .cpp:1535
            CGS_ASSERT(rw::math::vpu::IsValid(lpLimits->GetLinearVelocityLimit()),  "RwMathVPU::IsValid( lpLimits->GetLinearVelocityLimit() )");  // .cpp:1536
            CGS_ASSERT(rw::math::vpu::IsValid(lpLimits->GetPositionLimit()),        "RwMathVPU::IsValid( lpLimits->GetPositionLimit() )");        // .cpp:1537
            CGS_ASSERT(rw::math::fpu::IsValid(lpLimits->GetTwistLimit()),           "RwMathVPU::IsValid( lpLimits->GetTwistLimit() )");           // .cpp:1538
            CGS_ASSERT(rw::math::fpu::IsValid(lpLimits->GetTwistAngle()),           "RwMathVPU::IsValid( lpLimits->GetTwistAngle() )");           // .cpp:1539
            CGS_ASSERT(rw::math::fpu::IsValid(lpLimits->GetSwingLimit()),           "RwMathVPU::IsValid( lpLimits->GetSwingLimit() )");           // .cpp:1540
            CGS_ASSERT(rw::math::fpu::IsValid(lpLimits->GetSwingAngle()),           "RwMathVPU::IsValid( lpLimits->GetSwingAngle() )");           // .cpp:1541

            rw_physics::Joint* const lpJoint =
                mpSimulation->AddJoint(lpParentBody, lpChildBody, lpFrames, lpLimits);

            lpJoint->SetTag(static_cast<u32>(liJointIndex));   // +0x18  `stw r28,0x18(r3)`
            lpJoint->SetSpy(lrEvent.mbSpy);                    // +0x1C  `lbz 0xB0(event)` -> `stw`
            mJointData.SetJoint(liJointIndex, lpJoint);        // maRWJoints[idx] -- `stwx r3,(idx+0x1154)<<2`

            ++miNumJoints;   // `lwz/addi 1/stw 0x4850(r23)`
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessRemoveJointQueue @ 0x8289F970   (174 instructions)
    //
    // The exact inverse of the above, and it reads the slot BEFORE it frees it.
    //
    // ⚠️ THE UNLINK USES THE BODIES' TAGS, NOT THE TABLE INDICES, for the same reason the
    // drive twin does: the console reaches the two bodies THROUGH the joint (`lwz 0x10` child,
    // `lwz 0x14` parent) and passes `body->mTag` (+0x6C) for each. It cannot do what the add
    // side does, because by this point it only holds a joint id -- there is no body handle in
    // an InRemoveJoint (which is a bare 8-byte id and nothing else). The (parent, child)
    // argument ORDER is preserved from the add side.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessRemoveJointQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InRemoveJointQueue* const lpQueue =
            lpInput->GetRemoveJointQueue();                                 // `bl sub_8289E948`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InRemoveJoint& lrEvent = lpQueue->GetEvent(li);

            const s32 liJointIndex = mJointData.GetIndexFromGameID(JointId{ lrEvent.mu64Id });
            if (liJointIndex == -1)
            {
                // .cpp:1792. Console text preserved; see the group banner for the streamed
                // id and the flag-guarded TTY print that are not reconstructed.
                CGS_ASSERT(liJointIndex != -1, "Physics: Bad search for a joint ID ");
                continue;
            }

            CGS_ASSERT(liJointIndex < JointData::KI_SIZE, "liIndex < knSize");        // h:621
            CGS_ASSERT(mJointData.IsSlotUsed(liJointIndex), "mabUsedSlot[liIndex]");  // h:622

            rw_physics::Joint* const lpJoint = mJointData.GetJoint(liJointIndex);

            mpJointedPairs->UnlinkParts(static_cast<int>(lpJoint->GetParent()->GetTag()),
                                        static_cast<int>(lpJoint->GetChild()->GetTag()));

            mJointData.RemoveJoint(liJointIndex);   // clears mabUsedSlot only -- see its body
            mpSimulation->RemoveJoint(lpJoint);

            --miNumJoints;   // `lwz/addi -1/stw 0x4850(r15)`
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessUpdateJointFramesQueue @ 0x8289F2F0   (149 instructions)
    //
    // ⭐ THE STRUCTURAL WITNESS FOR THE JOINT PAYLOAD, exactly as its drive sibling is for the
    // drive one. Its copy loop -- base `event + 0x10`, destination stepped by
    // `slwi r10,r31,2 / add r10,r31,r10 / slwi r10,r10,4` (== i*5*16 == i*80), and FIVE
    // lvx128/stvx128 pairs at 0/0x10/0x20/0x30/0x40 -- is what proves sizeof(JointFrames)==80
    // independently of the DWARF, and it is the one lane MORE than the drive twin copies.
    // Written here as the whole-object assignment the five lanes are.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessUpdateJointFramesQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InUpdateJointFramesQueue* const lpQueue =
            lpInput->GetUpdateJointFramesQueue();                           // `bl sub_8289E9F0`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InUpdateJointFrames& lrEvent = lpQueue->GetEvent(li);

            const s32 liJointIndex = mJointData.GetIndexFromGameID(JointId{ lrEvent.mu64Id });
            if (liJointIndex == -1)
            {
                CGS_ASSERT(liJointIndex != -1, "Joint not found: ");   // .cpp:1577
                continue;
            }

            *mJointData.GetJointFrames(liJointIndex) = lrEvent.mJointFrames;
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessUpdateJointLimitsQueue @ 0x8289F548   (136 instructions)
    //
    // The limits sibling. Its copy is EIGHT `ld/std` pairs (64 bytes) rather than lvx128 lanes
    // -- an integer whole-object copy, the same idiom ProcessUpdateDriveDynamicsQueue uses --
    // and its destination steps by `addi r10,r31,0x2D / slwi r10,r10,6` == (i+45)*64, i.e.
    // maLimits at +0x0B40 with a 64-byte stride.
    //
    // ⚠️ UNLIKE THE DRIVE DYNAMICS TWIN THERE IS NO SECOND, SCALED ARRAY here to disagree with:
    // JointData has one limits array and this writes it. The "writes the unscaled array only"
    // defect flagged on the drive side has no analogue.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessUpdateJointLimitsQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InUpdateJointLimitsQueue* const lpQueue =
            lpInput->GetUpdateJointLimitsQueue();                           // `bl sub_8289EA98`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InUpdateJointLimits& lrEvent = lpQueue->GetEvent(li);

            const s32 liJointIndex = mJointData.GetIndexFromGameID(JointId{ lrEvent.mu64Id });
            if (liJointIndex == -1)
            {
                CGS_ASSERT(liJointIndex != -1, "Joint not found: ");   // .cpp:1611
                continue;
            }

            *mJointData.GetJointLimits(liJointIndex) = lrEvent.mJointLimits;
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessSetJointSpyQueue @ 0x8289F768   (129 instructions)
    //
    // ⚠️ The drain stores the event byte with a PLAIN `stw` into Joint::m_spy (+0x1C) -- a
    // whole word, not a bit. This is NOT the `ori 8` / `clrlwi ...,29` bitfield fork that the
    // rw::physics `SetSpy(bool)` sibling compiles to; do not import that shape here. Identical
    // to the drive twin.
    //
    // ⭐ Its two accessor asserts are h:621/622, i.e. GetJoint's -- which the out-of-line
    // GetJoint @0x8289D168 confirms directly. That is how this drain is known to go through
    // GetJoint rather than reaching into maRWJoints[].
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessSetJointSpyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InSetJointSpyQueue* const lpQueue =
            lpInput->GetSetJointSpyQueue();                                 // `bl sub_8289EB40`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InSetJointSpy& lrEvent = lpQueue->GetEvent(li);

            const s32 liJointIndex = mJointData.GetIndexFromGameID(JointId{ lrEvent.mu64Id });
            if (liJointIndex == -1)
            {
                CGS_ASSERT(liJointIndex != -1, "Joint not found: ");   // .cpp:1645
                continue;
            }

            CGS_ASSERT(liJointIndex < JointData::KI_SIZE, "liIndex < knSize");        // h:621
            CGS_ASSERT(mJointData.IsSlotUsed(liJointIndex), "mabUsedSlot[liIndex]");  // h:622

            mJointData.GetJoint(liJointIndex)->SetSpy(lrEvent.mbSpy);
        }
    }

    // =====================================================================================
    // THE RIGID-BODY GROUP -- seven more of the nineteen input drains (2026-08-05).
    //
    // Landed together as the complete rigid-body side, same rule as the drive (#143) and
    // joint (#144) groups: a partly-drained subsystem is the [[silent-drop-stubs]] shape.
    // (The "18 of 19 / only ProcessAddContactQueue remains" note that stood here is retired:
    // drain 19 and ProcessInputBuffers landed later the same day, at the end of this file.)
    //
    // ⚠️ NOTHING CALLS ANY OF THEM YET -- ProcessInputBuffers exists now but its own two
    // callers (the Update / ProcessInput virtuals) do not.
    //
    // ⭐ THE MISS POLICY IS PER-DRAIN, READ OFF EACH BODY'S OWN ASM, NOT INHERITED: the five
    // update-side drains skip a GetIndexFromGameID miss SILENTLY (the drive shape, not the
    // joint shape -- there is no "not found" assert in any of them); the remove drain asserts
    // on a miss ONLY when the event asks it to (mbFailIfRigidBodyNotFound); the remove-all
    // drain has no id at all. Do not harmonise them.
    //
    // ⭐ FOUR OF THE FIVE UPDATE DRAINS SHARE THE SAME TAIL, decoded identically in each:
    // reload mState AFTER the payload work, `if ((state & STATIC_BODY) == 0) SetCoolDown(0)`,
    // `if (state & FROZEN_BODY) mpSimulation->ActivateRigidBody(body)` -- i.e. touching a
    // body wakes it. ChangeInertia alone has NO such tail (it writes the shared Inertia
    // block, not the body).
    // =====================================================================================

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessRemoveRigidBodyQueue @ 0x828A2BD0   (546 instructions)
    //
    // The heaviest drain of the nineteen, and ~80% of it is diagnostics: an O(n^2)
    // duplicate-remove scan, a 36-slot jointed-body scan, and a cross-QUEUE scan against the
    // pending external-body updates. All three are kept -- a remove that silently races an
    // update is exactly the failure the third scan exists to catch.
    //
    // ⚠️ THE NOT-FOUND PATH IS TWO-TIERED, off `InRemoveRigidBody::mbFailIfRigidBodyNotFound`
    // (`lbz 8(event)` at 0x828A2E38): a miss with the flag CLEAR skips the event silently; a
    // miss with the flag SET fires ".cpp:1172" -- and then FALLS THROUGH into the checked
    // accessor with liBodyIndex == -1, exactly as the console does (fire-and-continue
    // asserts; the accessor's own h:594/h:595 tripwires then fire on the bad index). Not
    // "fixed": the divergence would be invisible exactly when the diagnostics matter.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessRemoveRigidBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InRemoveRigidBodyQueue* const lpQueue =
            lpInput->GetRemoveRigidBodyQueue();                             // `bl sub_8289E6A8`

        // ---- debug scan 1: no two requests may remove the same id this frame -------------
        // 0x828A2C44..0x828A2D2C; both loops re-read the length every pass, as the asm does.
        for (s32 liA = 0; liA < lpQueue->GetLength(); ++liA)
        {
            const u64 luIdA = lpQueue->GetEvent(liA).mID;
            for (s32 liB = 0; liB < lpQueue->GetLength(); ++liB)
            {
                if (liA != liB)
                {
                    CGS_ASSERT(luIdA != lpQueue->GetEvent(liB).mID,
                               "Trying to remove rigid bodies with the same ID on the same frame: ");   // .cpp:1145
                }
            }
        }

        // ---- the drain proper ------------------------------------------------------------
        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InRemoveRigidBody& lrEvent = lpQueue->GetEvent(li);

            const s32 liBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mID });
            if (liBodyIndex == -1 && !lrEvent.mbFailIfRigidBodyNotFound)
            {
                continue;   // the tolerant remove -- silent, 0x828A2E54
            }
            if (liBodyIndex == -1)
            {
                CGS_ASSERT(liBodyIndex != -1, "Couldn't find rigid body with id: ");   // .cpp:1172
                // fire-and-continue: the console falls through with -1 -- see the banner.
            }

            // Inlined checked accessor at 0x828A2EEC..0x828A2F68 (h:594/h:595 fire from this
            // frame on the console; here they live in the accessor's own body).
            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liBodyIndex);

            // ---- debug scan 2: is any live joint still attached to this body? ------------
            // 0x828A2F6C..0x828A3268. ⚠️ GetJoint is asserts-free HERE on the console: under
            // the IsSlotUsed guard, inside a loop bounded at KI_SIZE, both of its h:621/h:622
            // conditions are provably true and the compiler folded them. The THREE id lookups
            // in the diagnostic branch keep their tripwires (they sit past opaque assert-
            // machinery calls), and they are what pins JointData::GetGameID's h:612/h:613.
            for (s32 liJoint = 0; liJoint < JointData::KI_SIZE; ++liJoint)
            {
                if (!mJointData.IsSlotUsed(liJoint))
                {
                    continue;
                }
                rw_physics::Joint* const lpJoint = mJointData.GetJoint(liJoint);
                const bool lbJointed = (lpJoint->GetChild() == lpBody || lpJoint->GetParent() == lpBody);
                if (lbJointed)
                {
                    // The console streams " Removing jointed body <id> Joint ID: <jid>
                    // Body A: <idA> Body B: <idB>" from three checked lookups, in THIS order
                    // (B's id first, then A's, then the joint id -- 0x828A2FBC/0x828A302C/
                    // 0x828A3108). The lookups are kept because their tripwires are real
                    // behaviour; the streamed formatting is not reconstructed (this file's
                    // standing plain-literal convention).
                    const RigidBodyId lIdB = mBodyData.GetGameID(static_cast<s32>(lpJoint->GetParent()->GetTag()));   // h:585/h:586
                    const RigidBodyId lIdA = mBodyData.GetGameID(static_cast<s32>(lpJoint->GetChild()->GetTag()));    // h:585/h:586
                    const JointId     lJId = mJointData.GetGameID(liJoint);                                           // h:612/h:613
                    (void)lIdB; (void)lIdA; (void)lJId;
                    CGS_ASSERT(!lbJointed, " Removing jointed body ");   // .cpp:1188 -- fire-and-continue
                }
            }

            // ---- debug scan 3: is this body about to receive an external update? ---------
            // 0x828A326C..0x828A33AC -- reads the OTHER queue through its own const accessor
            // (`bl sub_8289EF30`), then per-element GetEvent (whose CgsBaseEventQueue.h :272/
            // :274/:275 tripwires the console emits inline in this frame).
            const PhysicsSimulationIO::InputBuffer::InUpdateExternalBodyQueue* const lpExtQueue =
                lpInput->GetUpdateExternalBodyQueue();
            for (s32 liExt = 0; liExt < lpExtQueue->GetLength(); ++liExt)
            {
                CGS_ASSERT(lpExtQueue->GetEvent(liExt).mID != lrEvent.mID,
                           "Attempting to remove a body which is going to be updated ");   // .cpp:1197
            }

            CGS_ASSERT(lpBody != NULL, "lpRwBody");   // .cpp:1202

            mpSimulation->RemoveRigidBody(lpBody);
            mBodyData.SetFree(liBodyIndex);   // inlined on the console: .cpp:2504 + the sentinel store
            --miNumRigidBodies;               // `lwz/addi -1/stw 0x4848` -- 0x828A342C
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessRemoveAllRigidBodiesQueue @ 0x8289F1D8   (69 instructions)
    //
    // Per event: sweep ALL 200 slots and remove every live body whose id's OWNER BYTE matches
    // the event's mu8OwnerId. ⭐ The owner byte is bits 56..63 of the 64-bit handle
    // (`srdi r10,r10,32` + `srwi r10,r10,24` at 0x8289F274) == RigidBodyId::GetEntityIDOwner()
    // -- the first in-scope witness for that DWARF accessor.
    //
    // ⚠️ TWO ASYMMETRIES AGAINST THE SINGLE-BODY REMOVE ABOVE, both real, neither harmonised:
    //   * it does NOT decrement miNumRigidBodies (no 0x4848 store anywhere in the body);
    //   * it runs none of the three diagnostic scans.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessRemoveAllRigidBodiesQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InRemoveAllRigidBodiesQueue* const lpQueue =
            lpInput->GetRemoveAllRigidBodiesQueue();                        // `bl sub_8289E750`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const u8 luOwner = lpQueue->GetEvent(li).mu8OwnerId;

            for (s32 liSlot = 0; liSlot < RigidBodyData::KI_SIZE; ++liSlot)
            {
                if (!mBodyData.IsSlotUsed(liSlot))    // inlined 64-bit sentinel compare, 0x8289F234
                {
                    continue;
                }
                if (mBodyData.GetGameID(liSlot).GetEntityIDOwner() != luOwner)   // called @0x8289CF78
                {
                    continue;
                }
                mpSimulation->RemoveRigidBody(mBodyData.GetRigidBody(liSlot));   // called @0x8289D020 / @0x82BC2950
                mBodyData.SetFree(liSlot);   // inlined: the .cpp:2504 tripwire + the sentinel store
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessUpdateExternalBodyQueue @ 0x828A3B30   (368 instructions)
    //
    // Push an externally-simulated body's authored pose/velocities into the rw body. The
    // payload block is the witnessed inline of THREE RigidBody methods, in the console's
    // order: SetTransform (the four w-preserving rows + QuaternionFromMatrix33 -- bodied in
    // src/vendor/renderware/physics/RigidBody.cpp off THIS drain's asm), the mInertia-guarded
    // InertiaUpdate (the same vpermwi128 0x97/0x9B block AddRigidBody and DynamicUpdate
    // emit), then the two velocity setters.
    //
    // ⚠️ DWARF ACCESSIBILITY: this is the ONE drain the DWARF declares PUBLIC -- see the
    // header. ⚠️ Its miss policy is the drive shape (SILENT skip), not the joint shape.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessUpdateExternalBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InUpdateExternalBodyQueue* const lpQueue =
            lpInput->GetUpdateExternalBodyQueue();                          // `bl sub_8289EF30`

        // ---- debug scan: no two updates may name the same id this frame ------------------
        // 0x828A3B98..0x828A3C88; both loops re-read the length every pass, as the asm does.
        for (s32 liA = 0; liA < lpQueue->GetLength(); ++liA)
        {
            const u64 luIdA = lpQueue->GetEvent(liA).mID;
            for (s32 liB = 0; liB < lpQueue->GetLength(); ++liB)
            {
                if (liA != liB)
                {
                    CGS_ASSERT(luIdA != lpQueue->GetEvent(liB).mID,
                               "Trying to update external bodies with the same ID on the same frame: ");   // .cpp:1450
                }
            }
        }

        // ---- the drain proper ------------------------------------------------------------
        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InUpdateExternalBody& lrEvent = lpQueue->GetEvent(li);

            const s32 liBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mID });
            if (liBodyIndex == -1)
            {
                // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_PROP_DIAG), first-N witness of the
                // console's silent skip: an update addressed to a body the sim never received.
                // Added 2026-09-02 with VehicleManager::GetUpdatedVehicleBodies -- the PROP_COLLISION
                // proxies are the first external bodies this drain has ever been fed, and "the
                // proxy was never created" and "the proxy is fed" must be distinguishable in a log.
                // DELETE-WHEN the high-speed prop reaction is confirmed on screen.
                {
                    static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
                    static s32        siMissLinesLeft = 8;
                    if (sbPropDiag && siMissLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                    {
                        --siMissLinesLeft;
                        *CgsDev::Log::gpDebugPrint
                            << "[extbody] update for entity " << CgsDev::E_PRINTMODE_HEXONCE
                            << static_cast<u32>(lrEvent.mID >> 32)
                            << " index " << static_cast<u32>(lrEvent.mID & 0xFFFFu)
                            << " has NO sim body -- skipped silently, as on the console\n";
                    }
                }
                continue;   // SILENT -- 0x828A3D8C, no assert on this path
            }

            // Inlined checked accessor (h:594/h:595 fire from this frame on the console).
            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liBodyIndex);
            CGS_ASSERT(lpBody != NULL, "lpBody");   // .cpp:1466

            lpBody->SetTransform(lrEvent.mTransform);           // 0x828A3E28..0x828A3FAC
            if (lpBody->GetInertia() != NULL)                   // `lwz 0x5C` guard, 0x828A3FB0
            {
                lpBody->InertiaUpdate(lpBody->GetInertia());    // 0x828A3FB4..0x828A4070
            }
            lpBody->SetLinearVelocity(lrEvent.mVel);            // event+0x50 -> mVel  (+0x20)
            lpBody->SetAngularVelocity(lrEvent.mAngularVel);    // event+0x60 -> mOmega(+0x30)

            lpBody->SetCoolDown(0);                             // unconditional, 0x828A40B4
            if ((lpBody->GetState() & rw_physics::FROZEN_BODY) != 0)
            {
                mpSimulation->ActivateRigidBody(lpBody);        // 0x828A40C8
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessUpdateRigidBodyQueue @ 0x828A3A08   (74 instructions)
    //
    // Overwrite a live body with the event's full RigidBody image via RigidBody::operator=
    // @0x825E3410, PRESERVING the destination's intrusive list links: the console saves
    // +0x2C/+0x3C around the call and restores them after (0x828A3AD4/0x828A3AE8) -- the
    // save/restore that proved operator= copies the w-lane payloads (see RigidBody.cpp's
    // corrected banner).
    //
    // ⚠️ THE STATE MAY NOT CHANGE THROUGH THIS PATH -- .cpp:1358 asserts the incoming image
    // carries the SAME mState the live body has, which is what makes the post-copy
    // wake/activate tail below meaningful.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessUpdateRigidBodyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InUpdateRigidBodyQueue* const lpQueue =
            lpInput->GetUpdateRigidBodyQueue();                             // `bl sub_8289E4B0`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InUpdateRigidBody& lrEvent = lpQueue->GetEvent(li);

            const s32 liBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mID });
            if (liBodyIndex == -1)
            {
                continue;   // SILENT -- 0x828A3A78
            }

            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liBodyIndex);   // called @0x8289D020
            CGS_ASSERT(lpBody != NULL, "lpBody");   // .cpp:1356
            CGS_ASSERT(lpBody->GetState() == lrEvent.mRigidBody.GetState(),
                       "Can't change the state in an update RigidBody");   // .cpp:1358 (`lwz 0x8C` vs `lwz 0x9C(event)`)

            rw_physics::RigidBody* const lpSavedRight = lpBody->GetRight();   // `lwz 0x2C`
            rw_physics::RigidBody* const lpSavedLeft  = lpBody->GetLeft();    // `lwz 0x3C`
            *lpBody = lrEvent.mRigidBody;                                     // `bl 0x825E3410`
            lpBody->SetRight(lpSavedRight);                                   // `stw 0x2C`
            lpBody->SetLeft(lpSavedLeft);                                     // `stw 0x3C`

            const s32 liState = lpBody->GetState();     // re-read AFTER the copy (`lwz 0x8C`, 0x828A3AE4)
            if ((liState & rw_physics::STATIC_BODY) == 0)
            {
                lpBody->SetCoolDown(0);                 // 0x828A3AFC
            }
            if ((liState & rw_physics::FROZEN_BODY) != 0)
            {
                mpSimulation->ActivateRigidBody(lpBody);   // 0x828A3B14
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessApplyForceQueue @ 0x828A6B80   (82 instructions)
    //
    // The payload is the witnessed inline of RigidBody::AddForce (rigidbody.h): the force is
    // PRE-SCALED by the body's inverse mass before accumulating -- `lfs 0x7C` (mInvm), splat,
    // `vmulfp128`, three scalar `fadds` into +0x90/+0x94/+0x98. mForce is an acceleration-
    // dimensioned accumulator here (it is seeded with gravity, -600 y, by ResetForces).
    //
    // ⚠️ THE COOLDOWN CLEAR IS EMITTED TWICE, 0x828A6C84 unconditional and 0x828A6C94 under
    // `(state & STATIC_BODY) == 0` -- redundant in the original source too, kept because it
    // is what executes (same rule as the double SetSpy in ProcessAddRigidBodyQueue).
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessApplyForceQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InApplyForceQueue* const lpQueue =
            lpInput->GetApplyForceQueue();                                  // `bl sub_8289E558`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InApplyForce& lrEvent = lpQueue->GetEvent(li);

            const s32 liBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mID });
            if (liBodyIndex == -1)
            {
                continue;   // SILENT -- 0x828A6BEC
            }

            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liBodyIndex);   // called @0x8289D020
            CGS_ASSERT(lpBody != NULL, "lpBody");   // .cpp:1405

            lpBody->AddForce(lrEvent.mForce, rw_physics::WORLD_SPACE);   // 0x828A6C1C..0x828A6C7C

            const s32 liState = lpBody->GetState();     // one read (`lwz 0x8C`), used twice
            lpBody->SetCoolDown(0);                     // unconditional first store
            if ((liState & rw_physics::STATIC_BODY) == 0)
            {
                lpBody->SetCoolDown(0);                 // the shipped second store
            }
            if ((liState & rw_physics::FROZEN_BODY) != 0)
            {
                mpSimulation->ActivateRigidBody(lpBody);   // 0x828A6CAC
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessSetRigidBodySpyQueue @ 0x828A49A8   (51 instructions)
    //
    // ⚠️ UNLIKE the joint/drive spy twins (plain whole-word `stw` into m_spy), this one IS
    // the `ori r11,r11,8` / `clrlwi r11,r11,29` bitfield fork on mState -- i.e. exactly the
    // committed RigidBody::SetSpy(bool). The three spy drains genuinely differ; do not
    // harmonise them in either direction.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessSetRigidBodySpyQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InSetRigidBodySpyQueue* const lpQueue =
            lpInput->GetSetRigidBodySpyQueue();                             // `bl sub_8289E600`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InSetRigidBodySpy& lrEvent = lpQueue->GetEvent(li);

            const s32 liBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mID });
            if (liBodyIndex == -1)
            {
                continue;   // SILENT -- 0x828A4A0C
            }

            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liBodyIndex);   // called @0x8289D020
            CGS_ASSERT(lpBody != NULL, "lpBody");   // .cpp:1683

            lpBody->SetSpy(lrEvent.mSpy);           // `lbz 8(event)`; ori 8 / clrlwi 29
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessChangeRigidBodyInertiaQueue @ 0x828A4A78   (143 instructions)
    //
    // Selective per-field update of the SHARED maInertias[] block the live body points at --
    // it never touches the body itself (no world-inertia rebuild, no cooldown clear, no
    // activate; the body picks the new values up through its mInertia pointer on its next
    // DynamicUpdate). `mu32Flags` selects fields, one bit per settable Inertia property:
    //     bit0 AngularDrag   bit1 InverseInertia (SetInverseInertia also re-derives
    //     mSpherical -- see inertia.h)   bit2 InverseMass   bit3 LinearDrag
    //     bit4 MaxAngularVelocity        bit5 MaxLinearVelocity
    // flags == 0x3F short-circuits into a whole-object copy (six ld/std pairs, 0x828A4BA8).
    //
    // ⭐ ITS QUEUE ACCESSOR IS THE RETRACTION: `bl 0x8259EE80` at 0x828A4A90 is the const
    // GetChangeRigidBodyInertiaQueue this tree's IO header used to claim did not exist in
    // the image. See the header's retraction block.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessChangeRigidBodyInertiaQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InChangeRigidBodyInertiaQueue* const lpQueue =
            lpInput->GetChangeRigidBodyInertiaQueue();                      // `bl 0x8259EE80`

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)
        {
            const PhysicsSimulationIO::InChangeRigidBodyInertia& lrEvent = lpQueue->GetEvent(li);

            const s32 liBodyIndex = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mID });
            if (liBodyIndex == -1)
            {
                continue;   // SILENT -- 0x828A4B08
            }

            Inertia* const lpInertia = mBodyData.GetInertia(liBodyIndex);   // called @0x8289D0C0
            CGS_ASSERT(lpInertia != NULL, "lpInertia");                     // .cpp:1716
            CGS_ASSERT(lrEvent.mu32Flags != 0, "You didn't send any flags!\n");   // .cpp:1718 -- fire-and-continue

            if (lrEvent.mu32Flags == 0x3F)
            {
                *lpInertia = lrEvent.mInertia;   // the whole-object fast path: 6x ld/std, 0x828A4BA8
                continue;                        // `b loc_828A4C98`
            }

            // The per-bit path re-reads mu32Flags before every test, as the asm does
            // (`lwz 0x40(r28)` six times). Console test order preserved: 1, 2, 4, 8, 0x10, 0x20.
            if ((lrEvent.mu32Flags & 0x01) != 0)
            {
                lpInertia->SetAngularDrag(lrEvent.mInertia.GetAngularDrag());           // e+0x34 -> +0x24
            }
            if ((lrEvent.mu32Flags & 0x02) != 0)
            {
                lpInertia->SetInverseInertia(lrEvent.mInertia.GetInverseInertia());     // e+0x10 -> +0x00 (+ mSpherical, see inertia.h)
            }
            if ((lrEvent.mu32Flags & 0x04) != 0)
            {
                lpInertia->SetInverseMass(lrEvent.mInertia.GetInverseMass());           // e+0x20 -> +0x10
            }
            if ((lrEvent.mu32Flags & 0x08) != 0)
            {
                lpInertia->SetLinearDrag(lrEvent.mInertia.GetLinearDrag());             // e+0x30 -> +0x20
            }
            if ((lrEvent.mu32Flags & 0x10) != 0)
            {
                lpInertia->SetMaxAngularVelocity(lrEvent.mInertia.GetMaxAngularVelocity()); // e+0x2C -> +0x1C
            }
            if ((lrEvent.mu32Flags & 0x20) != 0)
            {
                lpInertia->SetMaxLinearVelocity(lrEvent.mInertia.GetMaxLinearVelocity());   // e+0x28 -> +0x18
            }
        }
    }

    // =====================================================================================
    // DRAIN 19 -- THE CONTACT DRAIN (2026-08-05). All nineteen input drains now exist.
    // =====================================================================================

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessAddContactQueue @ 0x828A3458   (363 instructions)
    //
    // Per event: resolve both body ids, allocate a contact record off the simulation's
    // per-frame budget, fill it (the inlined Contact::GenerateFromCollision -- the ~200-insn
    // VMX block, transcribed in vendor's contact.h), and register the (idxA, idxB) pair in
    // the contact PairSet, de-duplicated ONLY against the immediately previous event's pair.
    //
    // ⭐ THE BLOCKER THAT KEPT THIS DRAIN UNWRITTEN FOR A WAVE IS CLOSED, from the consumer:
    // Contact::mBodyA/mBodyB, which on the console receive the event's mPointOnA/mPointOnB
    // w lanes via the full-row stvx, are DEAD CARGO -- ContactBatchBuild @0x82BC14C0
    // overwrites both from the snapshot mCom.w (== RigidBody::mId) before any read, and no
    // producer mints them (see contact.h's banner for all three witnesses).
    //
    // ⚠️ MISS POLICY: either id unresolved -> skip SILENTLY (0x828A3580/0x828A3588) -- the
    // drive shape, no assert. The three asserts it does have are all fire-and-continue:
    //   * NEITHER body ACTIVE (streamed "Rigid Body A: <idA> Rigid Body B: <idB>") .cpp:1302
    //     -- the streamed values are the raw event ids (`ld 0x30/0x38`), no checked lookups,
    //     so the plain-literal convention drops nothing;
    //   * lpBodyB == NULL                                                          .cpp:1307
    //   * lpContact == NULL (the frame's contact budget m_CT_Max is spent)         .cpp:1312
    //     -- and the console then generates into the NULL record regardless (fire-and-
    //     continue straight into the fill at 0x828A3708), exactly as transcribed: not
    //     "fixed", same rule as ProcessRemoveRigidBodyQueue's -1 fall-through.
    //
    // ⚠️ Contact::mTag gets THE LOOP INDEX, not the event's muTag -- the event field is never
    // read by this drain (verified: no load of 0x4C(event) in the body; the producers do
    // write it). The double store the asm shows at +0x5C (restore-then-overwrite) is the
    // dead prelude artifact documented on GenerateFromCollision.
    //
    // ⚠️ ACTIVE-BODY GATE ASYMMETRY, real, not harmonised: the assert fires only when
    // NEITHER body is ACTIVE, but the tangent frame's r-vector is chosen by BODY A's ACTIVE
    // bit alone (the vsel mask at 0x828A37B4 splats bodyA's state word only).
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessAddContactQueue(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        const PhysicsSimulationIO::InputBuffer::InAddContactQueue* const lpQueue =
            lpInput->GetAddContactQueue();                                  // `bl sub_8289E7F8`

        // The one-deep pair-dedup state (var_110/var_10C, init -1 -- 0x828A3488/0x828A3490).
        s32 liPrevIndexA = -1;
        s32 liPrevIndexB = -1;

        for (s32 li = 0; li < lpQueue->GetLength(); ++li)   // length RE-READ every pass (0x828A39EC)
        {
            const PhysicsSimulationIO::InAddPotentialContact& lrEvent = lpQueue->GetEvent(li);

            const s32 liIndexA = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mIDA });   // `ld 0x30(event)`
            const s32 liIndexB = mBodyData.GetIndexFromGameID(RigidBodyId{ lrEvent.mIDB });   // `ld 0x38(event)`
            if (liIndexA == -1 || liIndexB == -1)
            {
                continue;   // SILENT -- 0x828A3580 / 0x828A3588, no assert on this path
            }

            // Inlined checked accessors (h:594/h:595 fire from this frame on the console).
            rw_physics::RigidBody* const lpBodyA = mBodyData.GetRigidBody(liIndexA);
            rw_physics::RigidBody* const lpBodyB = mBodyData.GetRigidBody(liIndexB);

            // Both bodies asleep -> the streamed diagnostic (its two values are the raw event
            // ids, already in lrEvent -- this file's standing plain-literal convention).
            const bool lbEitherActive =
                (lpBodyA->GetState() & rw_physics::ACTIVE_BODY) != 0 ||
                (lpBodyB->GetState() & rw_physics::ACTIVE_BODY) != 0;
            if (!lbEitherActive)
            {
                CGS_ASSERT(lbEitherActive, "Rigid Body A: ");   // .cpp:1302 -- fire-and-continue
            }
            CGS_ASSERT(lpBodyB != NULL, "lpBodyB");             // .cpp:1307 -- fire-and-continue

            // Bump-allocate the record (the inlined Simulation::GetFreeContact, 0x828A36B0).
            rw_physics::Contact* const lpContact = mpSimulation->GetFreeContact();
            CGS_ASSERT(lpContact != NULL, "lpContact");         // .cpp:1312 -- fire-and-continue,
                                                                // straight into the fill, as shipped

            // The ~200-instruction VMX fill, 0x828A3708..0x828A39B4 -- transcribed as the
            // 9-argument GenerateFromCollision the console inlined (DWARF contact.h:207).
            // ⚠️ the tag argument is the LOOP INDEX (`lwz r28, var_104` at 0x828A3938).
            lpContact->GenerateFromCollision(lpBodyA, lpBodyB,
                                             lrEvent.mPointOnA, lrEvent.mPointOnB, lrEvent.mNormal,
                                             lrEvent.mStaticFriction, lrEvent.mDynamicFriction,
                                             lrEvent.mRestitution, static_cast<u32>(li));

            // Register the pair -- skipped ONLY when both indices match the previous event's
            // (0x828A39B8..0x828A39C4); the memo updates only when LinkParts runs.
            if (liIndexA != liPrevIndexA || liIndexB != liPrevIndexB)
            {
                mpContactPairs->LinkParts(liIndexA, liIndexB, 0);   // `bl 0x82BC6F18`, liData = 0
                liPrevIndexA = liIndexA;
                liPrevIndexB = liIndexB;
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessInputBuffers @ 0x828A73C0   (68 instructions)
    //
    // The pure dispatcher: NINETEEN drain calls, each `(this, lpInput)`, no branches, no
    // return value. Bodied ONLY NOW because the nineteenth drain exists -- the standing
    // "stays unbodied until all nineteen" rule is satisfied, not bent (a dispatcher over a
    // partial set loses whole queues without a diagnostic, the [[silent-drop-stubs]] shape).
    //
    // ⭐ THE ORDER IS LOAD-BEARING AND IS THE FUNCTION'S OWN: removes before adds; bodies,
    // then joints, then drives. Re-derived from the nineteen `bl` targets twice (task #142,
    // and again from the headless dump on 2026-08-05) and reproduced call-for-call; it is
    // recorded in no other source. Do not re-order to "group" the drains.
    //
    // ⚠️ NO CALLER YET: the console reaches this only from the Update / ProcessInput
    // virtuals (@0x828A74D0 / @0x828A76D0), neither of which is bodied -- declaring either
    // without its body would materialise the vtable into LNK2019 (see the header's slot-16
    // note). /OPT:REF strips this whole chain until they land; nothing new executes.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessInputBuffers(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        ProcessAddContactQueue(lpInput);               // bl 0x828A3458
        ProcessRemoveDriveQueue(lpInput);              // bl 0x8289FF98
        ProcessRemoveJointQueue(lpInput);              // bl 0x8289F970
        ProcessRemoveRigidBodyQueue(lpInput);          // bl 0x828A2BD0
        ProcessRemoveAllRigidBodiesQueue(lpInput);     // bl 0x8289F1D8
        ProcessAddRigidBodyQueue(lpInput);             // bl 0x828A2708
        ProcessUpdateExternalBodyQueue(lpInput);       // bl 0x828A3B30
        ProcessUpdateRigidBodyQueue(lpInput);          // bl 0x828A3A08
        ProcessApplyForceQueue(lpInput);               // bl 0x828A6B80
        ProcessSetRigidBodySpyQueue(lpInput);          // bl 0x828A49A8
        ProcessChangeRigidBodyInertiaQueue(lpInput);   // bl 0x828A4A78
        ProcessAddJointQueue(lpInput);                 // bl 0x828A40F0
        ProcessUpdateJointFramesQueue(lpInput);        // bl 0x8289F2F0
        ProcessUpdateJointLimitsQueue(lpInput);        // bl 0x8289F548
        ProcessSetJointSpyQueue(lpInput);              // bl 0x8289F768
        ProcessAddDriveQueue(lpInput);                 // bl 0x828A4CB8
        ProcessUpdateDriveFramesQueue(lpInput);        // bl 0x8289FC28
        ProcessUpdateDriveDynamicsQueue(lpInput);      // bl 0x8289FD60
        ProcessSetDriveSpyQueue(lpInput);              // bl 0x8289FE88
    }

    // =====================================================================================
    // THE UPDATE-SIDE CLOSURE + SPY EMITTERS + THE TWO VIRTUALS (2026-08-06).
    // The whole game-side callee set of Update @0x828A74D0; with these, the module's DWARF
    // virtual set is complete. ⚠️ NOTHING REACHES ANY OF THIS AT RUNTIME YET -- the only
    // console caller of the two virtuals is BrnPhysics::PhysicsModule::Update @0x825B0640,
    // still the inert WorldLinkStubs boot gate; /OPT:REF strips the cluster until it lands.
    // =====================================================================================

    // The world-bounds clamp constants (DWARF .cpp:2350-2353 declares the group
    // KVF_MAX_DIST_ALONG_AXIS / KV_MAX_POSITION / KV_MIN_POSITION /
    // TEMP_KF_MAX_DISTANCE_FROM_ORIGIN_SQUARED; only the two KV_ vectors are consumed by the
    // functions homed here, so only they are defined -- the other two have no in-scope
    // reader and would be dead weight).
    //
    // ⭐ VALUES RECOVERED FROM THE IMAGE, NOT GUESSED. On the X360 these live in BSS
    // (0x8307A7D0 / 0x8307A7F0 -- zero in the image) and are DYNAMICALLY initialised by an
    // unnamed static-initializer block the export set does not carry (found by a headless
    // dref hunt over the .i64): @0x82C6F490 splats flt_820080E8 into KVF_MAX_DIST_ALONG_AXIS,
    // @0x82C6F4B8 permutes that into KV_MAX_POSITION, @0x82C6F4F0 sign-flips it into
    // KV_MIN_POSITION. flt_820080E8 == 0x47C35000 == 100000.0f (read via x360rd, validated
    // reader). Cross-witness: the DecFIGS PS3 build's own initializer @0xC309C0 builds the
    // same pair by splat + sign-xor from one float. POD aggregates here, so the PC gets
    // static (compile-time) init where the console needed the runtime block -- same values,
    // no init-order hazard.
    static const rw::math::vpu::Vector3 KV_MAX_POSITION = {  100000.0f,  100000.0f,  100000.0f, 0.0f };
    static const rw::math::vpu::Vector3 KV_MIN_POSITION = { -100000.0f, -100000.0f, -100000.0f, 0.0f };

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::AddContactSpiesToOutputQueue @ 0x828A4ED8   (641 instructions)
    //
    // Join each ContactJacobianSpy record the solver emitted with the ORIGINAL drain event
    // (the spy's muTag is the drain's loop index into the input add-contact queue -- see
    // ProcessAddContactQueue's tag note above) and push an OutContactSpy. The output stresses
    // are the accumulated impulses over the step: the vendor spy divided by ts^2, this
    // multiplies by ts, net impulse/ts == force.
    //
    // ⚠️ 500+ of the 641 instructions are ONE diagnostic: when a spy's tag does not index a
    // live queue event, the console dumps the whole queue and every spy record to
    // gpDebugPrint and fires .cpp:2112 ("...please tell Andy or Graham..."), then FALLS
    // THROUGH into the emit with the bad tag -- GetEvent's own tripwires fire and the read
    // is out of range, as shipped (the ProcessRemoveRigidBodyQueue -1 fall-through rule).
    // The dump is reconstructed (it is behaviour, gated on gxMessageFilterFlags bit 0, and
    // this build routes gpDebugPrint to the game log); the E_PRINTMODE_HEXONCE pushes are
    // the console's own `mePrintMode = 2` stores before each 64-bit id.
    // ⚠️ The queue add is AddEventSafe -- the ONE bounded add in this module family: a full
    // spy queue drops the event and logs, it does not overrun (the console's `!result` +
    // "Warning: ..." path).
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::AddContactSpiesToOutputQueue(const PhysicsSimulationIO::InputBuffer* lpInput,
                                                               PhysicsSimulationIO::OutputBuffer* lpOutput)
    {
        const PhysicsSimulationIO::InputBuffer::InAddContactQueue* const lpQueue =
            lpInput->GetAddContactQueue();                              // `bl sub_8289E7F8`

        const s32 liNumSpies = static_cast<s32>(mpSimulation->GetContactSpyCount());   // `lwz 0x68`
        for (s32 liSpy = 0; liSpy < liNumSpies; ++liSpy)
        {
            const rw_physics::ContactJacobianSpy* const lpSpy = mpSimulation->GetContactSpy(liSpy);

            const s32 liTag = static_cast<s32>(lpSpy->muTag);
            if (liTag >= lpQueue->GetLength())
            {
                // ---- the "Invalid contact pair ID" diagnostic dump (0x828A4FD0..0x828A58A8) --
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Invalid contact pair ID:\n\nContact spy index = " << liSpy << "\n";
                }
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "\nPotential contacts:\n";
                }
                for (s32 li = 0; li < lpQueue->GetLength(); ++li)
                {
                    const PhysicsSimulationIO::InAddPotentialContact& lrDump = lpQueue->GetEvent(li);
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        // (The console streams an empty rodata string @0x820046A7 as the line
                        // prefix -- appending nothing; not reproduced.)
                        *CgsDev::Log::gpDebugPrint
                            << li << ". IdA = " << CgsDev::E_PRINTMODE_HEXONCE << lrDump.mIDA
                            << ", IdB = "       << CgsDev::E_PRINTMODE_HEXONCE << lrDump.mIDB
                            << ", Tag = "       << lrDump.muTag << "\n";
                    }
                }
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "\nContact spies:\n";
                }
                for (s32 li = 0; li < liNumSpies; ++li)
                {
                    const rw_physics::ContactJacobianSpy* const lpDumpSpy = mpSimulation->GetContactSpy(li);
                    // Resolve each side's game id ONLY when its tag lands on a live slot; a
                    // dead side prints the invalid sentinel (the console seeds both from
                    // qword_82F33E18 == K_INVALID_RIGID_BODY_ID and overwrites under the
                    // same guards).
                    u64 luIdA = K_INVALID_RIGID_BODY_ID;
                    u64 luIdB = K_INVALID_RIGID_BODY_ID;
                    const s32 liTagA = static_cast<s32>(lpDumpSpy->mpBodyA->GetTag());
                    if (liTagA < RigidBodyData::KI_SIZE && mBodyData.IsSlotUsed(liTagA))
                    {
                        luIdA = mBodyData.GetGameID(liTagA);            // h:585/h:586
                    }
                    const s32 liTagB = static_cast<s32>(lpDumpSpy->mpBodyB->GetTag());
                    if (liTagB < RigidBodyData::KI_SIZE && mBodyData.IsSlotUsed(liTagB))
                    {
                        luIdB = mBodyData.GetGameID(liTagB);            // h:585/h:586
                    }
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << li << ". IdA = " << CgsDev::E_PRINTMODE_HEXONCE << luIdA
                            << ", IdB = "       << CgsDev::E_PRINTMODE_HEXONCE << luIdB
                            << ", Tag = "       << lpDumpSpy->muTag << "\n";
                    }
                }
                // Streamed on the console; plain-literal per this file's standing convention.
                CGS_ASSERT(liTag < lpQueue->GetLength(),
                           "\nInvalid contact pair ID - please tell Andy or Graham and include the debug output above!");   // .cpp:2112
                // fire-and-continue INTO the emit with the bad tag, as shipped.
            }

            const PhysicsSimulationIO::InAddPotentialContact& lrEvent = lpQueue->GetEvent(liTag);   // `bl sub_8259D258`
            const f32 lfTimeStep = mpSimulation->GetTimeStep();                                      // `lfs 0xA0`

            PhysicsSimulationIO::OutContactSpy lEvent = {};   // zero the pad lanes the per-component stores below leave
            lEvent.mFrictionStress.x = lpSpy->mForceT.x * lfTimeStep;   // spy +0x50 row * splat(ts)
            lEvent.mFrictionStress.y = lpSpy->mForceT.y * lfTimeStep;
            lEvent.mFrictionStress.z = lpSpy->mForceT.z * lfTimeStep;
            lEvent.mNormalStress.x   = lpSpy->mForceN.x * lfTimeStep;   // spy +0x40 row * splat(ts)
            lEvent.mNormalStress.y   = lpSpy->mForceN.y * lfTimeStep;
            lEvent.mNormalStress.z   = lpSpy->mForceN.z * lfTimeStep;
            lEvent.mNormal   = lrEvent.mNormal;                         // drain event +0x20
            lEvent.mPointOnA = lrEvent.mPointOnA;                       // drain event +0x00
            lEvent.mPointOnB = lrEvent.mPointOnB;                       // drain event +0x10
            // The two ids resolve through the spy's tail pointers (the relocated event
            // w-lane RigidBody* -- contact.h's 272 tail) and each body's slot tag.
            lEvent.mIDA  = mBodyData.GetGameID(static_cast<s32>(lpSpy->mpBodyA->GetTag()));   // h:585/h:586
            lEvent.mIDB  = mBodyData.GetGameID(static_cast<s32>(lpSpy->mpBodyB->GetTag()));   // h:585/h:586
            lEvent.muTag = lrEvent.muTag;                               // drain event +0x4C

            const bool lbAdded = lpOutput->GetContactSpyQueue()->AddEventSafe(lEvent);
            if (!lbAdded && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                *CgsDev::Log::gpDebugPrint
                    << "Warning: Physics simulation contact spy queue full. Some contacts were not output";
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::AddJointSpiesToOutputQueue @ 0x828A58E0   (267 instructions)
    // ⚠️ An .ida-exports HOLE -- transcribed from the headless .i64 pull (the same route as
    // ProcessInputBuffers / Destruct).
    //
    // Per JointJacobianSpy record: chase the joint's slot tag, look up its game id, scale
    // the spy's force/torque rows by (m_TimeStep * 59.999996f) -- flt_820EA088, read off the
    // image; the odd constant is the shipped rodata, not 60.0f -- and AddEvent an
    // OutJointSpy. The scale splat is hoisted above the loop, as the console hoists it.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::AddJointSpiesToOutputQueue(PhysicsSimulationIO::OutputBuffer* lpOutput)
    {
        PhysicsSimulationIO::OutputBuffer::OutJointSpyQueue* const lpQueue =
            lpOutput->GetJointSpyQueue();                               // `bl sub_8259F1C8`

        // Diagnostic: a spy record per active joint at most. Streamed on the console
        // ("Active joints: <n> Joint Spies: <n>\n"); plain-literal here.
        const bool lbCountsSane =
            mpSimulation->GetActiveJointCount() >= mpSimulation->GetJointSpyCount();   // `lwz 0x50` vs `lwz 0x70`
        if (!lbCountsSane)
        {
            CGS_ASSERT(lbCountsSane, "Active joints: ");                // .cpp:2180 -- fire-and-continue
        }

        const f32 lfScale = mpSimulation->GetTimeStep() * 59.999996f;   // `lfs 0xA0` * flt_820EA088, splat
        const s32 liNumSpies = static_cast<s32>(mpSimulation->GetJointSpyCount());
        for (s32 liSpy = 0; liSpy < liNumSpies; ++liSpy)
        {
            const rw_physics::JointJacobianSpy* const lpSpy = mpSimulation->GetJointSpy(liSpy);

            const s32 liJointIndex = static_cast<s32>(lpSpy->mpJoint->GetTag());   // `lwz 0x18(joint)`
            CGS_ASSERT(liJointIndex != -1, "liJointIndex != -1");       // .cpp:2220 -- fire-and-continue

            // Streamed on the console (" Joint ptr: <p> Joint Index: <i> Child ptr: <c>
            // Parent ptr: <p>\n" -- the two body pointers read but only streamed);
            // plain-literal here, so nothing checked is dropped.
            const bool lbSlotUsed = mJointData.IsSlotUsed(liJointIndex);   // `lbz 0x4700+idx`
            if (!lbSlotUsed)
            {
                CGS_ASSERT(lbSlotUsed, " Joint ptr: ");                 // .cpp:2226 -- fire-and-continue
            }

            PhysicsSimulationIO::OutJointSpy lEvent = {};      // zero the pad lanes
            lEvent.mID = mJointData.GetGameID(liJointIndex).muId;       // h:612/h:613
            lEvent.mLinearStress.x  = lpSpy->mForce.x  * lfScale;       // spy +0x00 row
            lEvent.mLinearStress.y  = lpSpy->mForce.y  * lfScale;
            lEvent.mLinearStress.z  = lpSpy->mForce.z  * lfScale;
            lEvent.mAngularStress.x = lpSpy->mTorque.x * lfScale;       // spy +0x10 row
            lEvent.mAngularStress.y = lpSpy->mTorque.y * lfScale;
            lEvent.mAngularStress.z = lpSpy->mTorque.z * lfScale;
            lpQueue->AddEvent(lEvent);                                  // `bl 0x828A1C30`
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::AddDriveSpiesToOutputQueue @ 0x828A5D10   (72 instructions)
    //
    // Per DriveJacobianSpy record: chase the drive's slot tag and push the record verbatim
    // -- rows UNSCALED (the joint twin's *ts*59.999996f has no counterpart here; the vendor
    // spy already emitted force/torque), the two separations copied straight through.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::AddDriveSpiesToOutputQueue(PhysicsSimulationIO::OutputBuffer* lpOutput)
    {
        PhysicsSimulationIO::OutputBuffer::OutDriveSpyQueue* const lpQueue =
            lpOutput->GetDriveSpyQueue();                               // the 0x8259F270 accessor

        const s32 liNumSpies = static_cast<s32>(mpSimulation->GetDriveSpyCount());   // `lwz 0x78`
        for (s32 liSpy = 0; liSpy < liNumSpies; ++liSpy)
        {
            const rw_physics::DriveJacobianSpy* const lpSpy = mpSimulation->GetDriveSpy(liSpy);

            const s32 liDriveIndex = static_cast<s32>(lpSpy->mpDrive->GetTag());   // `lwz 0x18(drive)`

            PhysicsSimulationIO::OutDriveSpy lEvent = {};      // zero the pad lanes
            lEvent.mID = mDriveData.GetGameID(liDriveIndex).muId;       // h:648/h:649
            lEvent.mLinearStress.x  = lpSpy->mForce.x;                  // spy +0x00 row (whole-lane stvx)
            lEvent.mLinearStress.y  = lpSpy->mForce.y;
            lEvent.mLinearStress.z  = lpSpy->mForce.z;
            lEvent.mAngularStress.x = lpSpy->mTorque.x;                 // spy +0x10 row
            lEvent.mAngularStress.y = lpSpy->mTorque.y;
            lEvent.mAngularStress.z = lpSpy->mTorque.z;
            lEvent.mLinearDistanceToKey  = lpSpy->mSeparation;          // `lfs 0x20(spy)`
            lEvent.mAngularDistanceToKey = lpSpy->mAngSeparation;       // `lfs 0x24(spy)`
            lpQueue->AddEvent(lEvent);                                  // `bl 0x828A1D90`
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::AddActiveBodiesToOutputQueue @ 0x828A6CC8   (65 instructions)
    //
    // Sweep ALL 200 slots (not a list walk): for each in-use slot, clamp the body's centre
    // of mass into the world bounds [KV_MIN_POSITION, KV_MAX_POSITION] -- xyz only; the
    // console's `vmaxfp/vminfp` then `vrlimi128 ...,1,0` preserves the w lane, which packs
    // RigidBody::mId, and the xyz-only SetPosition IS that preservation on the host -- and
    // for each ACTIVE body push an OutUpdateRigidBody snapshot.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::AddActiveBodiesToOutputQueue(PhysicsSimulationIO::OutputBuffer* lpOutput)
    {
        PhysicsSimulationIO::OutputBuffer::OutUpdateRigidBodyQueue* const lpQueue =
            lpOutput->GetUpdateRigidBodyQueue();                        // `bl sub_8289F130`

        for (s32 liSlot = 0; liSlot < RigidBodyData::KI_SIZE; ++liSlot)
        {
            if (!mBodyData.IsSlotUsed(liSlot))      // the 64-bit sentinel compare vs qword_82F33E18
            {
                continue;
            }

            // The console reads maRWBodies[slot] raw here; the checked accessor's h:594/h:595
            // tripwires are provably true under the IsSlotUsed guard (the GetJoint folding
            // precedent in ProcessRemoveRigidBodyQueue above).
            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liSlot);
            CGS_ASSERT(lpBody != NULL, "lpRigidBody != NULL");          // .cpp:2371 -- fire-and-continue

            rw::math::vpu::Vector3 lPosition = lpBody->GetPosition();   // `lvx128 body+0x10` (mCom row)
            lPosition.x = (lPosition.x < KV_MIN_POSITION.x) ? KV_MIN_POSITION.x : (lPosition.x > KV_MAX_POSITION.x) ? KV_MAX_POSITION.x : lPosition.x;
            lPosition.y = (lPosition.y < KV_MIN_POSITION.y) ? KV_MIN_POSITION.y : (lPosition.y > KV_MAX_POSITION.y) ? KV_MAX_POSITION.y : lPosition.y;
            lPosition.z = (lPosition.z < KV_MIN_POSITION.z) ? KV_MIN_POSITION.z : (lPosition.z > KV_MAX_POSITION.z) ? KV_MAX_POSITION.z : lPosition.z;
            lpBody->SetPosition(lPosition);                             // xyz only -- w keeps mId

            if (lpBody->GetState() & rw_physics::ACTIVE_BODY)           // `lwz 0x8C` & 4
            {
                PhysicsSimulationIO::OutUpdateRigidBody lEvent = {};   // zero mIDPad; operator= fills the body
                lEvent.mRigidBody = *lpBody;                            // `bl RigidBody::operator=` (frame+0x10)
                lEvent.mID        = mBodyData.GetGameID(liSlot);        // `ld` maGameIDs[slot] -> frame+0x00
                lpQueue->AddEvent(lEvent);                              // `bl 0x828A66F8`
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::QuerySimulationToSetFlags @ 0x828A0428   (248 instructions)
    //
    // Partition the simulation's active-body ring by sleep counter, seeding the closure:
    //   * clear mNeedFreeze and mSeen; miActive := -1 (the chain-terminator);
    //   * walk the ring from m_ActiveRB_Anchor (a circular intrusive list -- GetRight until
    //     back at the anchor);
    //   * a body still "warm" (mCool < the simulation's cool-down ceiling) is pushed onto
    //     the miActive chain through mpiNextIndex and bit-set in mSeen;
    //   * a body "cold" (mCool >= ceiling) is bit-set in mNeedFreeze -- a freeze CANDIDATE,
    //     which ActiveSetClosure can still rescue (its UnSetBit path) before
    //     ActivateAndFreezeAsNeeded acts on what remains.
    // ⚠️ mDone is deliberately NOT touched here -- ActiveSetClosure clears it itself.
    //
    // The two SetBit calls carry the container's inlined bounds tripwires on the console
    // (CgsBitArray.h:222, same condition as the .cpp:2941 assert just above them); per the
    // committed CgsBitArray.h policy (bounds asserts live with callers, and duplicates of
    // an immediately-preceding check are folded) they are not re-emitted.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::QuerySimulationToSetFlags()
    {
        mNeedFreeze.UnSetAll();          // 4x `std 0` @ +0x47E8
        miActive = -1;                   // `stw -1, 0x47E0`
        mSeen.UnSetAll();                // 4x `std 0` @ +0x4828

        const rw_physics::RigidBody* const lpAnchor = mpSimulation->GetActiveBodyAnchor();   // `lwz 0x28(sim)`
        const u32 luCoolDown = mpSimulation->GetCoolDown();                                  // `lwz 0xA4(sim)`

        for (const rw_physics::RigidBody* lpBody = lpAnchor->GetRight();   // `lwz 0x2C(anchor)`
             lpBody != lpAnchor;
             lpBody = lpBody->GetRight())
        {
            const s32 liTag = static_cast<s32>(lpBody->GetTag());          // `lwz 0x6C(body)`

            // The slot table and the simulation must agree about who lives where.
            CGS_ASSERT(mBodyData.GetRigidBody(liTag) == lpBody, "Rigid Body Mapping Error!\n");   // .cpp:2940
            CGS_ASSERT(liTag < RigidBodyData::KI_SIZE, "Body tag is not a valid part index.");    // .cpp:2941 (streamed; plain-literal)

            if (lpBody->GetCoolDown() < luCoolDown)      // `lwz 0xAC(body)` -- still warm
            {
                mpiNextIndex[liTag] = miActive;          // push onto the closure's seed chain
                miActive = liTag;
                mSeen.SetBit(liTag);                     // (BitArray.h:222 tripwire folded -- see banner)
            }
            else                                         // cold long enough -- freeze candidate
            {
                mNeedFreeze.SetBit(liTag);
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ActiveSetClosure @ 0x828A0808   (1,184 instructions)
    //
    // Flood-fill the active set to closure over the three pair sets, pass by pass:
    //   * pass 0 walks the miActive seed chain QuerySimulationToSetFlags built;
    //   * every body reached is marked mDone; on passes > 0, a body carrying a
    //     mNeedFreeze bit is RESCUED (the bit cleared -- something active reached it),
    //     while a body without one joins the miNeedThaw chain (it was asleep and must wake);
    //   * each body's partners in mpContactPairs / mpJointedPairs / mpDrivenPairs (the
    //     inlined PairSet::LinkIterator walk -- see pairset.h's banner) that are not STATIC,
    //     not already done and not already seen join the NEXT pass's chain and mSeen;
    //     contact links additionally have their pair flags cleared and count against the
    //     pair budget;
    //   * the next pass walks exactly the newly-discovered set; the fill ends when a pass
    //     discovers nothing (or the depth cap trips).
    //
    // ⚠️⚠️ lpIslandGenerator IS DEAD, AND THAT IS THE SHIPPED TRUTH -- r4 is never read by
    // the X360 body (the brief that commissioned this wave claimed the union-find was
    // inlined here; it is not -- see CgsIslandGenerator.h's banner for the full evidence).
    // The parameter stays for the DWARF signature; nothing here may grow a use of it
    // without a new witness.
    //
    // ⚠️ The three limit asserts are shipped dev tripwires, transcribed with their exact
    // semantics: the depth warning fires one pass BEFORE the cap; the body cap fires only
    // on passes > 0 and BAILS OUT; the pair cap (contact links only) always bails.
    // ⚠️ The three partner walks are NOT identical and are not harmonised: the contact walk
    // reads the partner's state BEFORE the mDone test and burns pair budget + clears flags;
    // the joint/drive walks test mDone FIRST and touch no budget.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ActiveSetClosure(IslandGenerator* lpIslandGenerator, s32 liMaxDepth,
                                                   s32 liMaxPairs, s32 liMaxBodies)
    {
        (void)lpIslandGenerator;   // dead on the console -- see the banner

        mDone.UnSetAll();          // 4x `std 0` @ +0x4808
        miNeedThaw = -1;           // `stw -1, 0x47DC`

        s32 liNumBodies = 0;       // var_C8 -- total bodies processed, vs liMaxBodies
        s32 liNumPairs  = 0;       // var_CC -- contact links crossed, vs liMaxPairs
        s32 liDepth     = 0;       // var_1A0 -- pass counter, vs liMaxDepth
        s32 liCursor    = miActive;

        if (liMaxDepth < 0)
        {
            return;                // `blt` at entry
        }

        while (liCursor != -1)
        {
            CGS_ASSERT(liDepth != liMaxDepth - 1, "About to run out of depth\n");   // .cpp:3021 -- fire-and-continue
            s32 liPendingHead = -1;    // var_1A8 -- the next pass's chain

            do
            {
                ++liNumBodies;
                if (liNumBodies > liMaxBodies)
                {
                    if (liDepth > 0)
                    {
                        CGS_ASSERT(liNumBodies <= liMaxBodies, "Body limit reached\n");   // .cpp:3030
                        return;
                    }
                }

                mDone.SetBit(liCursor);                       // (BitArray.h:222 tripwire -- provably in range)
                const s32 liNext = mpiNextIndex[liCursor];    // saved BEFORE the thaw-push overwrites it

                if (liDepth > 0)
                {
                    if (mNeedFreeze.IsBitSet(liCursor))       // (:203 tripwire)
                    {
                        mNeedFreeze.UnSetBit(liCursor);       // rescued -- something active reached it (:241)
                    }
                    else
                    {
                        mpiNextIndex[liCursor] = miNeedThaw;  // asleep body dragged in -- queue the wake
                        miNeedThaw = liCursor;
                    }
                }

                // ---- contact partners (budgeted; flags cleared) -----------------------------
                for (rw_physics::PairSet::LinkIterator lIt = mpContactPairs->PartLinksBegin(liCursor);
                     lIt != mpContactPairs->PartLinksEnd(); ++lIt)
                {
                    const s32 liPartner = lIt.GetOtherPartIndex();
                    // Partner state read BEFORE the mDone test (this walk only). h:594/h:595
                    // fire from this frame on the console; here they live in the accessor.
                    const bool lbStatic =
                        (mBodyData.GetRigidBody(liPartner)->GetState() & rw_physics::STATIC_BODY) != 0;

                    if (mDone.IsBitSet(liPartner))            // (:203 tripwire)
                    {
                        continue;
                    }
                    ++liNumPairs;
                    if (liNumPairs > liMaxPairs)
                    {
                        CGS_ASSERT(liNumPairs <= liMaxPairs, "Pair limit reached\n");   // .cpp:3075
                        return;
                    }
                    mpContactPairs->SetPairFlags(lIt.GetPairIndex(), 0);   // `stw 0` on link0's flags
                    if (lbStatic)
                    {
                        continue;                             // a static partner absorbs, never joins
                    }
                    if (mSeen.IsBitSet(liPartner))            // (:203 tripwire)
                    {
                        continue;
                    }
                    mpiNextIndex[liPartner] = liPendingHead;  // join the NEXT pass's chain
                    liPendingHead = liPartner;
                    mSeen.SetBit(liPartner);                  // (:222 tripwire)
                }

                // ---- jointed partners (mDone first; no budget, no flags) --------------------
                for (rw_physics::PairSet::LinkIterator lIt = mpJointedPairs->PartLinksBegin(liCursor);
                     lIt != mpJointedPairs->PartLinksEnd(); ++lIt)
                {
                    const s32 liPartner = lIt.GetOtherPartIndex();
                    if (mDone.IsBitSet(liPartner))
                    {
                        continue;
                    }
                    if ((mBodyData.GetRigidBody(liPartner)->GetState() & rw_physics::STATIC_BODY) != 0)
                    {
                        continue;
                    }
                    if (mSeen.IsBitSet(liPartner))
                    {
                        continue;
                    }
                    mpiNextIndex[liPartner] = liPendingHead;
                    liPendingHead = liPartner;
                    mSeen.SetBit(liPartner);
                }

                // ---- driven partners (identical to the jointed walk) ------------------------
                for (rw_physics::PairSet::LinkIterator lIt = mpDrivenPairs->PartLinksBegin(liCursor);
                     lIt != mpDrivenPairs->PartLinksEnd(); ++lIt)
                {
                    const s32 liPartner = lIt.GetOtherPartIndex();
                    if (mDone.IsBitSet(liPartner))
                    {
                        continue;
                    }
                    if ((mBodyData.GetRigidBody(liPartner)->GetState() & rw_physics::STATIC_BODY) != 0)
                    {
                        continue;
                    }
                    if (mSeen.IsBitSet(liPartner))
                    {
                        continue;
                    }
                    mpiNextIndex[liPartner] = liPendingHead;
                    liPendingHead = liPartner;
                    mSeen.SetBit(liPartner);
                }

                liCursor = liNext;
            }
            while (liCursor != -1);

            ++liDepth;                    // one pass done; the discoveries become the chain
            liCursor = liPendingHead;
            if (liDepth > liMaxDepth)
            {
                return;                   // `ble` back to the top otherwise
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ActivateAndFreezeAsNeeded @ 0x828A6DD0   (249 instructions)
    //
    // Act on the partition the two functions above computed:
    //   * PHASE 1 -- walk the miNeedThaw chain and Activate every body on it that is not
    //     already ACTIVE. ⚠️ The simulation is reached THROUGH THE BODY here
    //     (body->GetSimulation()->ActivateRigidBody) where phase 2 uses mpSimulation --
    //     both shapes are the console's, not harmonised.
    //   * PHASE 2 -- for every body still bit-set in mNeedFreeze: Freeze it (unless already
    //     FROZEN) and emit an OutUpdateRigidBody snapshot so the game sees the body's final
    //     resting state. The bit iteration is BitArray::GetFirst/GetNextNonZeroBit (the
    //     console open-codes the word-skip + bit-probe; value-identical).
    //   * then clear mNeedFreeze and reset miNeedThaw.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ActivateAndFreezeAsNeeded(PhysicsSimulationIO::OutputBuffer* lpOutput)
    {
        PhysicsSimulationIO::OutputBuffer::OutUpdateRigidBodyQueue* const lpQueue =
            lpOutput->GetUpdateRigidBodyQueue();                        // `bl sub_8289F130`

        // ---- phase 1: the thaw chain ----------------------------------------------------
        for (s32 liIndex = miNeedThaw; liIndex != -1; liIndex = mpiNextIndex[liIndex])
        {
            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liIndex);   // `bl 0x8289D020`
            if (!(lpBody->GetState() & rw_physics::ACTIVE_BODY))
            {
                lpBody->GetSimulation()->ActivateRigidBody(lpBody);     // `lwz 0x4C(body)` as this
            }
        }

        // ---- phase 2: the freeze set ----------------------------------------------------
        for (s32 liIndex = mNeedFreeze.GetFirstNonZeroBit(); liIndex >= 0;
             liIndex = mNeedFreeze.GetNextNonZeroBit(liIndex))
        {
            // h:594/h:595 fire from this frame on the console (the inlined checked slot
            // read); here they live in the accessor's own body.
            rw_physics::RigidBody* const lpBody = mBodyData.GetRigidBody(liIndex);
            if (!(lpBody->GetState() & rw_physics::FROZEN_BODY))        // `lwz 0x8C` & 2
            {
                mpSimulation->FreezeRigidBody(lpBody);                  // `lwz 0x4864(this)` as this
            }

            PhysicsSimulationIO::OutUpdateRigidBody lEvent = {};   // zero mIDPad; operator= fills the body
            lEvent.mRigidBody = *lpBody;                                // `bl RigidBody::operator=` first...
            lEvent.mID        = mBodyData.GetGameID(liIndex);           // ...then the raw id re-read
                                                                        // (GetGameID's h:585/h:586 are
                                                                        // provably true under the reads above)
            lpQueue->AddEvent(lEvent);                                  // `bl 0x828A66F8`
        }

        mNeedFreeze.UnSetAll();     // 4x `std 0`
        miNeedThaw = -1;            // `stw -1, 0x47DC`
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::Update @ 0x828A74D0   (127 instructions) -- ONE TICK.
    //
    // The call order below is the console's own and is load-bearing end to end: drains under
    // the input read-lock; the closure between the perf-mon phase brackets; spy-counter
    // resets around SimulationUpdate; the output emitters under the output write-lock, with
    // the input re-locked just for the contact-spy join.
    //
    // ⚠️ lpUnusedStack (the second IOBufferStack) is accepted and never read -- dead r5 in
    // the shipped body. ⚠️ The two SimulationUpdate-bracketing resets are the DWARF's own
    // HackResetSpyCountHack / ResetContactStack (simulation.h :294/:286), inlined by the
    // console as the +0x68/+0x78/+0x70 and +0x64/+0x68 stores.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::Update(CgsModule::IOBufferStack* lpTempStack,
                                         CgsModule::IOBufferStack* lpUnusedStack,
                                         const PhysicsSimulationIO::InputBuffer* lpInput,
                                         PhysicsSimulationIO::OutputBuffer* lpOutput)
    {
        (void)lpUnusedStack;   // dead on the console

        CgsDev::PerfMonCpu::StartMonitor(miTimeInSim1);        // "Sim setup"
        lpInput->LockForRead();
        ProcessInputBuffers(lpInput);

        const f32 lfNewStep = lpInput->GetTimeStep();          // `bl sub_8289E260` -- the READ-lock const overload
        CGS_ASSERT(lfNewStep > 0.0f, "lfNewStep > 0.0f");                              // .cpp:794
        CGS_ASSERT(lpInput->GetMaxIterations() > 0, "lpInput->GetMaxIterations() > 0");   // .cpp:867
        mpSimulation->SetMaxIteration(static_cast<u32>(lpInput->GetMaxIterations()));  // `stw 0xAC(sim)`

        lpInput->UnlockForRead();
        lpOutput->LockForWrite();
        CgsDev::PerfMonCpu::StopMonitor(miTimeInSim1);
        CgsDev::PerfMonCpu::StartMonitor(miTimeInSim2);        // "Sim freezing"

        IslandGenerator* lpIslandGenerator = NULL;
        lpTempStack->CreateIOBuffer(&lpIslandGenerator, NULL); // `bl 0x8289E0D0` -- Alloc(0x4B2)
        QuerySimulationToSetFlags();
        ActiveSetClosure(lpIslandGenerator, 100, 1024, 200);   // `li 0x64 / 0x400 / 0xC8`
        ActivateAndFreezeAsNeeded(lpOutput);
        mpContactPairs->ClearAll();

        CgsDev::PerfMonCpu::StopMonitor(miTimeInSim2);
        CgsDev::PerfMonCpu::StartMonitor(miTimeInSim3);        // "Sim update"
        // ⭐⭐ 2026-08-10 (root-cause wave): THIS LINE NOW RUNS. With the world frame timer
        // staged in DriveWorldUpdateFrame and both root causes closed, the first tick was
        // witnessed here as `dt=0.016667 iters=2 bodySlotsUsed=0 bodiesACTIVE=0` -- the real
        // 1/60 s step, carrying the solver cap the console's
        // BridgeEntityModulesToPhysicsModule_PreScene delivers, over an EMPTY body set
        // (the vehicle create path is still absent). The temporary witness prints that
        // recorded it were removed after observation, as briefed.
        mpSimulation->HackResetSpyCountHack();                 // `stw 0, 0x68/0x78/0x70`
        mpSimulation->SimulationUpdate(lfNewStep);
        CgsDev::PerfMonCpu::StopMonitor(miTimeInSim3);
        CgsDev::PerfMonCpu::StartMonitor(miTimeInSim4);        // "Sim output"

        lpOutput->SetTimeStepUsed(mpSimulation->GetTimeStep());                       // `lfs 0xA0(sim)`
        lpOutput->SetMaxIterationsUsed(static_cast<s32>(mpSimulation->GetMaxIteration()));   // `lwz 0xAC(sim)`
        AddActiveBodiesToOutputQueue(lpOutput);

        lpInput->LockForRead();                                // re-locked JUST for the spy join
        AddContactSpiesToOutputQueue(lpInput, lpOutput);
        lpInput->UnlockForRead();

        AddJointSpiesToOutputQueue(lpOutput);
        AddDriveSpiesToOutputQueue(lpOutput);
        lpOutput->UnlockForWrite();

        mpSimulation->ResetContactStack();                     // `stw 0, 0x64/0x68`
        CgsDev::PerfMonCpu::StopMonitor(miTimeInSim4);
        lpTempStack->DestroyIOBuffer(&lpIslandGenerator);      // `bl 0x8289E190` -- Free(0x4B2)
    }

    // -------------------------------------------------------------------------------------
    // PhysicsSimulationModule::ProcessInput @ 0x828A76D0   (33 instructions)
    //
    // The drain-only entry: no solver step, so a contact event arriving through this path
    // would be generated against bodies that never simulate -- hence the emptiness assert.
    // -------------------------------------------------------------------------------------
    void PhysicsSimulationModule::ProcessInput(const PhysicsSimulationIO::InputBuffer* lpInput)
    {
        lpInput->LockForRead();
        CGS_ASSERT(lpInput->GetAddContactQueue()->GetLength() == 0,
                   "It's invalid to add contacts during a ProcessInput update");   // .cpp:961 -- fire-and-continue
        ProcessInputBuffers(lpInput);
        lpInput->UnlockForRead();
    }
}
