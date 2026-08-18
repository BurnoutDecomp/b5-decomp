// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_07.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q, ROUND 2, group 07, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// GROUP 07 WAS THREE FUNCTIONS. ONE LANDS HERE; TWO ARE PARKED, each on declarations that do not
// exist in the tree today (verified by grep and by a negative compile probe, not by repeating a
// spec):
//
//   * PropManager::FindPartIndex               @0x82605E10  -- BODIED BELOW (205 insns).
//   * PropManager::HandleContactWithLeanProp   @0x8260FB60  -- PARKED,
//         scratchpad/waveQ2/parked/PropManager_07_HandleContactWithLeanProp.cpp
//   * PropManager::HandleContactWithTiltProp   @0x826108B8  -- PARKED,
//         scratchpad/waveQ2/parked/PropManager_07_HandleContactWithTiltProp.cpp
//     Both parked files carry COMPLETE bodies plus a banner naming every missing declaration and
//     the evidence for it. THE FULL BLOCKER LIST -- eight declarations, each PROVEN absent by the
//     two negative compile probes scratchpad/waveQ2/probe_wq2_07/probe_api.cpp and probe_api2.cpp
//     (not by repeating a spec, and not by grep alone):
//       game headers (2)   ExternalPhysicsBody::GetLinearMomentum(VecFloat) const   [DWARF
//                            ExternalPhysicsBody.h:262 -- unavoidable: the console inlines
//                            `v*m + F*dt + J` and all three members are `protected`]
//                          Vehicle::Wheel::GetRoadLongSpeed() const
//                            [== mSpeedAndMassOnWheelVariables.x; the asm reads lpRaceCar+0x360
//                             == maWheels[eRearLeftWheel] + 0x70]
//       vendor rw-math (6) rw::math::vpu::Matrix44AffineFromAxisRotationAngle(Vector3, VecFloat)
//                            [the tree has only MakeRotationX/Y/Z; the ARTIST bodies inline this
//                             as a ~130-instruction 2pi-reduction + sin/cos minimax + Rodrigues
//                             block at 0x82610140..0x82610370]
//                          GetVector3_YAxis() / GetVector3_ZAxis()
//                          CompLessThan / CompGreaterThan  (VecFloat and Vector3 forms)
//                          struct Mask3  + its three lane broadcasts
//                          Select(Vector3, Vector3, MaskScalar)
//     The vendor six sit in b5-decomp/vendor/renderware/include/rw/math/vpu/, which this wave may
//     not edit -- so these are a conductor request, not something a lander can route around.
//
//     ⚠️ THE ROUND-1/EARLIER FRAMING OF THIS PARK WAS INCOMPLETE and is corrected here: it named
//     three blockers; there are eight. It also implied the axis-angle builder was the whole story.
//     Separately, and contrary to what a "rodata-blocked" reading would suggest, the FOUR file-scope
//     VecFloat constants both bodies need are NOT blocked -- they are zero in the image and written
//     by CRT-init thunks at 0x82C5E910..0x82C5E9AC, and a headless IDA read recovers them:
//     KVF_ROTATION_FACTOR 0.07f / KVF_MAX_ROTATION 0.05f / KVF_PENETRATION_RESOLUTION_FACTOR 0.5f /
//     KVF_MOMENTUM_RESOLUTION_FACTOR 0.5f (names from the DecFIGS file-scope list,
//     BrnPropManager.cpp:1755-1758). Details in the Lean parked file's banner, section C.
//
// ⚠️ LINK, for the conductor (gotcha 12) -- ONE hole, not two (round-2 MUST_FIX; re-grepped
//    2026-08-18 across b5-decomp/src AND b5-decomp/vendor):
//      * BrnPhysics::ExternallySimulatedBody::Translate(Vector3) -- DECLARED at
//        GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h:105, NO definition anywhere.
//        THAT is the parked bodies' link hole.
//      * ✅ BrnPhysics::ExternalPhysicsBody::GetLocalVelocity IS BODIED -- a real, complete body at
//        GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp:839 (the WORLD_SPACE /
//        BODY_SPACE branch over mLinearVelocity + mAngularVelocity x r), not a stub. It is NOT a
//        link hole; do NOT write a second definition for it (an LNK2005 `cl /c` cannot see). The
//        earlier banner named both, and the parked files' own banners carry the same wrong claim.
//    `cl /c` is green on the parks; a link is not. FindPartIndex below has no such callee.
//
// ⚠️ ODR / LINK NOTE FOR THE CONDUCTOR. Grepped before writing: FindPartIndex has NO other
//    definition anywhere in b5-decomp/src or b5-decomp/vendor -- not a real body, not a trap stub,
//    and (unlike UpdateTriangleCache / OutputUpdatedProps / ProcessInputsPreScene) NOT a one-shot
//    body in GameSource/Physics/BrnPhysicsConductorGates.cpp or World/WorldLinkStubs.cpp. So this
//    partfile introduces no LNK2005 and retires no gate.
//
// Grounding: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82605E10.json -- the RAW `assembly` array. The
// Hex-Rays pseudocode in the same export was used only to cross-read the stack-slot map. Nothing
// below is invented; every claim in a comment is either an asm line quoted verbatim from that
// export or is labelled INFERENCE.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"  // PropPartInstance::GetEntityId
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                     // PropEntityID::AssertIsProp

