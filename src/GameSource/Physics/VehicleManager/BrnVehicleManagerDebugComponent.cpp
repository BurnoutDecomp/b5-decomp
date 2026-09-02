// ============================================================================================
// BrnPhysics::Vehicle::VehicleManagerDebugComponent - Construct + GetName.
//
// Layout/shape authority: references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/
// BrnVehicleManagerDebugComponent.h (complete member sequence) + the X360 ARTIST asm.
// Every offset quoted in the comments is asm-literal; see the header banner for the three
// independent closures that pin the class to 1296 bytes.
// ============================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"

#include <cstddef>                                          // offsetof (layout asserts)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"                   // VehicleManager (RecordCrashContact's two bare reads; friend)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"       // RaceCarPhysics (GetTransform / GetDeformableAABB / GetLinearVelocity)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"         // CgsGeometric::AxisAlignedBox
#include "GameSource/World/BrnEntityTypes.h"                                        // BrnWorld::E_ENTITYTYPE_*

namespace BrnPhysics
{
namespace Vehicle
{
    // ----------------------------------------------------------------------------------------
    // The "invalid volume instance" sentinel the two embedded PotentialContacts are seeded with.
    //
    // X360: `ld rN, qword_82F2A3C8` -> stored to both PotentialContact::muVolumeInstanceId{A,B}.
    // The slot is INITIALISED IN THE IMAGE -- read directly out of BURNOUT_X360_ARTIST.XEX.id1
    // with the self-calibrating reader (delta -1594, 9/9 witnesses):
    //     0x82F2A3C8 = FF FF FF FF FF FF FF FF
    // so this is NOT one of the static-init-filled slots that read zero in the image. (Its
    // neighbour qword_82F2A3A8, the invalid RigidBodyId that VehicleManager::Construct seeds
    // maRaceCarHandlingBodyIDs with, reads the same all-ones value.)
    // ----------------------------------------------------------------------------------------
    static const u64 KU64_INVALID_VOLUME_INSTANCE_ID = 0xFFFFFFFFFFFFFFFFull;   // qword_82F2A3C8

