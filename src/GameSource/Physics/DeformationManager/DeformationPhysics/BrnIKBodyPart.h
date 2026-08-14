#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, VecFloat, Matrix44Affine
#include "SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h"                              // IKBodyPartSpec, DeformationJointSpec (fwd)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"        // IKDrivenPoint
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"             // TagPoint
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"              // EBodyParts (committed home)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"                    // CgsGeometric::AxisAlignedBox

// BrnPhysics::Deformation::IKBodyPart — the SKINNING + DETACHMENT side of one deformable
// vehicle panel. It owns (by pointer-into-the-owning-DeformableObject's-pools) the array of
// IKDrivenPoints and TagPoints that drive the panel's rendered mesh, points at its streamed
// IKBodyPartSpec, tracks which deformation joint is currently active (mu8ActiveJointIndex),
// pushes accumulated per-point deformation into the skin (UpdateSkinningOffsets[WithinBox]),
// and decides when the panel detaches (CheckForDetachment / CheckForJointDetachment /
// CheckSensorForcesForJointDetachment).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (offset authority) with member names/types and
// the full method list from the DecFIGS DWARF (BrnIKBodyPart.h:51).
//
// Layout (DWARF BrnIKBodyPart.h:202-206). The console pointer offsets the X360 asm uses are
// noted per member for provenance; they are 32-bit-pointer offsets that do NOT (and need not)
// reproduce byte-for-byte on the 64-bit host — every access is BY MEMBER NAME, so the host
// recomputes the addresses and semantic parity is exact:
//
//   +0x00 IKDrivenPoint*          maDrivenPoints       (this part's driven-point window; asm *(this+0))
//   +0x04 TagPoint*               maTagPoints          (this part's tag-point window;    asm *(this+4))
//   +0x08 const IKBodyPartSpec*   mpSpec               (the streamed spec;               asm *(this+8))
//   +0x0C int16_t                 miPartPoolIndex      (this part's slot in the IK-part pool)
//   +0x0E uint8_t                 mu8ActiveJointIndex  (active deformation joint, or KU8_NO_ACTIVE_JOINT)
//
// NOTE on the asm's `*(this+6)` / `*(this+14)`: those are WORD indices in the Hex-Rays view,
// i.e. byte +0x18 / +0x38 in the 32-bit struct — they target miPartPoolIndex (set to -1 in
// Construct) and mu8ActiveJointIndex (set to -1/0xFF in Construct, written by SetActiveJointIndex).
// GROW this header for further IKBodyPart TUs — do not fork it.

namespace BrnPhysics
{
namespace Deformation
{
    // ------------------------------------------------------------------------------------
    // Detachment impulse tuning (DWARF BrnIKBodyPart.h:370-372). Namespace-scope VecFloat
    // thresholds the detachment logic compares accumulated sensor impulses against. DECLARED
    // extern here (values live in BrnIKBodyPart.cpp / a deformation-constants TU) — do NOT
    // define values in this header.
    // ------------------------------------------------------------------------------------
    extern VecFloat KVF_MIN_IMPULSE_FOR_DETACHMENT;        // BrnIKBodyPart.h:370
    extern VecFloat KVF_MAX_IMPULSE_FOR_DETACHMENT;        // BrnIKBodyPart.h:371
    extern VecFloat KVF_MAX_IMPULSE_FOR_DETACHMENT_TOUGH;  // BrnIKBodyPart.h:372 (toughened parts)

    struct IKBodyPart
    {
    public:
        // ---- detachment / active-joint sentinels (DWARF BrnIKBodyPart.h:55/56/59) --------
        // KI_DETACH_WITHOUT_JOINT / KI_DONT_DETACH are the two non-index results
        // CheckForDetachment can hand back through its `int32_t&` out-param: detach the whole
        // panel (no joint), or do not detach. The DWARF prints them as the unsigned bit
        // patterns 0xFFFFFFFF / 0xFFFFFFFE (i.e. -1 / -2 as int32_t). KU8_NO_ACTIVE_JOINT is
        // the mu8ActiveJointIndex "none set yet" sentinel.
        static const s32 KI_DETACH_WITHOUT_JOINT = -1;     // 0xFFFFFFFF  (DWARF :55)
        static const s32 KI_DONT_DETACH          = -2;     // 0xFFFFFFFE  (DWARF :56)
        static const u8  KU8_NO_ACTIVE_JOINT     = 255u;   // 0xFF        (DWARF :59)

