#include "vendor/renderware/collision/GPInstance.hpp"

#include "vendor/renderware/collision/AABBox.hpp"   // AABBox (the GetBBoxFn slot's out param)

// ===========================================================================
// rw::collision GP REGISTRATION TABLES -- the two rodata dispatch tables the
// narrow phase runs on. Recovered 2026-08-18 (waveQ5 C1) by headless IDA on a
// PRIVATE copy of BURNOUT_X360_ARTIST.XEX.i64; every slot below is a dumped
// pointer whose target IDA names, not an inference.
//
//   rw::collision::gapFindBestSeparatingDirection   X360 off_82F91800 (6x6)
//   rw::collision::g_aGPVolumeMethods               X360 unk_82F918F0 (6 x 0x10)
//
// This is the TU that GPInstance.hpp, CapsuleVolume.cpp and TriangleVolume*.cpp
// have been calling "the un-recovered GP registration TU" (canonical
// FixupSepDirMethods, rwccore.h:2955). It carries DATA only -- no reconstructed
// function bodies other than the one folded empty callback described below -- which is
// why it can land ahead of the SAT/prism kernels that are still missing.
//
// WHY IT IS DATA, NOT CODE, ON THIS BUILD: on the console the canonical SDK
// fills both tables at runtime from pointers-to-member (FixupSepDirMethods).
// The X360 compile emitted them as fully-resolved rodata blocks of PLAIN
// function pointers -- that is what the two dumps below are -- so the host
// spelling is a static initialiser, and the GPInstance.hpp "X360 SHAPE DELTA"
// note (static plain-function callbacks) is what makes it type-check.
//
// AGENTS gotcha 5 ("rodata tables are dumps, not blocks") applies and was
// checked: both tables' extents are pinned by their contents, not assumed.
// off_82F91800 runs exactly 36 slots -- slot 36 (0x82F91890) is a string
// pointer (aBadAllocation_24), and 0x82F91894 is the start of the CYLINDER
// Volume descriptor record. unk_82F918F0 runs exactly 6 records of 4 slots --
// 0x82F91950 onward is a string pointer plus TriangleKDTreeProcedural entry
// points, i.e. a different record.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // THE CONSOLE FUNCTION IS EMPTY. Every VolumeMethods record's +0xB0
    // (GetBBox) slot in unk_82F918F0 holds 0x82AD5078, and 0x82AD5078 is the
    // XEX's ICF-folded shared empty function -- a lone `blr`, the single
    // address every empty function in the image was folded onto (AGENTS
    // gotcha 6 names this exact address). This is therefore the reconstructed
    // body of a real function that executes nothing, reproduced exactly; the
    // GP GetBBox slot simply returns on this build for all five types.
    void GPFoldedEmptyGetBBox(const GPInstance* /*lpThis*/, AABBox& /*arBBox*/)
    {
    }
}

// ---------------------------------------------------------------------------
// rw::collision::gapFindBestSeparatingDirection -- X360 off_82F91800
//
// Indexed [gp1.mVolumeType][gp2.mVolumeType]; the row stride of 6 is the
// `mulli r11, rN, 6` in ComputeContactPoints and both batch kernels. Dump
// (address, value, IDA name), 36 consecutive slots 0x82F91800..0x82F9188C:
//
//   [0][0..4] 0x82BB4660 FindBestSeparatingDirection   [0][5] 0x82BAA258 VolCyl
//   [1][0]    0x82BB4660                                [1][1] 0x82BAA1F0 SphSph
//   [1][2..4] 0x82BB4660                                [1][5] 0x82BAA258 VolCyl
//   [2][0..4] 0x82BB4660                                [2][5] 0x82BAA258 VolCyl
//   [3][0..3] 0x82BB4660        [3][4] 0x82BB4CA8 TriBox [3][5] 0x82BAA258 VolCyl
//   [4][0..2] 0x82BB4660        [4][3] 0x82BAA1A0 BoxTri
//                               [4][4] 0x82BB47F0 BoxBox [4][5] 0x82BAA258 VolCyl
//   [5][0..5] 0x82BAA250 FindBestSeparatingDirCylVol
//
// Reading of the table: the CYLINDER row and column are wholly served by the
// two cylinder thunks; SPHERExSPHERE, TRIANGLExBOX, BOXxTRIANGLE and BOXxBOX
// have dedicated closed forms; everything else falls to the generic
// candidate-and-project SAT driver @ 0x82BB4660. Type ids are the canonical
// VolumeType run (0 UNUSED, 1 SPHERE, 2 CAPSULE, 3 TRIANGLE, 4 BOX,
// 5 CYLINDER); row/column 0 is never indexed by a live GPInstance but is
// present in the image and reproduced.
//
// [3][4] WAS the one slot whose target had no body in the tree. CLOSED
// 2026-08-18 (waveQ5 rwc3): FindBestSeparatingDirectionTriBox @ 0x82BB4CA8
// (272 insns) is bodied at the END of SeparatingDirection.cpp, under the
// "wave Q5 rwc3" banner, so this slot and the BoxTri thunk at
// SeparatingDirection.cpp:538 both resolve now. (Comment-only correction; the
// slot expression itself was already correct.)
// ---------------------------------------------------------------------------
const FindBestSeparatingDirectionFn
gapFindBestSeparatingDirection[GPInstance::NUMINTERNALTYPES][GPInstance::NUMINTERNALTYPES] =
{
    // gp2:  UNUSED                       SPHERE                              CAPSULE
    //       TRIANGLE                     BOX                                 CYLINDER
    /* UNUSED   */ { FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirVolCyl },
    /* SPHERE   */ { FindBestSeparatingDirection,        FindBestSeparatingDirectionSphSph,
                     FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirVolCyl },
    /* CAPSULE  */ { FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirVolCyl },
    /* TRIANGLE */ { FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirectionTriBox,  FindBestSeparatingDirVolCyl },
    /* BOX      */ { FindBestSeparatingDirection,        FindBestSeparatingDirection,
                     FindBestSeparatingDirection,        FindBestSeparatingDirectionBoxTri,
                     FindBestSeparatingDirectionBoxBox,  FindBestSeparatingDirVolCyl },
    /* CYLINDER */ { FindBestSeparatingDirCylVol,        FindBestSeparatingDirCylVol,
                     FindBestSeparatingDirCylVol,        FindBestSeparatingDirCylVol,
                     FindBestSeparatingDirCylVol,        FindBestSeparatingDirCylVol },
};