    // ========================================================================================
    // VehicleManagerDebugComponent::Construct   @ 0x825B5A78   (194 instructions)
    // DWARF: BrnVehicleManagerDebugComponent.cpp:87  `void Construct(VehicleManager *)`
    //
    // The X360 scheduler interleaves this function's stores almost completely (the six 16-byte
    // stack scratch slots at sp+0x50..sp+0xAC are re-written between every `lvx128`), so the
    // stores are re-ordered here into the DWARF member sequence. Every store targets a distinct
    // member, so the ordering is behaviourally inert; the SET of stores is what is faithful, and
    // it is exact -- including the four members Construct deliberately leaves alone.
    //
    // NOT written by the X360 body (deliberate, verified by scanning every store in the range):
    //   miNumWorldContactSpies, mbDisplayContact, mfOtherCarContactAngleRad,
    //   mPlayerCarContactPosition, mOtherCarContactPosition, mClosingVelocityPlayerCarSpace,
    //   mClosingVelocityOtherCarSpace, mPlayerCarVelocity, mOtherCarVelocity,
    //   mLastTrafficContact_RaceCarVelocity, mLastTrafficContact_TrafficVelocity.
    // ========================================================================================
    void VehicleManagerDebugComponent::Construct(VehicleManager* lpVehicleManager)
    {
        // 0x825B5A8C  bl <0x8284CB38>. That target is a single `blr` -- the ICF representative
        // for every empty function in the image (IDA names it
        // CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct). Called with r3 ==
        // this at the very top of a Construct, it is the base component's own Construct.
        DebugComponent::Construct();

        // 0x825B5A90..0x825B5AB4, BrnVehicleManagerDebugComponent.cpp:92
        CGS_ASSERT(lpVehicleManager != 0, "lpVehicleManager != NULL");

        mbDisplayWorldContactSpies      = false;             // stb 0, 0x1D4(r31)
        mpVehicleManager                = lpVehicleManager;  // stw r29, 0x1D8(r31)
        mbSlamDebugRenderEnabled        = false;             // stb 0, 0x1DC(r31)
        mbRenderResetLines              = false;             // stb 0, 0x1DD(r31)
        mbGrindDebugRenderEnabled       = false;             // stb 0, 0x1DE(r31)

        // The eight consecutive render gates at 0x250..0x257.
        mbDrawLastRaceCarTrafficContact  = false;            // stb 0, 0x250(r31)
        mbDrawLastSlamInfo               = false;            // stb 0, 0x251(r31)
        mbDrawLastShuntInfo              = false;            // stb 0, 0x252(r31)
        mbDrawLastCrashContact           = false;            // stb 0, 0x253(r31)
        mbDrawCatchupTargets             = false;            // stb 0, 0x254(r31)
        mbRenderWallContacts             = false;            // stb 0, 0x255(r31)
        mbRenderGroundContacts           = false;            // stb 0, 0x256(r31)
        mbDrawPlayerStuckInCollisionTests = false;           // stb 0, 0x257(r31)

        // ---- last race-car-vs-traffic contact -----------------------------------------------
        // Traced store-by-store out of the interleaved stack scratch: both matrices resolve to
        // {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0}, i.e. Matrix44Affine::SetIdentity (wAxis zero,
        // not (0,0,0,1)) -- the same identity Camera::Clear @0x8223CE70 already documents.
        mLastTrafficContact_RaceCarTransform.SetIdentity();  // 4 x stvx128 @ r9 = this + 0x260
        mLastTrafficContact_TrafficTransform.SetIdentity();  // 4 x stvx128 @ r8 = this + 0x2A0
        mLastTrafficContact_RaceCarHalfExt.SetZero();        // stvx128 v0, r31, r26 (0x2E0)
        mLastTrafficContact_TrafficHalfExt.SetZero();        // stvx128 v0, r31, r26 (0x2F0)

        // PotentialContact::Construct() INLINED at r10 = this + 0x320. There is no out-of-line
        // CgsSceneManager::SceneManagerIO::PotentialContact::Construct symbol anywhere in the
        // ARTIST export set, so (per the standing convention for inlined helpers with no
        // out-of-line body) the expansion is transcribed store-for-store rather than invented as
        // a body in the scene-manager group's header. The nine stores map field-for-field onto
        // the committed CgsPotentialContact.h member sequence.
        mLastTrafficContact_Contact.mPointOnA.SetZero();                                  // stvx128 v0, r0,  r10
        mLastTrafficContact_Contact.mPointOnB.SetZero();                                  // stvx128 v0, r10, 0x10
        mLastTrafficContact_Contact.mNormal.SetZero();                                    // stvx128 v0, r10, 0x20
        mLastTrafficContact_Contact.muVolumeInstanceIdA.muId = KU64_INVALID_VOLUME_INSTANCE_ID; // std, 0x30(r10)
        mLastTrafficContact_Contact.muVolumeInstanceIdB.muId = KU64_INVALID_VOLUME_INSTANCE_ID; // std, 0x38(r10)
        mLastTrafficContact_Contact.muPolyTagA = 0;                                       // stw 0, 0x40(r10)
        mLastTrafficContact_Contact.muPolyTagB = 0;                                       // stw 0, 0x44(r10)
        mLastTrafficContact_Contact.mu16PrimitiveIndexA = 0;                              // sth 0, 0x48(r10)
        mLastTrafficContact_Contact.mu16PrimitiveIndexB = 0;                              // sth 0, 0x4A(r10)

        // ---- last crash contact --------------------------------------------------------------
        mLastCrashContact_RaceCarTransform.SetIdentity();    // 4 x stvx128 @ r10 = this + 0x370
        mLastCrashContact_RaceCarHalfExt.SetZero();          // stvx128 v0, r31, r28 (0x3B0)
        mLastCrashContact_RaceCarVelocity.SetZero();         // stvx128 v0, r31, r27 (0x3C0)

        // The second inlined PotentialContact::Construct(), at r10 = this + 0x3D0 -- byte for byte
        // the same nine stores as the first, which is what attests the expansion.
        mLastCrashContact_Contact.mPointOnA.SetZero();
        mLastCrashContact_Contact.mPointOnB.SetZero();
        mLastCrashContact_Contact.mNormal.SetZero();
        mLastCrashContact_Contact.muVolumeInstanceIdA.muId = KU64_INVALID_VOLUME_INSTANCE_ID;
        mLastCrashContact_Contact.muVolumeInstanceIdB.muId = KU64_INVALID_VOLUME_INSTANCE_ID;
        mLastCrashContact_Contact.muPolyTagA = 0;
        mLastCrashContact_Contact.muPolyTagB = 0;
        mLastCrashContact_Contact.mu16PrimitiveIndexA = 0;
        mLastCrashContact_Contact.mu16PrimitiveIndexB = 0;

        // ---- last slam / last shunt ------------------------------------------------------------
        // The two `li r30, -1` stores are E_IMPACT_SITUATION_INVALID (BrnVehicleConstants.h).
        meLastSlamImpactSituation  = E_IMPACT_SITUATION_INVALID;  // stw r30(-1), 0x420(r31)
        miLastSlamNumber           = 0;                           // stw 0,       0x424(r31)
        mfLastSlamDuration         = 0.0f;                        // stfs f0,     0x428(r31)
        mfLastSlamBaseDuration     = 0.0f;                        // stfs f0,     0x42C(r31)
        mfLastSlamMassFactor       = 0.0f;                        // stfs f0,     0x430(r31)
        meLastShuntImpactSituation = E_IMPACT_SITUATION_INVALID;  // stw r30(-1), 0x434(r31)
        mfLastShuntMagnitude       = 0.0f;                        // stfs f0,     0x438(r31)
        mfLastShuntClosingSpeed    = 0.0f;                        // stfs f0,     0x43C(r31)

        // ---- the stuck-in-collision line tests ------------------------------------------------
        // The tail loop @0x825B5D58: r9 starts at this + 0x480 (== &maStuckInCollisionLineTestEnd[0])
        // and each iteration stores a zero vector at [r9-64] (Start), [r9] (End) and [r9+64] (Point)
        // then `stbx` a zero byte at this + 0x500 + i, advancing r9 by 16. Four iterations.
        for (s32 liTest = 0; liTest < KI_MAX_WORLD_CONTACT_SPIES; ++liTest)
        {
            maStuckInCollisionLineTestStart[liTest].SetZero();   // stvx128 v0, r9, -0x40
            maStuckInCollisionLineTestEnd[liTest].SetZero();     // stvx128 v0, r0,  r9
            maStuckInCollisionLineTestPoint[liTest].SetZero();   // stvx128 v0, r9,  0x40
            mabStuckInCollisionIntersection[liTest] = false;     // stbx    r11, r27, r3
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::GetName   @ 0x825B5D80
    //   lis r11, aVehicleManager@ha ; addi r3, r11, aVehicleManager@l ; blr
    // ========================================================================================
    const char* VehicleManagerDebugComponent::GetName() const
    {
        return "Vehicle Manager";
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RecordCrashContact   @ 0x825B7880   (114 instructions)
    //
    // AN EXPORT HOLE -- decoded from the raw image words (tools/re/x360rd.py 825B7880 456); the
    // next export starts at 0x825B7A48 and the body's `b __restgprlr_26` sits at 0x825B7A44.
    // Word-by-word:
    //   825B788C  mr r29, r4 ; mr r31, r3            lpPotentialContact / this
    //   825B7898  cmplwi r29, 0 -> assert :1045       "lpPotentialContact != NULL"
    //   825B78C0  ld 0x30 / ld 0x38 ; srdi 32          the two entity words (r28 = A, r27 = B)
    //   825B78D8  srwi r28, 24 ; cmplwi 1 -> :1053     "lEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"
    //   825B7900  srwi r27, 24 ; cmplwi 0 ; bne RET    B not WORLD -> silent return (a test, not an assert)
    //   825B7908  rlwinm r10, r28, 22,18,31            idx = (A >> 10) & 0x3FFF
    //   825B7914  lwz r11, 0x1D8(this)                 mpVehicleManager
    //   825B791C  lwzx r9, r11, 0x2A0AC ; cmpw ; bne   idx != mePlayerActiveRaceCarIndex -> return
    //   825B7928  mulli r30, r10, 0x1460 ; lbz 0xE50   maRaceCarVehicles[idx].mbCrashing -> return if set
    //   825B793C  ld 0x400 / ld 0x408 (this)           mLastCrashContact_Contact.muVolumeInstanceId{A,B}
    //             cmplw B, last.A ; cmplw A, last.B -> assert :1065 (fires only if BOTH match)
    //   825B79A8  addi r10, rec, 0x10 ; 4 x lvx128/stvx128 -> +0x370  (mTransform -> _RaceCarTransform)
    //   825B79F4  lvx128 rec+0x6D0 / rec+0x6E0 ; vsubfp ; vrefp(2.0) + 2 NR ; vmulfp128 ; stvx128 +0x3B0
    //             (max - min) / 2  -> _RaceCarHalfExt   (vcfsx v0,2 / vcfsx v13,1 are the 2.0 and 1.0)
    //   825B7A18  mtctr 10 ; ld/std x10                 80-byte contact -> +0x3D0 (_Contact)
    //   825B7A38  lvx128 rec+0x50 ; stvx128 +0x3C0      mLinearVelocity -> _RaceCarVelocity
    // The friend grant in BrnVehicleManager.h carries the two bare manager reads.
    // ========================================================================================
    void VehicleManagerDebugComponent::RecordCrashContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact* lpPotentialContact)
    {
        CGS_ASSERT(lpPotentialContact != 0, "lpPotentialContact != NULL");   // :1045

        const u32 luEntityIdA = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdA.muId >> 32);   // r28
        const u32 luEntityIdB = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdB.muId >> 32);   // r27

        CGS_ASSERT((luEntityIdA >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
                   "lEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :1053

        // 0x825B7904: a race-car-vs-non-world contact is simply not recorded here.
        if ((luEntityIdB >> 24) != static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD))
            return;

        // 0x825B7908..0x825B7938: the player's slot only, and only before it is crashing.
        const u32 luRaceCarIndex = (luEntityIdA >> 10) & 0x3FFFu;
        if (luRaceCarIndex != static_cast<u32>(mpVehicleManager->mePlayerActiveRaceCarIndex))
            return;

        const RaceCarPhysics& lrRaceCar = mpVehicleManager->maRaceCarVehicles[luRaceCarIndex];
        if (lrRaceCar.IsCrashing())
            return;

        // 0x825B793C..0x825B797C (:1065) -- the same pair, mirrored, must not already be the record.
        CGS_ASSERT(luEntityIdB != static_cast<u32>(mLastCrashContact_Contact.muVolumeInstanceIdA.muId >> 32)
                   || luEntityIdA != static_cast<u32>(mLastCrashContact_Contact.muVolumeInstanceIdB.muId >> 32),
                   "lEntityIdB != mLastCrashContact_Contact.muVolumeInstanceIdA.GetEntityId() || "
                   "lEntityIdA != mLastCrashContact_Contact.muVolumeInstanceIdB.GetEntityId()");

        mLastCrashContact_RaceCarTransform = lrRaceCar.GetTransform();                     // rec +0x10 -> +0x370

        const CgsGeometric::AxisAlignedBox& lrBox = lrRaceCar.GetDeformableAABB();         // rec +0x6D0 / +0x6E0
        mLastCrashContact_RaceCarHalfExt = Vector3{ (lrBox.mMax.x - lrBox.mMin.x) / 2.0f,   // -> +0x3B0
                                                    (lrBox.mMax.y - lrBox.mMin.y) / 2.0f,
                                                    (lrBox.mMax.z - lrBox.mMin.z) / 2.0f,
                                                    (lrBox.mMax.w - lrBox.mMin.w) / 2.0f };

        mLastCrashContact_Contact         = *lpPotentialContact;                           // 80 bytes -> +0x3D0
        mLastCrashContact_RaceCarVelocity = lrRaceCar.GetLinearVelocity();                 // rec +0x50 -> +0x3C0
    }

    // ========================================================================================
    // Layout pins. Never called.
    //
    // These are written as RELATIVE deltas, not absolute offsets, because two members widen
    // on x64 (the base's vtable pointer and mpVehicleManager) and shift everything after them.
    // Every delta below IS X360-identical, and every number is asm-literal:
    //   * the head deltas are the byte gaps between the asm's own store targets;
    //   * from mPlayerCarContactPosition onwards there is not a single pointer left in the
    //     class, so that whole tail's offsets are identical on both ABIs and are pinned against
    //     one anchor (KI_HEAD == offsetof(mPlayerCarContactPosition), 480 on X360).
    // ========================================================================================
    void VehicleManagerDebugComponent::_AssertLayout()
    {
        typedef VehicleManagerDebugComponent V;

        // ---- the spy array: 4 elements at the X360-attested 112-byte stride --------------------
        // (stride from BaseEventQueue<OutContactSpy>::AddEvent @0x825E44C8 / AddEventSafe
        // @0x828A1B78; 16 + 4*112 == 464 == miNumWorldContactSpies is the closure.)
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::OutContactSpy) == 112,
                      "OutContactSpy stride (X360-attested 112)");
        static_assert(offsetof(V, miNumWorldContactSpies) - offsetof(V, maWorldContactSpies) == 4 * 112,
                      "maWorldContactSpies[4] fills 448 bytes and ends at miNumWorldContactSpies");
        static_assert(offsetof(V, mbDisplayWorldContactSpies) - offsetof(V, miNumWorldContactSpies) == 4,
                      "mbDisplayWorldContactSpies (asm stb 0x1D4) is one word past miNumWorldContactSpies");
        static_assert(offsetof(V, mpVehicleManager) - offsetof(V, mbDisplayWorldContactSpies) == 4,
                      "mpVehicleManager (asm stw r29, 0x1D8) is one word past the display flag");
        static_assert(offsetof(V, mbSlamDebugRenderEnabled) - offsetof(V, mpVehicleManager) == sizeof(void*),
                      "mbSlamDebugRenderEnabled (asm stb 0x1DC) sits immediately after the manager pointer");
        static_assert(offsetof(V, mbRenderResetLines)        - offsetof(V, mbSlamDebugRenderEnabled) == 1,
                      "mbRenderResetLines (asm stb 0x1DD)");
        static_assert(offsetof(V, mbGrindDebugRenderEnabled) - offsetof(V, mbSlamDebugRenderEnabled) == 2,
                      "mbGrindDebugRenderEnabled (asm stb 0x1DE)");

