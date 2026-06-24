#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line members of the CgsPhysics::PhysicsSimulationModule slot tables
// JointData and DriveData. Reconstructed store-for-store from the X360 asm; the
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
    }

    // X360 @0x8289D168. Checked slot read returning maRWJoints[liIndex].
    rw_physics::Joint* JointData::GetJoint(s32 liIndex)
    {
        CGS_ASSERT(liIndex < KI_SIZE, "liIndex < knSize");          // h:621 tripwire (blt skips)
        CGS_ASSERT(mabUsedSlot[liIndex], "mabUsedSlot[liIndex]");   // h:622 tripwire (bne skips)
        return maRWJoints[liIndex];
    }

    // ===================== DriveData =======================================

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
}
