#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Deformation::IKBodyPart -- the SKINNING + DETACHMENT side of one deformable
// vehicle panel. Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (offset authority).
// This TU bodies the declare-only methods of the frozen layout header:
//
//   Construct                         @ 0x825B3EE8  -- wire pools + spec, seed sentinels
//   IsToughenedPart                   @ 0x825B3E60  -- part-type "tough" classifier
//   SetActiveJointIndex               @ 0x825B40A0  -- store active joint (range-asserted)
//   GetActiveJointIndex (DWARF "GetAct") @ 0x825B4110 -- active joint spec ptr
//   DetachablePart                    @ 0x825C13F8  -- any tag point breakable?
//   UpdateSkinningOffsets             @ 0x825C11F0  -- push per-point offset into the skin
//   UpdateSkinningOffsetsWithinBox    @ 0x825C12E8  -- as above, box-clamped
//   CheckSensorForcesForJointDetachment @ 0x825C17F8 -- peak sensor impulse vs detach band
//   FindLeftMostTagPoint              @ 0x825E2410  -- min-X tag point index
//   FindRightMostTagPoint             @ 0x825E24B8  -- max-X tag point index
//   Update                            @ 0x825E76F0  -- re-solve every owned IKDrivenPoint
//
// LAYOUT NOTE (offset authority): the X360 image uses 32-bit pointers, so the asm reaches
// members by raw byte offset. Every access below is BY MEMBER NAME via the frozen header /
// the sibling spec accessors, so the host recomputes addresses and semantic parity is exact:
//   this+0  maDrivenPoints   this+4 maTagPoints   this+8 mpSpec
//   this+0x18(word6) miPartPoolIndex (=-1 in Construct)
//   this+0x38(word14) mu8ActiveJointIndex (=-1/0xFF in Construct, set by SetActiveJointIndex)
//   spec+464 GetNumberOfDrivenPoints   spec+472 GetNumberOfTagPoints
//   spec+476 GetPartType   spec+448 mpaJointSpecs (GetActiveJointIndex)
//   IKDrivenPoint stride 48 (0x30); TagPoint stride 32 (0x20).
//
// The asserts are NON-gating tripwires: in the asm execution continues past a failed
// BeginAssert/FireAssert/EndAssert triple with no early-out, so each CGS_ASSERT here is a
// pure tripwire and the work that follows runs regardless (exactly like the asm).

namespace BrnPhysics
{
namespace Deformation
{
    // ---------------------------------------------------------------------------------------------
    // FLAGGED-0 PLACEHOLDERS for the three detach-impulse thresholds (extern-declared in the frozen
    // header at DWARF BrnIKBodyPart.h:370-372). CheckSensorForcesForJointDetachment loads these from
    // X360 rodata:
    //   non-toughened band upper  <- &unk_82FB9730  => KVF_MAX_IMPULSE_FOR_DETACHMENT
    //   toughened band upper      <- &unk_82FB9630  => KVF_MAX_IMPULSE_FOR_DETACHMENT_TOUGH
    //   toughened band lower      <- &unk_82FB9670  => KVF_MIN_IMPULSE_FOR_DETACHMENT
    // The concrete rodata bytes are NOT present in the per-function exports. Per project rule they are
    // carried here as correctly-shaped, zero-initialised placeholders -- NEVER fabricated. The
    // band-selection + comparison SHAPE below is exact; the numeric band stays inert (every panel sees
    // a [0,0]/[0,0] band, so "peak within band" only ever fires at exactly zero peak) until the real
    // XEX rodata is recovered. Replace the zeros with the homed values then.
    VecFloat KVF_MIN_IMPULSE_FOR_DETACHMENT       = { 0.0f, 0.0f, 0.0f, 0.0f };  // &unk_82FB9670 (FLAGGED-0)
    VecFloat KVF_MAX_IMPULSE_FOR_DETACHMENT       = { 0.0f, 0.0f, 0.0f, 0.0f };  // &unk_82FB9730 (FLAGGED-0)
    VecFloat KVF_MAX_IMPULSE_FOR_DETACHMENT_TOUGH = { 0.0f, 0.0f, 0.0f, 0.0f };  // &unk_82FB9630 (FLAGGED-0)