        // ---- the pointer-free tail, anchored at mPlayerCarContactPosition (X360 +480) ----------
        const size_t KI_HEAD = offsetof(V, mPlayerCarContactPosition);
        // The head->tail bridge is the ONE delta that is not ABI-invariant: on X360 it is
        // 480 - 476 == 4, but on x64 the widened base vptr + mpVehicleManager push the three
        // gates to a different residue and the 16-aligned Vector3 lands 16 bytes later. What IS
        // invariant -- and what the X360 +480 attests -- is that the contact readout opens a
        // fresh 16-byte lane immediately after the three gate bytes.
        static_assert(KI_HEAD % 16 == 0,
                      "mPlayerCarContactPosition opens a 16-byte lane (X360 +480)");
        static_assert(KI_HEAD - offsetof(V, mbGrindDebugRenderEnabled) <= 16,
                      "nothing sits between the third gate byte and the contact readout");

        static_assert(offsetof(V, mOtherCarContactPosition)       - KI_HEAD ==  16,  "X360 +496");
        static_assert(offsetof(V, mbDisplayContact)               - KI_HEAD ==  32,  "X360 +512");
        static_assert(offsetof(V, mfOtherCarContactAngleRad)      - KI_HEAD ==  36,  "X360 +516");
        static_assert(offsetof(V, mClosingVelocityPlayerCarSpace) - KI_HEAD ==  48,  "X360 +528");
        static_assert(offsetof(V, mClosingVelocityOtherCarSpace)  - KI_HEAD ==  64,  "X360 +544");
        static_assert(offsetof(V, mPlayerCarVelocity)             - KI_HEAD ==  80,  "X360 +560");
        static_assert(offsetof(V, mOtherCarVelocity)              - KI_HEAD ==  96,  "X360 +576");

