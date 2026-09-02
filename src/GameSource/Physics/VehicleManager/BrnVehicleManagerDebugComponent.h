#pragma once

// ============================================================================================
// BrnPhysics::Vehicle::VehicleManagerDebugComponent - the VehicleManager's own in-game debug
// menu/overlay ("Vehicle Manager"). It records the last race-car-vs-traffic contact, the last
// crash contact, the last slam/shunt classification, the world contact spies and the
// stuck-in-collision line tests, and renders them as 3D/HUD overlays.
//
// Layout/shape authority: the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h)
// gives the COMPLETE member sequence; every byte offset below is asm-literal out of
// VehicleManagerDebugComponent::Construct @ 0x825B5A78 (194 instructions).
//
// THE X360 LAYOUT CLOSES TO THE BYTE, three independent ways:
//   1. sizeof(CgsDev::DebugComponent) == 12 on X360 (vptr 4 + mbActive 1 + pad 3 +
//      mpDebugLinkedListNext 4) and OutContactSpy is alignas(16), so maWorldContactSpies starts
//      at +16.  16 + 4*112 == 464, which is exactly where the asm's next named member
//      (mbDisplayWorldContactSpies at +468, one word past miNumWorldContactSpies) sits. The 112
//      stride is independently X360-attested from BaseEventQueue<OutContactSpy>::AddEvent
//      @0x825E44C8, so the base size and the spy stride confirm each other.
//   2. The two embedded PotentialContacts land at +800 and +976 and the members that follow them
//      are at +880 and +1056 -- two witnesses that sizeof(PotentialContact) == 80 (the committed
//      76 bytes padded to its alignas(16)).
//   3. The tail closes at 1284 -> 1296 with alignment, and 1296 is EXACTLY the
//      `mDebugComponent[163264 - 161968]` span BrnVehicleManager.h pinned from the other side
//      (VehicleManagerDebugComponent::Construct(this + 161968, this) followed by
//      maRaceCarDebugComponent at +163264).
//
// THE X360 OFFSETS BELOW ARE NOT THE HOST OFFSETS. Two members widen on x64 -- the vtable
// pointer inside the base and `mpVehicleManager` -- so everything after them shifts. Per the
// project's x64 rule (parity by NAMED MEMBERS, not byte offsets) the _AssertLayout() pins below
// are written as RELATIVE deltas, which are X360-identical for every member from
// mPlayerCarContactPosition onwards (that whole tail contains no pointers).
//
// This banner used to end: "VehicleManager keeps its own
// `mDebugComponent` as the X360-sized 1296-byte opaque span so its byte-pinned offsetof chain stays
// intact -- exactly as it already does for mStuntOffencesManager and the contained
// PhysicalTrafficManager." Every clause of that is now out of date:
//   * VehicleManager embeds this class BY VALUE at +161968 (host 1328 vs the X360's 1296) and
//     carries the 32-byte difference as KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, so its offsetof chain
//     stays intact WITH the real type rather than instead of it;
//   * mStuntOffencesManager was never an opaque span in the sense meant -- its real class fits the
//     X360 span exactly;
//   * PhysicalTrafficManager is embedded by name too, and the 103360-byte "span" it was being kept
//     for was 2288 bytes short of that class's real X360 size.
// The layout below did not change; only the reason it is here did. `VehicleManagerDebugComponent::
// Construct(this + 161968, this)` is now callable as `mDebugComponent.Construct(this)`.
//
// INCREMENTAL: this slice lands the full DATA layout plus Construct + GetName. The ~20
// render/record members declared by the DWARF (RenderWorld, RenderHUD, RecordSlam,
// RenderCarDiagram, ...) belong to the dev-UI render pass and are deliberately NOT declared here
// -- declaring a virtual override without a body would put an unresolvable entry in this class's
// vtable.
// ============================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                  // Vector3, Matrix44Affine
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"    // CgsPhysics::PhysicsSimulationIO::OutContactSpy
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"           // EImpactSituation

namespace BrnPhysics
{
namespace Vehicle
{
    // The owning manager. Pointer-only collaborator: Construct only stores it. Declared with the
    // SAME class-key BrnVehicleManager.h uses (`class`) -- a class-key fork changes the MSVC
    // mangling of anything templated on the name.
    class VehicleManager;