    namespace
    {
        // The "never detaches" sentinel DetachablePart compares each tag point's detach-threshold
        // against (asm: lfc f0,9995.0; fcmpu >= ). A tag point whose squared detach threshold is at or
        // above this is treated as unbreakable.
        const f32 KF_TAG_POINT_NEVER_DETACHES = 9995.0f;

        // Part-type classifier shared by IsToughenedPart and CheckSensorForcesForJointDetachment's
        // toughened-band branch (the identical part-type switch in both X360 bodies: the structural
        // /chassis types 1,2,26-29 and 108-115).
        inline bool IsToughenedPartType(EBodyParts lePartType)
        {
            switch ( static_cast<s32>(lePartType) )
            {
                case 1:   case 2:
                case 26:  case 27:  case 28:  case 29:
                case 108: case 109: case 110: case 111:
                case 112: case 113: case 114: case 115:
                    return true;
                default:
                    return false;
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Construct @ 0x825B3EE8
    //   Range-assert the spec's pool windows, then point the part at its slices of the owning
    //   DeformableObject's driven-point / tag-point pools and seed the -1 "unset" sentinels.
    //     maDrivenPoints = lpDrivenPointArrayBase + 48*GetStartIndexOfDrivenPoints()   (*v5 = 48*a2[115]+a3)
    //     maTagPoints    = lpTagPointArrayBase    + 32*GetStartIndexOfTagPoints()      (v5[1] = 32*a2[117]+a4)
    //     mpSpec = lpSpec ; miPartPoolIndex = -1 ; mu8ActiveJointIndex = -1 (0xFF)
    // ---------------------------------------------------------------------------------------------
    void IKBodyPart::Construct(const IKBodyPartSpec* lpSpec, IKDrivenPoint* lpDrivenPointArrayBase,
                               TagPoint* lpTagPointArrayBase)
    {
        CGS_ASSERT(lpSpec->GetStartIndexOfDrivenPoints() < 128,
                   "lpData->GetStartIndexOfDrivenPoints() < KI_MAX_NUM_DRIVEN_POINTS_PER_DEFORMABLE_OBJECT");
        CGS_ASSERT(lpSpec->GetStartIndexOfDrivenPoints() >= 0,
                   "lpData->GetStartIndexOfDrivenPoints() >= 0");
        CGS_ASSERT(lpSpec->GetNumberOfDrivenPoints() >= 0,
                   "lpData->GetNumberOfDrivenPoints() >= 0");
        CGS_ASSERT(lpSpec->GetStartIndexOfDrivenPoints() + lpSpec->GetNumberOfDrivenPoints() < 128,
                   "lpData->GetStartIndexOfDrivenPoints() + lpData->GetNumberOfDrivenPoints() < KI_MAX_NUM_DRIVEN_POINTS_PER_DEFORMABLE_OBJECT");
        CGS_ASSERT(lpSpec->GetStartIndexOfTagPoints() < 128,
                   "lpData->GetStartIndexOfTagPoints() < KI_MAX_NUM_TAG_POINTS_PER_DEFORMABLE_OBJECT");
        CGS_ASSERT(lpSpec->GetStartIndexOfTagPoints() >= 0,
                   "lpData->GetStartIndexOfTagPoints() >= 0");
        CGS_ASSERT(lpSpec->GetNumberOfTagPoints() >= 0,
                   "lpData->GetNumberOfTagPoints() >= 0");
        CGS_ASSERT(lpSpec->GetStartIndexOfTagPoints() + lpSpec->GetNumberOfTagPoints() < 128,
                   "lpData->GetStartIndexOfTagPoints() + lpData->GetNumberOfTagPoints() < KI_MAX_NUM_TAG_POINTS_PER_DEFORMABLE_OBJECT");

        // *v5 = 48*GetStartIndexOfDrivenPoints() + a3 (pointer stride sizeof(IKDrivenPoint)==48).
        maDrivenPoints = lpDrivenPointArrayBase + lpSpec->GetStartIndexOfDrivenPoints();
        // v5[1] = 32*GetStartIndexOfTagPoints() + a4 (pointer stride sizeof(TagPoint)==32).
        maTagPoints = lpTagPointArrayBase + lpSpec->GetStartIndexOfTagPoints();
        mpSpec = lpSpec;
        miPartPoolIndex = -1;                          // *(v5+6) = -1
        mu8ActiveJointIndex = KU8_NO_ACTIVE_JOINT;     // *(v5+14) = -1 (0xFF)
    }

    // ---------------------------------------------------------------------------------------------
    // Update @ 0x825E76F0
    //   Re-solve every owned driven point. Loop count == GetNumberOfDrivenPoints() (spec+464); the
    //   per-iteration assert is the inlined GetDrivenPoint() bounds check (header :236).
    // ---------------------------------------------------------------------------------------------
    void IKBodyPart::Update()
    {
        const s32 liNumDrivenPoints = mpSpec->GetNumberOfDrivenPoints();   // v3 = *(spec+464)
        s32 liIndex = 0;
        if ( liNumDrivenPoints > 0 )
        {
            do
            {
                CGS_ASSERT(liIndex < mpSpec->GetNumberOfDrivenPoints(),
                           "liIndex < GetNumberOfDrivenPoints()");
                maDrivenPoints[liIndex].Update();          // IKDrivenPoint::Update(v4 + *v1)
                ++liIndex;
            }
            while ( liIndex < liNumDrivenPoints );
        }
    }

    // ---------------------------------------------------------------------------------------------
    // UpdateSkinningOffsets @ 0x825C11F0
    //   For each driven point write its accumulated deformation into the caller's skinning-offset
    //   buffer (one 16-byte slot per point, the asm strides r31 by 16):
    //       slot.xyz = currentPosition - originalPosition   (vsubfp drivenPoint+0 - *(drivenPoint+32))
    //       slot.w   = drivenPoint.mfScratchAmount          (the +44 float folded into the w lane)
    //   The asm's vrlimi128 first preserves the slot's prior w lane, then the trailing stack dance
    //   overwrites it with mfScratchAmount -- so the NET observable is {offset.xyz, scratch}. Modelled
    //   here through the IKDrivenPoint accessors (members are private): GetOffsetFromInitialPosition()
    //   is exactly currentPos-originalPos, GetScratchAmount() is the w lane. The per-iteration assert
    //   is the inlined GetDrivenPoint() bounds check (header :236).
    // ---------------------------------------------------------------------------------------------
    void IKBodyPart::UpdateSkinningOffsets(Vector3Plus* lpaSkinningOffsets)
    {
        s32 liIndex = 0;
        if ( mpSpec->GetNumberOfDrivenPoints() > 0 )
        {
            do
            {
                const Vector3 lvOffset = maDrivenPoints[liIndex].GetOffsetFromInitialPosition();
                const f32     lfScratch = maDrivenPoints[liIndex].GetScratchAmount();

                CGS_ASSERT(liIndex < mpSpec->GetNumberOfDrivenPoints(),
                           "liIndex < GetNumberOfDrivenPoints()");

                lpaSkinningOffsets[liIndex].x = lvOffset.x;
                lpaSkinningOffsets[liIndex].y = lvOffset.y;
                lpaSkinningOffsets[liIndex].z = lvOffset.z;
                lpaSkinningOffsets[liIndex].w = lfScratch;   // v13 = *(drivenPoint+44) into the w lane
                ++liIndex;
            }
            while ( liIndex < mpSpec->GetNumberOfDrivenPoints() );
        }
    }

    // ---------------------------------------------------------------------------------------------
    // UpdateSkinningOffsetsWithinBox @ 0x825C12E8
    //   As UpdateSkinningOffsets, but each driven point's world position is first clamped into
    //   lpClampBox before the offset-from-original is taken:
    //       clamped = vminfp( vmaxfp(currentPos, box.mMin), box.mMax )
    //       slot.xyz = clamped - originalPosition
    //       slot.w   = drivenPoint.mfScratchAmount
    //   (asm: lvx box+0=min, box+16=max; vmaxfp v0=max(pos,min); vminfp v0=min(v0,max); vsubfp
    //    v0=clamped-spec[0]; vrlimi128 preserves the slot w, the stack dance then writes scratch.)
    //   Keeps detached/loose panels from skinning outside the panel's allowed volume.
    // ---------------------------------------------------------------------------------------------
    void IKBodyPart::UpdateSkinningOffsetsWithinBox(Vector3Plus* lpaSkinningOffsets,
                                                    const CgsGeometric::AxisAlignedBox* lpClampBox)
    {
        s32 liIndex = 0;
        if ( mpSpec->GetNumberOfDrivenPoints() > 0 )
        {
            const Vector4& lvBoxMin = lpClampBox->mMin;   // lvx r27 (box+0)
            const Vector4& lvBoxMax = lpClampBox->mMax;   // lvx r26 (box+16)
            do
            {
                const Vector3& lvCurrentPos = maDrivenPoints[liIndex].GetPosition();
                const Vector3  lvOriginalPos = maDrivenPoints[liIndex].GetOriginalPosition();
                const f32      lfScratch = maDrivenPoints[liIndex].GetScratchAmount();

                // vmaxfp(pos, boxMin) then vminfp(., boxMax) -- per-lane clamp.
                f32 lfClampedX = lvCurrentPos.x > lvBoxMin.x ? lvCurrentPos.x : lvBoxMin.x;
                f32 lfClampedY = lvCurrentPos.y > lvBoxMin.y ? lvCurrentPos.y : lvBoxMin.y;
                f32 lfClampedZ = lvCurrentPos.z > lvBoxMin.z ? lvCurrentPos.z : lvBoxMin.z;
                lfClampedX = lfClampedX < lvBoxMax.x ? lfClampedX : lvBoxMax.x;
                lfClampedY = lfClampedY < lvBoxMax.y ? lfClampedY : lvBoxMax.y;
                lfClampedZ = lfClampedZ < lvBoxMax.z ? lfClampedZ : lvBoxMax.z;

                CGS_ASSERT(liIndex < mpSpec->GetNumberOfDrivenPoints(),
                           "liIndex < GetNumberOfDrivenPoints()");

                lpaSkinningOffsets[liIndex].x = lfClampedX - lvOriginalPos.x;
                lpaSkinningOffsets[liIndex].y = lfClampedY - lvOriginalPos.y;
                lpaSkinningOffsets[liIndex].z = lfClampedZ - lvOriginalPos.z;
                lpaSkinningOffsets[liIndex].w = lfScratch;   // v16 = *(drivenPoint+44) into the w lane
                ++liIndex;
            }
            while ( liIndex < mpSpec->GetNumberOfDrivenPoints() );
        }
    }

    // ---------------------------------------------------------------------------------------------
    // CheckSensorForcesForJointDetachment @ 0x825C17F8
    //   1) Tough-check early-out: when lbIsToughCheck and the part type is 14 or 15, return false.
    //   2) Reduce the panel's peak sensor impulse: for every tag point, max-fold the w lane of each of
    //      its two sensors' impulse vectors (sensor+0x10) into a running peak (accumulator starts 0).
    //   3) Select the detach band by toughness and compare:
    //        toughened  -> band = [KVF_MIN_IMPULSE_FOR_DETACHMENT, KVF_MAX_IMPULSE_FOR_DETACHMENT_TOUGH]
    //        otherwise  -> band = [0, KVF_MAX_IMPULSE_FOR_DETACHMENT]
    //      Detach (return true) iff  lower <= peak <= upper, i.e. NOT(peak > upper) AND NOT(lower > peak).
    //
    //   asm decode of the two vcmpgtfp128. tests (all lanes equal because each operand is a w-splat):
    //     vcmpgtfp128. v0, v127(peak), v0(upper) ; if all-true (peak>upper) goto no-detach(result 0)
    //     result = 1
    //     vcmpgtfp128. v0, v13(lower), v127(peak); if all-true (lower>peak) result = 0
    //   The per-tag index asserts (liIndex>=0 / liIndex<GetNumberOfTagPoints()) are the inlined
    //   GetTagPoint() bounds checks (header :264/:265).
    //
    //   The peak fold reads each sensor's impulse vector by name (GetDeformationSensorA/B ->
    //   GetMaxSensorImpulse(), the w lane the vspltw,3 broadcasts). See FLAG below.
    // ---------------------------------------------------------------------------------------------
    bool IKBodyPart::CheckSensorForcesForJointDetachment(bool lbIsToughCheck) const
    {
        // Tough-check exhaust-ish early-out (part types 14 / 15).
        if ( lbIsToughCheck )
        {
            const s32 liPartType = static_cast<s32>(mpSpec->GetPartType());
            if ( liPartType == 14 || liPartType == 15 )
            {
                return false;   // goto LABEL_31 (result = 0)
            }
        }

        // vspltisw v126,0 ; vmr v127,v126 -- peak accumulator starts at the zero vector (w lane == 0).
        f32 lfPeakImpulse = 0.0f;

        const s32 liNumTagPoints = mpSpec->GetNumberOfTagPoints();   // *(spec+472)
        s32 liIndex = 0;
        if ( liNumTagPoints > 0 )
        {
            do
            {
                CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
                CGS_ASSERT(liIndex < mpSpec->GetNumberOfTagPoints(),
                           "liIndex < GetNumberOfTagPoints()");

                const TagPoint& lTagPoint = maTagPoints[liIndex];
                // sensor+0x10 vector, w lane (vspltw v0,v0,3) -- max-folded across both sensors.
                const f32 lfImpulseA = lTagPoint.GetDeformationSensorA()->GetMaxSensorImpulse().w;
                const f32 lfImpulseB = lTagPoint.GetDeformationSensorB()->GetMaxSensorImpulse().w;

                // vmaxfp128 v0 = max(peak, A) ; vmaxfp128 peak = max(v0, B).
                f32 lfStep = lfPeakImpulse > lfImpulseA ? lfPeakImpulse : lfImpulseA;
                lfPeakImpulse = lfStep > lfImpulseB ? lfStep : lfImpulseB;

                ++liIndex;
            }
            while ( liIndex < liNumTagPoints );
        }

        // Band selection by part-type toughness (the same switch as IsToughenedPart).
        const bool lbToughened = IsToughenedPartType(mpSpec->GetPartType());

        f32 lfLowerBound;
        f32 lfUpperBound;
        if ( lbToughened )
        {
            lfUpperBound = KVF_MAX_IMPULSE_FOR_DETACHMENT_TOUGH.w;   // v0  <- &unk_82FB9630
            lfLowerBound = KVF_MIN_IMPULSE_FOR_DETACHMENT.w;         // v13 <- &unk_82FB9670
        }
        else
        {
            lfLowerBound = 0.0f;                                     // v13 <- v126 (zero vector)
            lfUpperBound = KVF_MAX_IMPULSE_FOR_DETACHMENT.w;         // v0  <- &unk_82FB9730
        }

        // vcmpgtfp128. peak > upper -> no detach.
        if ( lfPeakImpulse > lfUpperBound )
        {
            return false;   // goto LABEL_31 (result = 0)
        }
        // result = 1 ; vcmpgtfp128. lower > peak -> result = 0.
        bool lbDetach = true;
        if ( lfLowerBound > lfPeakImpulse )
        {
            lbDetach = false;
        }
        return lbDetach;
    }

    // ---------------------------------------------------------------------------------------------
    // DetachablePart @ 0x825C13F8
    //   True if at least one of this part's tag points can actually break: scan the tag points and
    //   return true as soon as a tag point's joint/detach threshold drops below the "never detaches"
    //   sentinel (9995.0). If every tag point is at/above the sentinel (or there are none) -> false.
    //     asm: while ( *(*(tagPoint+16)+56) >= 9995.0 ) advance; ...
    //          tagPoint+16 = TagPoint::mpSpec ; spec+56 = TagPointSpec::mfDetachThresholdSquared.
    //   Read by name via TagPoint::GetDetachThresholdSquared().
    // ---------------------------------------------------------------------------------------------
    bool IKBodyPart::DetachablePart()
    {
        s32 liIndex = 0;
        if ( mpSpec->GetNumberOfTagPoints() <= 0 )
        {
            return false;   // LABEL_5 (result = 0)
        }

        while ( maTagPoints[liIndex].GetDetachThresholdSquared() >= KF_TAG_POINT_NEVER_DETACHES )
        {
            ++liIndex;
            if ( liIndex >= mpSpec->GetNumberOfTagPoints() )
            {
                return false;   // LABEL_5 (result = 0)
            }
        }
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // IsToughenedPart @ 0x825B3E60
    //   True for the "toughened" structural/chassis part types (1,2,26-29,108-115); else false.
    // ---------------------------------------------------------------------------------------------
    bool IKBodyPart::IsToughenedPart() const
    {
        return IsToughenedPartType(mpSpec->GetPartType());   // switch on *(spec+476)
    }

    // ---------------------------------------------------------------------------------------------
    // SetActiveJointIndex @ 0x825B40A0
    //   Range-assert 0 <= liActiveJointIndex < 256, then store into mu8ActiveJointIndex (*(v3+14)=a2).
    // ---------------------------------------------------------------------------------------------
    void IKBodyPart::SetActiveJointIndex(s32 liActiveJointIndex)
    {
        CGS_ASSERT(liActiveJointIndex < 256, "liActiveJointIndex < 256");
        CGS_ASSERT(liActiveJointIndex >= 0, "liActiveJointIndex >= 0");
        mu8ActiveJointIndex = static_cast<u8>(liActiveJointIndex);
    }

    // ---------------------------------------------------------------------------------------------
    // GetActiveJointIndex @ 0x825B4110 (DWARF truncated symbol "GetAct")
    //   Assert an active joint is set, then return the active deformation-joint spec:
    //     return *(spec+448) + (mu8ActiveJointIndex ROL 6)   == &mpaJointSpecs[mu8ActiveJointIndex]
    //   (__ROL4__(index,6) == index*64 == index*sizeof(DeformationJointSpec); spec+448 == mpaJointSpecs).
    //   Reached by name via GetJointSpec(mu8ActiveJointIndex).
    //
    //   FLAG (signature reconciliation): the frozen header declares GetActiveJointIndex() returning
    //   s32, but the X360 body (and the GetActiveJointSpec accessor it backs) returns the indexed
    //   DeformationJointSpec*. The asm computes &mpaJointSpecs[mu8ActiveJointIndex]; modelled below
    //   as the active-joint *index* (the header's declared s32 return), with the spec-pointer form
    //   left to GetActiveJointSpec(). The asserted precondition + the index value are exact.
    // ---------------------------------------------------------------------------------------------
    s32 IKBodyPart::GetActiveJointIndex() const
    {
        CGS_ASSERT(mu8ActiveJointIndex != KU8_NO_ACTIVE_JOINT,
                   "mu8ActiveJointIndex != KU8_NO_ACTIVE_JOINT");
        return static_cast<s32>(mu8ActiveJointIndex);
    }

    // ---------------------------------------------------------------------------------------------
    // FindLeftMostTagPoint @ 0x825E2410
    //   Index of the tag point with the smallest X (left-most). Seed at +100.0; -1 if no tag points.
    //     asm: v3=100.0; for i in [0,GetNumberOfTagPoints()): x = *(maTagPoints[i]) (mPos.x);
    //          if ( x < v3 ) { best=i; v3=x; }
    //   (The shown pseudocode has no per-iteration assert -- GetTagPoint()'s bounds check is provably
    //    in range here and was elided; read directly via maTagPoints[i].GetPosition().x to match.)
    // ---------------------------------------------------------------------------------------------
    s32 IKBodyPart::FindLeftMostTagPoint()
    {
        s32 liLeftMostIndex = -1;            // v1 = -1
        s32 liTagPointIndex = 0;             // v2 = 0
        f32 lfLeftMostPos = 100.0f;          // v3 = 100.0
        if ( mpSpec->GetNumberOfTagPoints() > 0 )
        {
            do
            {
                const f32 lfXDist = maTagPoints[liTagPointIndex].GetPosition().x;
                if ( lfXDist < lfLeftMostPos )
                {
                    liLeftMostIndex = liTagPointIndex;
                    lfLeftMostPos = lfXDist;
                }
                ++liTagPointIndex;
            }
            while ( liTagPointIndex < mpSpec->GetNumberOfTagPoints() );
        }
        return liLeftMostIndex;
    }

    // ---------------------------------------------------------------------------------------------
    // FindRightMostTagPoint @ 0x825E24B8
    //   Index of the tag point with the largest X (right-most). Seed at -100.0; -1 if no tag points.
    //   Mirror of FindLeftMostTagPoint with the comparison flipped (x > v3) and seed -100.0.
    // ---------------------------------------------------------------------------------------------
    s32 IKBodyPart::FindRightMostTagPoint()
    {
        s32 liRightMostIndex = -1;           // v1 = -1
        s32 liTagPointIndex = 0;             // v2 = 0
        f32 lfRightMostPos = -100.0f;        // v3 = -100.0
        if ( mpSpec->GetNumberOfTagPoints() > 0 )
        {
            do
            {
                const f32 lfXDist = maTagPoints[liTagPointIndex].GetPosition().x;
                if ( lfXDist > lfRightMostPos )
                {
                    liRightMostIndex = liTagPointIndex;
                    lfRightMostPos = lfXDist;
                }
                ++liTagPointIndex;
            }
            while ( liTagPointIndex < mpSpec->GetNumberOfTagPoints() );
        }
        return liRightMostIndex;
    }
}
}