        static_assert(offsetof(V, mbDrawLastRaceCarTrafficContact)  - KI_HEAD == 112, "X360 +592 (asm stb 0x250)");
        static_assert(offsetof(V, mbDrawLastSlamInfo)               - KI_HEAD == 113, "X360 +593 (asm stb 0x251)");
        static_assert(offsetof(V, mbDrawLastShuntInfo)              - KI_HEAD == 114, "X360 +594 (asm stb 0x252)");
        static_assert(offsetof(V, mbDrawLastCrashContact)           - KI_HEAD == 115, "X360 +595 (asm stb 0x253)");
        static_assert(offsetof(V, mbDrawCatchupTargets)             - KI_HEAD == 116, "X360 +596 (asm stb 0x254)");
        static_assert(offsetof(V, mbRenderWallContacts)             - KI_HEAD == 117, "X360 +597 (asm stb 0x255)");
        static_assert(offsetof(V, mbRenderGroundContacts)           - KI_HEAD == 118, "X360 +598 (asm stb 0x256)");
        static_assert(offsetof(V, mbDrawPlayerStuckInCollisionTests)- KI_HEAD == 119, "X360 +599 (asm stb 0x257)");

        static_assert(offsetof(V, mLastTrafficContact_RaceCarTransform) - KI_HEAD == 128, "X360 +608 (asm r9  = this+0x260)");
        static_assert(offsetof(V, mLastTrafficContact_TrafficTransform) - KI_HEAD == 192, "X360 +672 (asm r8  = this+0x2A0)");
        static_assert(offsetof(V, mLastTrafficContact_RaceCarHalfExt)   - KI_HEAD == 256, "X360 +736 (asm r26 = 0x2E0)");
        static_assert(offsetof(V, mLastTrafficContact_TrafficHalfExt)   - KI_HEAD == 272, "X360 +752 (asm r26 = 0x2F0)");
        static_assert(offsetof(V, mLastTrafficContact_RaceCarVelocity)  - KI_HEAD == 288, "X360 +768 (not written by Construct)");
        static_assert(offsetof(V, mLastTrafficContact_TrafficVelocity)  - KI_HEAD == 304, "X360 +784 (not written by Construct)");
        static_assert(offsetof(V, mLastTrafficContact_Contact)          - KI_HEAD == 320, "X360 +800 (asm r10 = this+0x320)");