    class VehicleManagerDebugComponent : public CgsDev::DebugComponent
    {
        // 2026-08-24 (physics mount wave B3b): VehicleManager::CalculateSlamData @0x825C782C and
        // CalculateShuntData @0x825C7B78 stamp the last-slam/last-shunt display fields below by
        // raw offset (this+161968+0x420..0x43C) -- friend/same-TU context on the console. The
        // friend grant is the write seam; nothing else about the class changes.
        friend class VehicleManager;

    public:
        // ------------------------------------------------------------------------------------
        // @0x825B5A78 (DWARF BrnVehicleManagerDebugComponent.cpp:87). Bind the component to its
        // manager and reset every recorded contact/slam/shunt/line-test slot.
        // ------------------------------------------------------------------------------------
        void Construct(VehicleManager* lpVehicleManager);

        // ------------------------------------------------------------------------------------
        // @0x825B7880 (114 insns; DWARF BrnVehicleManagerDebugComponent.cpp:1045..1065 by its
        // assert lines). AN EXPORT HOLE: no .ida-exports JSON exists for it and identity.json has
        // no row -- decoded from the raw image words 0x825B7880..0x825B7A44 (tools/re/x360rd.py).
        // Called by VehicleManager::HandleRaceCarWorldPotentialContact @0x8263EC3C on every
        // committed world crash. Snapshots the PLAYER car's transform / deformable half-extents /
        // linear velocity and the 80-byte contact into the mLastCrashContact_* slots below --
        // only for the player's slot, and only while that car is not already crashing.
        // ------------------------------------------------------------------------------------
        void RecordCrashContact(const CgsSceneManager::SceneManagerIO::PotentialContact* lpPotentialContact);

        // read access to the two contact-classification render
        // gates ValidateRaceCarWorldContact checks (the console reads the component's bytes
        // +597/+598 directly from inside the VehicleManager method; the host goes through these
        // ADDITIVE inline getters instead of a friend grant). Both stay false on this build --
        // nothing toggles the dev menu.
        bool RenderWallContacts() const   { return mbRenderWallContacts; }    // +597 :194
        bool RenderGroundContacts() const { return mbRenderGroundContacts; }  // +598 :195

    protected:
        // @0x825B5D80: the debug-menu display name.
        //   lis r11,aVehicleManager@ha ; addi r3,r11,aVehicleManager@l "Vehicle Manager" ; blr
        const char* GetName() const override;

    private:
        // Never called -- exists only so offsetof() can see the private members below (offsetof on
        // a private member needs member-function context). The gate FAILS if any pin moves.
        static void _AssertLayout();

        // DWARF :169 / :171. Both are 4 in the console build.
        static const s32 KI_CONTACT_DISPLAY_SECONDS  = 4;
        static const s32 KI_MAX_WORLD_CONTACT_SPIES  = 4;

        // ---- world contact spies (DWARF :173-175) -----------------------------------------
        // X360 +16 .. +464. RecordWorldContactSpy() appends here; RenderWorldContactSpies()
        // draws them. Construct does NOT touch miNumWorldContactSpies -- only the display flag.
        CgsPhysics::PhysicsSimulationIO::OutContactSpy
                       maWorldContactSpies[KI_MAX_WORLD_CONTACT_SPIES];  // :173  X360 +16   (4 * 112)
        s32            miNumWorldContactSpies;                           // :174  X360 +464  (NOT written by Construct)
        bool           mbDisplayWorldContactSpies;                       // :175  X360 +468  (stb 0, 0x1D4)

        // ---- owner + the three render gates written first (DWARF :177-180) ------------------
        VehicleManager* mpVehicleManager;                                // :177  X360 +472  (stw r29, 0x1D8)
        bool           mbSlamDebugRenderEnabled;                         // :178  X360 +476  (stb 0, 0x1DC)
        bool           mbRenderResetLines;                               // :179  X360 +477  (stb 0, 0x1DD)
        bool           mbGrindDebugRenderEnabled;                        // :180  X360 +478  (stb 0, 0x1DE)

        // ---- the race-car-vs-race-car contact readout (DWARF :181-188) ----------------------
        // SetContactDisplay() fills these; RenderContact()/RenderCarDiagram() draw them.
        // NONE of them is written by Construct.
        Vector3        mPlayerCarContactPosition;                        // :181  X360 +480
        Vector3        mOtherCarContactPosition;                         // :182  X360 +496
        bool           mbDisplayContact;                                 // :183  X360 +512
        f32            mfOtherCarContactAngleRad;                        // :184  X360 +516
        Vector3        mClosingVelocityPlayerCarSpace;                   // :185  X360 +528
        Vector3        mClosingVelocityOtherCarSpace;                    // :186  X360 +544
        Vector3        mPlayerCarVelocity;                               // :187  X360 +560
        Vector3        mOtherCarVelocity;                                // :188  X360 +576