namespace BrnPhysics
{
namespace Props
{

// FindPartIndex @0x82605E10 (const, 205 instructions -- counted as
// (0x82606140 - 0x82605E10)/4 + 1 == 205, which matches the owner report's table).
//
// The part-instance twin of the already-committed FindPropIndex
// (BrnPropManager_PropInstanceQueries.cpp): walk the used-PART bit-set, owner-check the query id
// and each stored id, and return the first slot whose stored PropEntityID equals lEntityId.
// Deliberately spelled exactly like that sibling -- same loop shape, same by-name calls -- because
// the two X360 bodies are the same source with mUsedProps/mpaPropInstances swapped for
// mUsedParts/mpaPartInstances.
//
// WHAT THE ASM ACTUALLY CONTAINS, and how each piece maps to a by-name call here (MEASURED):
//
//   * `addi r19, r31, 0x90` @0x82605E24 -- the walked bit-set is the object's SECOND BitArray,
//     mUsedParts (console +0x90), not mUsedProps (+0x80). Capacity 30: every bound in the body is
//     the literal 0x1E, but each site is quoted with ITS OWN verbatim text (round-2 NIT -- one
//     instruction text used to be quoted for two different addresses):
//     `cmpwi cr6, r11, 0x1E` @0x82605E88 (SIGNED, on the freshly computed first-bit index);
//     `cmplwi cr6, r29, 0x1E` @0x826060B4 (UNSIGNED, on the scan cursor r29);
//     `li r5, 0x1E` @0x82606030 and @0x82606054 inside the assert text.
//   * 0x82605E3C..0x82605E88 is CgsContainers::BitArray<30>::GetFirstNonZeroBit inlined: scan the
//     64-bit fields for a non-zero one (`ld r9,0(r10)` / `cmpldi` / `addi r10,r10,8`, bounded by
//     `cmplwi cr6, r11, 1` -- kuNumberOfBitFields == 1 for 30 bits), then isolate the lowest set
//     bit with the field*64 - cntlzd(x & -x) + 63 idiom, then the `>= 0x1E` guard.
//   * 0x82605E94 `cmpwi cr6, r11, -1 ; beq` is the loop's `!= KI_INVALID_BITINDEX` exit.
//   * 0x82605F54..0x82606120 is GetNextNonZeroBit inlined. The console spelling is the two-phase
//     one (scan bit-by-bit to the end of the current 64-bit field via `clrrwi r11,r30,6 ;
//     addi r28,r11,0x40` capped at 30, then fall through to the cross-field field-scan at
//     0x826060B4). The header's GetNextNonZeroBit is the single `for (bit = after+1; bit < N; ++bit)`
//     loop -- VALUE-IDENTICAL for a single-field array, which is what BitArray<30> is, so the
//     by-name call is used here exactly as the committed FindPropIndex uses it.
//   * The two `srwi r11, <id>, 24 ; cmplwi cr6, r11, 3 ; beq` guards (@0x82605EF4 on the QUERY id
//     reloaded from arg_1C, @0x82605F28 on the STORED id) are PropEntityID's baked owner tripwire
//     "mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278 (`li r5, 0x116`) -> the
//     two AssertIsProp() calls below, in the console's own order (query first, stored second).
//   * `lwz r10, 0x8C(r31)` @0x82605F18 + `slwi r11, r30, 6` + `lwz r31, 0x30(r11)` -- the stored id
//     is mpaPartInstances (console +0x8C) indexed at STRIDE 64 with mEntityId at +0x30, i.e.
//     PropPartInstance::GetEntityId(). Console offsets are quoted in this comment only; the code
//     uses the members by name (host layout differs -- pointers widen on LLP64).
//   * `li r3, -1` at 0x82605E58 / 0x826060EC / 0x8260612C is the not-found return; the header
//     names that value KI_PROP_INDEX_NOT_FOUND (== -1, DWARF BrnPropManager.h:245), so it is
//     spelled by name here.
//
// WHAT IS DELIBERATELY NOT REPRODUCED, and why (MEASURED, then INFERENCE):
//   The block at 0x82605F7C..0x82606074 is a CgsBitArray.h:203 ("invalid index : <i> < <30>",
//   `li r5, 0xCB`) bounds assert, emitted inside the inlined GetNextNonZeroBit's IsBitSet. MEASURED:
//   it is guarded by `cmplwi cr6, r29, 0x1E ; blt cr6, loc_82606078` @0x82605F74 -- i.e. it only
//   fires when the scan index has already reached 30. INFERENCE: it is unreachable in the console
//   too, because the enclosing scan's own bound r28 is `min(clrrwi(r30,6) + 64, 30)`
//   (0x82605F54..0x82605F64), so r29 can never reach 0x1E inside that loop. The committed sibling
//   FindPropIndex makes the same omission for the same reason. No control flow is added or
//   inverted relative to the asm.
s32 PropManager::FindPartIndex( PropEntityID lEntityId ) const
{
    for ( s32 liPartIndex = mUsedParts.GetFirstNonZeroBit();
          liPartIndex != CgsContainers::BitArray<30>::KI_INVALID_BITINDEX;
          liPartIndex = mUsedParts.GetNextNonZeroBit( liPartIndex ) )
    {
        lEntityId.AssertIsProp();
        PropEntityID lStoredEntityId = mpaPartInstances[liPartIndex].GetEntityId();
        lStoredEntityId.AssertIsProp();

        if ( lStoredEntityId == lEntityId )
        {
            return liPartIndex;
        }
    }

    return KI_PROP_INDEX_NOT_FOUND;
}

}
}