        // Two independent witnesses that the embedded PotentialContact is 80 bytes: 800 -> 880 and
        // 976 -> 1056. That is the committed 76-byte record padded to its alignas(16).
        static_assert(sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) == 80,
                      "PotentialContact: 76 bytes at align 16");
        static_assert(offsetof(V, mLastCrashContact_RaceCarTransform) - offsetof(V, mLastTrafficContact_Contact) == 80,
                      "800 + sizeof(PotentialContact) == 880");
        static_assert(offsetof(V, meLastSlamImpactSituation)          - offsetof(V, mLastCrashContact_Contact)  == 80,
                      "976 + sizeof(PotentialContact) == 1056");

        static_assert(offsetof(V, mLastCrashContact_RaceCarTransform) - KI_HEAD == 400, "X360 +880 (asm r10 = this+0x370)");
        static_assert(offsetof(V, mLastCrashContact_RaceCarHalfExt)   - KI_HEAD == 464, "X360 +944 (asm r28 = 0x3B0)");
        static_assert(offsetof(V, mLastCrashContact_RaceCarVelocity)  - KI_HEAD == 480, "X360 +960 (asm r27 = 0x3C0)");
        static_assert(offsetof(V, mLastCrashContact_Contact)          - KI_HEAD == 496, "X360 +976 (asm r10 = this+0x3D0)");

