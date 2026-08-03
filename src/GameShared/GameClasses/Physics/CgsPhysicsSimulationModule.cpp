#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // AddMonitor

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

    // DriveDynamics interior (promoted from opaque 2026-08-03). Pins the four fields the
    // module constructor writes by member, and the two-Params split.
    static_assert(sizeof(DriveDynamics::Params)            == 16, "DriveDynamics::Params is 16B (drivedynamics.h:204-207)");
    static_assert(offsetof(DriveDynamics::Params, mfSpring)   == 0,  "Params.mSpring   @+0x00");
    static_assert(offsetof(DriveDynamics::Params, mfDamping)  == 4,  "Params.mDamping  @+0x04");
    static_assert(offsetof(DriveDynamics::Params, mfStrength) == 8,  "Params.mStrength @+0x08");
    static_assert(offsetof(DriveDynamics::Params, meType)     == 12, "Params.mType     @+0x0C (asm stw 0, 0xC(r11))");
    static_assert(offsetof(DriveDynamics, mLinear)  == 0,  "DriveDynamics.mLinear  @+0x00 (drivedynamics.h:261)");
    static_assert(offsetof(DriveDynamics, mAngular) == 16, "DriveDynamics.mAngular @+0x10 (asm second quad at +0x10..+0x1C)");

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
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            DriveDynamics& lDynamics = maDynamics[li];
            lDynamics.mLinear.mfSpring    = 0.0f;      // stfs f0, 0(r11)
            lDynamics.mLinear.mfDamping   = 0.0f;      // stfs f0, 4(r11)
            lDynamics.mLinear.mfStrength  = 0.0f;      // stfs f0, 8(r11)
            lDynamics.mLinear.meType      = E_NO_DRIVE; // stw r9(=0), 0xC(r11)
            lDynamics.mAngular.mfSpring   = 0.0f;      // stfs f0, 0x10(r11)
            lDynamics.mAngular.mfDamping  = 0.0f;      // stfs f0, 0x14(r11)
            lDynamics.mAngular.mfStrength = 0.0f;      // stfs f0, 0x18(r11)
            lDynamics.mAngular.meType     = E_NO_DRIVE; // stw r9(=0), 0x1C(r11)

            DriveDynamics& lScaled = maScaledDynamics[li];
            lScaled.mLinear.mfSpring    = 0.0f;        // stfs f0, 0(r10)
            lScaled.mLinear.mfDamping   = 0.0f;        // stfs f0, 4(r10)
            lScaled.mLinear.mfStrength  = 0.0f;        // stfs f0, 8(r10)
            lScaled.mLinear.meType      = E_NO_DRIVE;  // stw r9(=0), 0xC(r10)
            lScaled.mAngular.mfSpring   = 0.0f;        // stfs f0, 0x10(r10)
            lScaled.mAngular.mfDamping  = 0.0f;        // stfs f0, 0x14(r10)
            lScaled.mAngular.mfStrength = 0.0f;        // stfs f0, 0x18(r10)
            lScaled.mAngular.meType     = E_NO_DRIVE;  // stw r9(=0), 0x1C(r10)
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

    // ===================== RigidBodyData ===================================

    // CgsRigidBody.h:87: the module-global invalid-rigid-body sentinel (X360 static
    // qword_82F33E18 == 0xFFFFFFFFFFFFFFFF). RigidBodyId::IsInvalid() compares against it.
    const RigidBodyId K_INVALID_RIGID_BODY_ID = { 0xFFFFFFFFFFFFFFFFull };

    // Host-stable element strides (X360-attested).
    static_assert(sizeof(Inertia)     == 48, "Inertia stride 48 (GetInertia 48*(idx+50))");
    static_assert(sizeof(RigidBodyId) == 8,  "RigidBodyId is a single u64 (CgsRigidBody.h:48)");

    // Inertia interior (promoted from opaque 2026-08-03). Pins the six scalars the
    // RigidBodyData constructor writes at +0x10..+0x24 and the leading 16-byte vector.
    static_assert(offsetof(Inertia, mafInvTens)    == 0,  "mInvTens @+0x00 (asm stvx128 v0, r0, r11)");
    static_assert(offsetof(Inertia, mfInvMass)     == 16, "mInvMass @+0x10 (asm stfs f0(1.0f), 0x10(r11))");
    static_assert(offsetof(Inertia, mfSpherical)   == 20, "mSpherical @+0x14 (asm stfs f0(1.0f), 0x14(r11))");
    static_assert(offsetof(Inertia, mfMaxVelocity) == 24, "mMaxVelocity @+0x18 (asm stfs f12(FLT_MAX), 0x18(r11))");
    static_assert(offsetof(Inertia, mfMaxOmega)    == 28, "mMaxOmega @+0x1C (asm stfs f12(FLT_MAX), 0x1C(r11))");
    static_assert(offsetof(Inertia, mfLinearDrag)  == 32, "mLinearDrag @+0x20 (asm stfs f13(0.0f), 0x20(r11))");
    static_assert(offsetof(Inertia, mfAngularDrag) == 36, "mAngularDrag @+0x24 (asm stfs f13(0.0f), 0x24(r11))");

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
    RigidBodyData::RigidBodyData()
    {
        for (s32 li = 0; li < KI_SIZE; ++li)
        {
            Inertia& lInertia = maInertias[li];
            lInertia.mafInvTens[0] = 1.0f;          // stack vector lane 0 -> stvx128 to +0x00
            lInertia.mafInvTens[1] = 1.0f;          // lane 1
            lInertia.mafInvTens[2] = 1.0f;          // lane 2
            lInertia.mafInvTens[3] = 0.0f;          // lane 3 (the Vector3 pad; stw 0)
            lInertia.mfInvMass     = 1.0f;          // stfs f0,  0x10(r11)
            lInertia.mfSpherical   = 1.0f;          // stfs f0,  0x14(r11)
            lInertia.mfMaxVelocity = FLT_MAX;       // stfs f12, 0x18(r11)
            lInertia.mfMaxOmega    = FLT_MAX;       // stfs f12, 0x1C(r11)
            lInertia.mfLinearDrag  = 0.0f;          // stfs f13, 0x20(r11)
            lInertia.mfAngularDrag = 0.0f;          // stfs f13, 0x24(r11)
        }

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
}