        // ---- lifecycle (DWARF :65-74) ----------------------------------------------------

        // X360 0x825B3EE8. Wire this part to its streamed spec and the owning DeformableObject's
        // pools: maDrivenPoints = lpDrivenPointArrayBase + spec->GetStartIndexOfDrivenPoints(),
        // maTagPoints = lpTagPointArrayBase + spec->GetStartIndexOfTagPoints(), cache mpSpec, and
        // seed miPartPoolIndex / mu8ActiveJointIndex to their -1 "unset" sentinels. The body
        // range-asserts the spec's pool windows against KI_MAX_NUM_*_PER_DEFORMABLE_OBJECT.
        void Construct(const IKBodyPartSpec* lpSpec, IKDrivenPoint* lpDrivenPointArrayBase,
                       TagPoint* lpTagPointArrayBase);
        void Destruct();
        bool Prepare();
        bool Release();

        // ---- driven-point / tag-point access (DWARF :77-92) ------------------------------
        IKDrivenPoint*       GetDrivenPoint(s32 liIndex);
        const IKDrivenPoint* GetDrivenPoint(s32 liIndex) const;
        // ⭐ HEADER-INLINE (2026-08-14, deformation-mount wave): no export on either console
        // (console-inline; the callers read spec+464 directly). Forwards to mpSpec.
        s32                  GetNumberOfDrivenPoints() const { return mpSpec->GetNumberOfDrivenPoints(); }   // console spec+464
        TagPoint*            GetTagPoint(s32 liIndex);
        const TagPoint*      GetTagPoint(s32 liIndex) const;
        s32                  GetNumberOfTagPoints() const;      // forwards to mpSpec (console spec+472)

        // ---- per-frame / skinning (DWARF :95-99) -----------------------------------------

        // X360 0x825E76F0. Re-solve every owned IKDrivenPoint (loops GetNumberOfDrivenPoints,
        // calls IKDrivenPoint::Update on each), pulling the IK constraints up to date for the
        // current tag-point positions.
        void Update();

        // X360 0x825C11F0. Write each driven point's accumulated deformation offset into the
        // caller's skinning-offset buffer (lpaSkinningOffsets, one Vector3Plus per driven point):
        // offset = driven-point current pos - original pos, preserving the w lane.
        void UpdateSkinningOffsets(Vector3Plus* lpaSkinningOffsets);

        // ---- detachment (DWARF :104-108, :191) -------------------------------------------

        // Decide whether/how this panel detaches given an accumulated impulse. Returns true if a
        // detachment is warranted and writes the joint index (or KI_DETACH_WITHOUT_JOINT /
        // KI_DONT_DETACH) into lriDetachJointIndexOut. Consults IsToughenedPart to pick the
        // tough vs normal max-impulse threshold.
        bool CheckForDetachment(f32 lfImpulse, s32& lriDetachJointIndexOut) const;

        // True if the accumulated impulse on the active joint exceeds its breaking threshold.
        bool CheckForJointDetachment(f32 lfImpulse) const;

        // X360 0x825C17F8. Walk this part's tag points, max-reduce their two stored sensor
        // impulses (per-tag SIMD max), then compare the panel's peak impulse against the
        // part-type-dependent detach band (toughened parts use the higher KVF_*_TOUGH band; a
        // tough part early-outs to "no detach" when lbIsToughCheck and part-type is exhaust-ish).
        // Returns true iff the joint should break. lbIsToughCheck selects the tough-part early-out.
        bool CheckSensorForcesForJointDetachment(bool lbIsToughCheck) const;