        // ---- the eight render gates Construct clears back-to-back (DWARF :189-196) ----------
        // asm 0x825B5B80..0x825B5D5C: stb 0 at 0x250..0x257, eight consecutive bytes.
        bool           mbDrawLastRaceCarTrafficContact;                  // :189  X360 +592
        bool           mbDrawLastSlamInfo;                               // :190  X360 +593
        bool           mbDrawLastShuntInfo;                              // :191  X360 +594
        bool           mbDrawLastCrashContact;                           // :192  X360 +595
        bool           mbDrawCatchupTargets;                             // :193  X360 +596
        bool           mbRenderWallContacts;                             // :194  X360 +597
        bool           mbRenderGroundContacts;                           // :195  X360 +598
        bool           mbDrawPlayerStuckInCollisionTests;                // :196  X360 +599

        // ---- last race-car-vs-traffic contact (DWARF :199-205) ------------------------------
        Matrix44Affine mLastTrafficContact_RaceCarTransform;             // :199  X360 +608  SetIdentity
        Matrix44Affine mLastTrafficContact_TrafficTransform;             // :200  X360 +672  SetIdentity
        Vector3        mLastTrafficContact_RaceCarHalfExt;               // :201  X360 +736  zeroed
        Vector3        mLastTrafficContact_TrafficHalfExt;               // :202  X360 +752  zeroed
        // Construct does NOT zero these two, even though it zeroes the two half-extents above
        // and both of the crash-contact vectors. Transcribed as-is; do not "fix" it.
        Vector3        mLastTrafficContact_RaceCarVelocity;              // :203  X360 +768  (NOT written)
        Vector3        mLastTrafficContact_TrafficVelocity;              // :204  X360 +784  (NOT written)
        CgsSceneManager::SceneManagerIO::PotentialContact
                       mLastTrafficContact_Contact;                      // :205  X360 +800  (80 bytes)

        // ---- last crash contact (DWARF :208-211) --------------------------------------------
        Matrix44Affine mLastCrashContact_RaceCarTransform;               // :208  X360 +880  SetIdentity
        Vector3        mLastCrashContact_RaceCarHalfExt;                 // :209  X360 +944  zeroed
        Vector3        mLastCrashContact_RaceCarVelocity;                // :210  X360 +960  zeroed
        CgsSceneManager::SceneManagerIO::PotentialContact
                       mLastCrashContact_Contact;                        // :211  X360 +976  (80 bytes)

        // ---- last slam (DWARF :213-217) ------------------------------------------------------
        EImpactSituation meLastSlamImpactSituation;                      // :213  X360 +1056 (stw -1)
        s32            miLastSlamNumber;                                 // :214  X360 +1060
        f32            mfLastSlamDuration;                               // :215  X360 +1064
        f32            mfLastSlamBaseDuration;                           // :216  X360 +1068
        f32            mfLastSlamMassFactor;                             // :217  X360 +1072

        // ---- last shunt (DWARF :219-221) -----------------------------------------------------
        EImpactSituation meLastShuntImpactSituation;                     // :219  X360 +1076 (stw -1)
        f32            mfLastShuntMagnitude;                             // :220  X360 +1080
        f32            mfLastShuntClosingSpeed;                          // :221  X360 +1084

        // ---- the player stuck-in-collision line tests (DWARF :223-226) -----------------------
        // Construct clears all four elements of all four arrays in one 4-iteration loop.
        Vector3        maStuckInCollisionLineTestStart[KI_MAX_WORLD_CONTACT_SPIES]; // :223 X360 +1088
        Vector3        maStuckInCollisionLineTestEnd[KI_MAX_WORLD_CONTACT_SPIES];   // :224 X360 +1152
        Vector3        maStuckInCollisionLineTestPoint[KI_MAX_WORLD_CONTACT_SPIES]; // :225 X360 +1216
        bool           mabStuckInCollisionIntersection[KI_MAX_WORLD_CONTACT_SPIES]; // :226 X360 +1280
    };
}
}