        static_assert(offsetof(V, meLastSlamImpactSituation)  - KI_HEAD == 576, "X360 +1056 (asm stw -1, 0x420)");
        static_assert(offsetof(V, miLastSlamNumber)           - KI_HEAD == 580, "X360 +1060 (asm stw 0,  0x424)");
        static_assert(offsetof(V, mfLastSlamDuration)         - KI_HEAD == 584, "X360 +1064 (asm stfs 0, 0x428)");
        static_assert(offsetof(V, mfLastSlamBaseDuration)     - KI_HEAD == 588, "X360 +1068 (asm stfs 0, 0x42C)");
        static_assert(offsetof(V, mfLastSlamMassFactor)       - KI_HEAD == 592, "X360 +1072 (asm stfs 0, 0x430)");
        static_assert(offsetof(V, meLastShuntImpactSituation) - KI_HEAD == 596, "X360 +1076 (asm stw -1, 0x434)");
        static_assert(offsetof(V, mfLastShuntMagnitude)       - KI_HEAD == 600, "X360 +1080 (asm stfs 0, 0x438)");
        static_assert(offsetof(V, mfLastShuntClosingSpeed)    - KI_HEAD == 604, "X360 +1084 (asm stfs 0, 0x43C)");

        // The tail loop's three bases are exactly 64 bytes apart -- that IS the loop's [r9-64] /
        // [r9] / [r9+64] addressing, so these three pins are the loop transcription's own proof.
        static_assert(offsetof(V, maStuckInCollisionLineTestStart) - KI_HEAD == 608, "X360 +1088 == r9 - 64");
        static_assert(offsetof(V, maStuckInCollisionLineTestEnd)   - KI_HEAD == 672, "X360 +1152 == r9");
        static_assert(offsetof(V, maStuckInCollisionLineTestPoint) - KI_HEAD == 736, "X360 +1216 == r9 + 64");
        static_assert(offsetof(V, mabStuckInCollisionIntersection) - KI_HEAD == 800, "X360 +1280 == r27");

        // sizeof() ALONE CANNOT PIN THE LAST MEMBER. Growing mabStuckInCollisionIntersection
        // from 4 to 8 bytes leaves sizeof(V) at 1296 (the 16-byte tail padding absorbs it), and a
        // tamper test caught exactly that: the sizeof assert below MISSED the wrong array count.
        // The end-of-DATA pin is what catches it -- 1284 - 480 == 804, the loop's own trip count.
        static_assert(sizeof(V::mabStuckInCollisionIntersection) == KI_MAX_WORLD_CONTACT_SPIES,
                      "one intersection flag per line test (the tail loop's stbx count)");
        static_assert(offsetof(V, mabStuckInCollisionIntersection)
                          + sizeof(V::mabStuckInCollisionIntersection) - KI_HEAD == 804,
                      "the class's last DATA byte is X360 +1284");

        // The whole tail is 816 bytes (1296 - 480), i.e. the class ends exactly where
        // BrnVehicleManager.h's `mDebugComponent[163264 - 161968]` span ends.
        static_assert(sizeof(V) - offsetof(V, mPlayerCarContactPosition) == 816,
                      "1296 - 480 == 816: the class closes on the 1296-byte VehicleManager span");
    }
}
}
