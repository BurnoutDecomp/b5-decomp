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
// entry BY MEMBER.
//
// X360 byte offsets in the comments are the console's 4-byte-pointer ABI. The
// host gate builds x64, so absolute offsets past a pointer array widen; every
// access here is BY NAME, and the layout gate in the .cpp asserts the
// pointer-width-independent relationships (see _AssertLayout).

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<200>
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // the module base class

// rw::physics pointer members of PhysicsSimulationModule. Forward declarations
// only -- the module never dereferences them in the functions homed here. Class
// keys match their real homes (vendor/renderware/include/rw/physics/{simulation,
// pairset}.h), so no ODR/class-key mismatch.
namespace rw { namespace physics { class Simulation; class PairSet; } }

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

    // rw::physics::Inertia. 48-byte X360-attested array stride (GetInertia
    // @0x8289D0C0 indexes maInertias at 48*(idx+50)).
    //
    // PROMOTED from an opaque 48-byte span (was: u8 macOpaque[48]). DWARF
    // (rw/physics/inertia.h:176..:182) names the interior -- Vector3 mInvTens
    // then six f32 (mInvMass, mSpherical, mMaxVelocity, mMaxOmega, mLinearDrag,
    // mAngularDrag) == 40 bytes, padded to 48 by the leading Vector3's 16-align --
    // and RigidBodyData's constructor (X360 @0x827DB728) default-initialises every
    // maInertias[] entry BY MEMBER, so the interior is now required.
    // Vector3 is carried as a 4-lane f32 array, matching the JointLimits
    // mafPprism/mafVprism precedent immediately below (the console's stvx128 of a
    // 16-byte {1,1,1,0} stack vector writes all four lanes).
    struct alignas(16) Inertia
    {
        f32 mafInvTens[4];    // @+0x00  Vector3 mInvTens (inertia.h:176); lane 3 is the pad
        f32 mfInvMass;        // @+0x10  (:177)
        f32 mfSpherical;      // @+0x14  (:178)
        f32 mfMaxVelocity;    // @+0x18  (:179)
        f32 mfMaxOmega;       // @+0x1C  (:180)
        f32 mfLinearDrag;     // @+0x20  (:181)
        f32 mfAngularDrag;    // @+0x24  (:182)
    };

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

    namespace rw_physics
    {
        struct Joint;      // opaque rw::physics::Joint (maRWJoints holds Joint*)
        struct Drive;      // opaque rw::physics::Drive (maRWDrives holds Drive*)
        struct RigidBody;  // opaque rw::physics::RigidBody (maRWBodies holds RigidBody*)
    }

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
    // export-set hole. What is still NOT declared, deliberately, is the three new virtuals at
    // slots 16/17/18:
    //     Prepare(rw::IResourceAllocator*, const SimulationParams&) @0x828A6A08
    //     Update(IOBufferStack*, IOBufferStack*, const InputBuffer*, OutputBuffer*) @0x828A74D0
    //     ProcessInput(const InputBuffer*) @0x828A76D0
    // Declaring a virtual with no body while a constructor is defined materialises
    // the vtable and turns straight into LNK2019, so they land WITH their bodies,
    // not before. Until then those slots keep the base's behaviour.
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

        // X360 @0x828A2048 (54 instructions). Console vtable slot 2. A fall-through
        // release FSM over meReleaseStage; returns false (cursor left on stage 1) when
        // the base's Release has not finished, so the owner re-enters next frame.
        bool Release() override;

        // X360 @0x828A2120 (16 instructions). Console vtable slot 3. ⚠️ Absent from
        // .ida-exports -- recovered from BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3
        // (the same export-set hole RigidBodyData::RigidBodyData was).
        void Destruct() override;

    private:
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
}