        // ---- classification / queries (DWARF :111-114, :120, :162-170) -------------------
        // ⭐ INLINE 2026-08-06 (bridge de-facade wave): no out-of-line emission -- the
        // detached-part creator @0x825DD7A8/0x825DD7AC chains part->mpIKPart (+8) -> spec
        // (+476) directly. Forwards to the spec's own committed inline.
        EBodyParts GetPartType() const { return mpSpec->GetPartType(); }   // console spec+476
        s32        GetMeshId() const;     // forwards to mpSpec
        void       Reset();

        // X360 0x825C13F8. True if this panel is currently detachable: scans its tag points and
        // returns true as soon as one tag point's joint-detach threshold is below the "never
        // detaches" sentinel (9995.0), i.e. at least one tag point can actually break.
        bool DetachablePart();

        bool IsExhaustPart();

        // X360 0x825B3E60. True for the "toughened" part types (the part-type switch covering the
        // structural/chassis parts 1/2/26-29 and 108-115); these use the higher detach-impulse band.
        bool IsToughenedPart() const;

        // ---- joint access (DWARF :123-127, :144-151) -------------------------------------
        s32                          GetNumberOfJoints() const;          // forwards to mpSpec
        const DeformationJointSpec*  GetJointSpec(s32 liIndex) const;    // forwards to mpSpec

        // Trivial raw-index accessor: mu8ActiveJointIndex as s32. NOT the 0x825B4110 body
        // (that is GetActiveJointSpec below). Used for the "active joint set?" tripwire.
        s32                          GetActiveJointIndex() const;

        // X360 0x825B4110 (the truncated "GetAct" symbol). Asserts mu8ActiveJointIndex !=
        // KU8_NO_ACTIVE_JOINT, then returns the active deformation-joint spec pointer
        // &mpaJointSpecs[mu8ActiveJointIndex] (*(mpSpec+448) + idx*sizeof(DeformationJointSpec)).
        // The Hex-Rays "int" return is the pointer-typed-as-int trap.
        const DeformationJointSpec*  GetActiveJointSpec() const;

        // X360 0x825B40A0. Set the active deformation joint (asserts 0 <= index < 256, then
        // stores into mu8ActiveJointIndex).
        void                         SetActiveJointIndex(s32 liActiveJointIndex);

        // ---- spec / transform (DWARF :130-134) -------------------------------------------
        const rw::math::vpu::Matrix44Affine& GetPartGraphicsTransform() const;  // forwards to mpSpec
        const IKBodyPartSpec*                GetSpec() const { return mpSpec; }

        // ---- tag-point geometry queries (DWARF :138-141) ---------------------------------

        // X360 0x825E2410. Index of the tag point with the smallest X (left-most) among this
        // part's tag points; -1 if the part has none. Seeds the scan at +100.0.
        s32 FindLeftMostTagPoint();

        // X360 0x825E24B8. Index of the tag point with the largest X (right-most) among this
        // part's tag points; -1 if the part has none. Seeds the scan at -100.0.
        s32 FindRightMostTagPoint();

        // ---- pool index (DWARF :154-158) -------------------------------------------------
        s16  GetPartPoolIndex() const { return miPartPoolIndex; }
        void SetPartPoolIndex(s16 li16PartPoolIndex) { miPartPoolIndex = li16PartPoolIndex; }

        // ---- box-clamped skinning (DWARF :196) -------------------------------------------

        // X360 0x825C12E8. As UpdateSkinningOffsets, but each driven point's world position is
        // first clamped into lpClampBox (per-lane max with box min, min with box max) before the
        // offset-from-original is written into lpaSkinningOffsets — keeps detached/loose panels
        // from skinning outside the panel's allowed volume.
        void UpdateSkinningOffsetsWithinBox(Vector3Plus* lpaSkinningOffsets,
                                            const CgsGeometric::AxisAlignedBox* lpClampBox);

    private:
        IKDrivenPoint*        maDrivenPoints;       // +0x00 (DWARF :202)
        TagPoint*             maTagPoints;          // +0x04 (DWARF :203)
        const IKBodyPartSpec* mpSpec;               // +0x08 (DWARF :204)
        s16                   miPartPoolIndex;      // +0x0C (DWARF :205)
        u8                    mu8ActiveJointIndex;  // +0x0E (DWARF :206)
    };
}
}
