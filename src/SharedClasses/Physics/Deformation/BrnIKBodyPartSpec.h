#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Matrix44Affine
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // EBodyParts (committed home)

// BrnPhysics::Deformation::IKBodyPartSpec — MINIMAL OWNING SLICE.
//
// The streamed (resource-baked) description of one deformable IK body panel: where its
// driven points / tag points live in the owning DeformableObject's pools, which mesh it
// drives, its part-type / toughness classification, the graphics transform, and the array
// of deformation-joint specs that govern how/when the panel hinges and detaches. Homed at
// the mirrored DWARF path (SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h:150).
//
// This slice reconstructs exactly the fields the IKBodyPart bodies read BY NAME (the X360
// IKBodyPart asm reaches them through `*(mpSpec + off)`):
//   * the driven-point / tag-point start indices + counts (Construct's range asserts; the
//     per-point loops in Update / UpdateSkinningOffsets read miNumberOfDrivenPoints @console
//     spec+464 and miNumberOfTagPoints @console spec+472);
//   * mePartType @console spec+476 (the part-type switch in IsToughenedPart /
//     CheckSensorForcesForJointDetachment, and the mesh id);
//   * mpaJointSpecs @console spec+448 + miNumJoints (GetJointSpec / GetActiveJointSpec);
//   * mGraphicsTransform (GetPartGraphicsTransform).
//
// LAYOUT NOTE (project rule — LAYOUT RECOVERY, members BY NAME): members carry their DWARF
// source line for provenance. The absolute console offsets the X360 asm uses (spec+448 /
// +464 / +472 / +476) are 32-bit-pointer offsets that do NOT (and need not) reproduce
// byte-for-byte on the 64-bit host — every access is BY MEMBER NAME, so the host compiler
// recomputes the addresses and semantic parity (which field, which count) is exact. The full
// DWARF struct also defines the BBoxPointSkinData / BodyPartBBoxSpec skin-data helpers and
// the FixUp/FixDown/GetBBoxSpec API; those are LEFT for IKBodyPartSpec's own TU (this slice
// only homes what IKBodyPart needs to compile). GROW this header — do not fork it.
//
// FORWARD-DECL: DeformationJointSpec is homed by another agent (BrnDeformationJointSpec.h /
// the full BrnIKBodyPartSpec TU). Here it is a pointer member + a const* return type, so a
// forward declaration suffices and avoids redefining a type another agent owns.

namespace BrnPhysics
{
namespace Deformation
{
    // Homed elsewhere (BrnDeformationJointSpec.h / full IKBodyPartSpec TU). Pointer member +
    // const* return only, so forward-declare; do NOT define it here.
    struct DeformationJointSpec;

    struct IKBodyPartSpec
    {
    public:
        // BrnIKBodyPartSpec.h:158 — author the spec's pool placement + classification.
        // (EBodyParts ePartType, s32 iStartIndexOfDrivenPoints, s32 iNumberOfDrivenPoints,
        //  s32 iStartIndexOfTagPoints, s32 iNumberOfTagPoints). Declare-only.
        void Construct(EBodyParts lePartType, s32 liStartIndexOfDrivenPoints,
                       s32 liNumberOfDrivenPoints, s32 liStartIndexOfTagPoints,
                       s32 liNumberOfTagPoints);

        // ---- accessors the IKBodyPart bodies read (declare-only; bodied in spec's own TU,
        //      or trivially inlined below where the field is a direct member read) ---------

        // BrnIKBodyPartSpec.h:162/165 — driven-point pool window (Construct's range asserts).
        s32 GetStartIndexOfDrivenPoints() const { return miStartIndexOfDrivenPoints; }
        s32 GetNumberOfDrivenPoints() const { return miNumberOfDrivenPoints; }   // console spec+464

        // BrnIKBodyPartSpec.h:168/171 — tag-point pool window.
        s32 GetStartIndexOfTagPoints() const { return miStartIndexOfTagPoints; }
        s32 GetNumberOfTagPoints() const { return miNumberOfTagPoints; }         // console spec+472

        // BrnIKBodyPartSpec.h:174 — part classification (drives toughness / detach tuning /
        // mesh id; the X360 asm reads this word at console spec+476).
        EBodyParts GetPartType() const { return mePartType; }

        // BrnIKBodyPartSpec.h:177 — the renderable mesh this panel skins. FLAG: the DWARF
        // stores the mesh id in miPartGraphics; GetMeshId is the IKBodyPart-facing name.
        s32 GetMeshId() const { return miPartGraphics; }

        // BrnIKBodyPartSpec.h:180 — the panel's rest graphics transform (IKBodyPart forwards
        // this through GetPartGraphicsTransform). Returns the rw:: math type the DWARF names.
        const rw::math::vpu::Matrix44Affine& GetPartGraphicsTransform() const { return mGraphicsTransform; }

        // BrnIKBodyPartSpec.h:195 — number of deformation joints on this panel (console
        // spec+... joint count); GetJointSpec indexes the mpaJointSpecs array.
        s32 GetNumberOfJoints() const { return miNumJoints; }

        // BrnIKBodyPartSpec.h:199 — the indexed deformation-joint spec (the X360 GetAct path
        // reaches mpaJointSpecs at console spec+448 then strides by sizeof(DeformationJointSpec)).
        // Declare-only (returns a forward-declared type; bodied in the spec's own TU).
        const DeformationJointSpec* GetJointSpec(s32 liIndex) const;

        // BrnIKBodyPartSpec.h:189/192 — relocate-fix the streamed record (pointer swizzle).
        // Declare-only; bodies in the spec's own TU.
        void FixUp(void* lpBase);
        void FixDown(void* lpBase);

    private:
        // Full DWARF member list (BrnIKBodyPartSpec.h:214-229). mBBoxSkinData is the embedded
        // bounding-box skin-data helper (BodyPartBBoxSpec) the full TU defines; here it is held
        // BY NAME as an opaque byte block so the record layout/offsets the IKBodyPart asm walks
        // stay correct without pulling in the (other-agent-owned) skin-data struct definitions.
        Matrix44Affine        mGraphicsTransform;          // :214
        // :216 mBBoxSkinData (BodyPartBBoxSpec — 10 BBoxPointSkinData entries). Opaque slice:
        // sizeof matches the DWARF (8 corner + centre + joint skin records, each a Vector3 +
        // 3 weights + 3 bone indices = 32 bytes, plus a leading Matrix44Affine orientation).
        // FLAG: held as a byte block (role = embedded bbox skin data) so the surrounding member
        // offsets are preserved without redefining BodyPartBBoxSpec here.
        u8                    mBBoxSkinData[64 + 10 * 32]; // :216 (opaque embedded BodyPartBBoxSpec)
        DeformationJointSpec* mpaJointSpecs;               // :218 (console spec+448)
        s32                   miNumJoints;                 // :219
        s32                   miPartGraphics;              // :221 (mesh id)
        s32                   miStartIndexOfDrivenPoints;  // :223
        s32                   miNumberOfDrivenPoints;      // :224 (console spec+464)
        s32                   miStartIndexOfTagPoints;     // :226
        s32                   miNumberOfTagPoints;         // :227 (console spec+472)
        EBodyParts            mePartType;                  // :229 (console spec+476)
    };
}
}