// ---------------------------------------------------------------------------
// rw::collision::g_aGPVolumeMethods -- X360 unk_82F918F0
//
// One 0x10-byte record per VolumeType, copied wholesale into
// GPInstance::mMethods by each volume's CreateGPInstance (CapsuleVolume.cpp:198
// `arInst.mMethods = g_aGPVolumeMethods[arInst.mVolumeType]`; TriangleVolume.cpp
// documents the same fetch as "0x82F918F0 + 3*0x10"). Dump:
//
//   [0] UNUSED   0x82F918F0  00000000 00000000 00000000 00000000
//   [1] SPHERE   0x82F91900  82BA8088 82BA80A8 82BA80C0 82AD5078
//   [2] CAPSULE  0x82F91910  82BAFA80 82BAFB68 82BAFC10 82AD5078
//   [3] TRIANGLE 0x82F91920  82BBA158 82BBA6A0 82BBA6E0 82AD5078
//   [4] BOX      0x82F91930  82BA8918 82BA8650 82BA9238 82AD5078
//   [5] CYLINDER 0x82F91940  82BAE430 82BAD7F8 82BAD938 82AD5078
//
// Slot order is GetMaximumFeature (+0xA4), GetInterval (+0xA8), GetIntervals
// (+0xAC), GetBBox (+0xB0) -- the committed VolumeMethods member order. All
// fifteen concrete callbacks have bodies in this directory; the sixteenth
// distinct target, 0x82AD5078, is the folded empty function above.
//
// Row 0 is all-zero in the image and is reproduced as all-null: a GPInstance
// of type UNUSED that reaches a dispatch would null-call on the console too.
// ---------------------------------------------------------------------------
const GPInstance::VolumeMethods g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES] =
{
    /* [0] UNUSED   */ { 0, 0, 0, 0 },
    /* [1] SPHERE   */ { &GPSphere::GetMaximumFeature,   &GPSphere::GetInterval,
                         &GPSphere::GetIntervals,        &GPFoldedEmptyGetBBox },
    /* [2] CAPSULE  */ { &GPCapsule::GetMaximumFeature,  &GPCapsule::GetInterval,
                         &GPCapsule::GetIntervals,       &GPFoldedEmptyGetBBox },
    /* [3] TRIANGLE */ { &GPTriangle::GetMaximumFeature, &GPTriangle::GetInterval,
                         &GPTriangle::GetIntervals,      &GPFoldedEmptyGetBBox },
    /* [4] BOX      */ { &GPBox::GetMaximumFeature,      &GPBox::GetInterval,
                         &GPBox::GetIntervals,           &GPFoldedEmptyGetBBox },
    /* [5] CYLINDER */ { &GPCylinder::GetMaximumFeature, &GPCylinder::GetInterval,
                         &GPCylinder::GetIntervals,      &GPFoldedEmptyGetBBox },
};

} // namespace collision
} // namespace rw
