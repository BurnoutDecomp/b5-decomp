// GameSource/Physics/PropManager/PropManager_wQ2_09.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave (waveQ) ROUND 2, 2026-08-18.
// Part-file of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp.
//
//     GetPropInertia( const PropTypeData* )     @ 0x82612640  (289 insns)  -- NOT LANDED
//     GetPartInertia( const PropPartTypeData* ) @ 0x82612AC8  (272 insns)  -- NOT LANDED
//
// ⛔ THIS FILE INTENTIONALLY CONTAINS NO BODIES. It is the SINGLE banner for this function
// pair: the round-1 duplicate banner PropManager_wQ_04.cpp has since been DELETED by the
// conductor (verified 2026-08-18: the directory holds only PropManager_wQ_01/_02/_03.cpp), so
// there is no second, staler description of this blocker left to trust. That deletion was the
// point -- wQ_04's BLOCKER 1a asked for a declaration that had already landed, and re-landing
// its requested `s32 GetBBox(...)` spelling beside the real `RwBool` one would have forked a
// non-virtual dispatcher.
//
// The complete, corrected reconstruction of BOTH bodies -- round-1 verify items applied, and
// re-derived instruction by instruction against the raw ARTIST asm by the round-2 lander --
// is parked at
//
//     scratchpad/waveQ2/parked/PropManager_09_GetPropInertia_GetPartInertia.cpp
//
// and it is the intended drop-in for THIS file the moment the single remaining blocker below
// is cleared. It supersedes the round-1 park
// (scratchpad/waveQ/parked/PropManager_04_GetPropInertia_GetPartInertia.cpp), whose banner
// carries three claims that are now wrong: blocker 1a is closed; its instruction counts
// ("~180"/"~170") are wrong (289/272, counted); and its W-lane note says "four instructions"
// where it is one instruction (four BYTES).
//
// -----------------------------------------------------------------------------------------
// BLOCKER 1a -- ⛔ CLOSED 2026-08-18. DO NOT RE-REQUEST IT.
//   `rw::collision::Volume::GetBBox` now exists, non-virtual, at
//   SDKs/EATech/rwcollision/volume_debug_access.h:257 --
//       RwBool GetBBox(const Matrix44Affine*, RwBool, AABBox&) const
//   dispatching through the rwcollision per-TYPE descriptor at `volume+0x40`, function pointer
//   at descriptor+0x04 (NOT a C++ vptr; `sizeof(rw::collision::Volume) == 96` is asserted).
//   ⚠️ The only file that still claimed this was missing (PropManager_wQ_04.cpp) has been
//   deleted; note the landed signature's second parameter is `RwBool`, not the `s32` that stale
//   banner requested -- do not land a second overload.
//
// BLOCKER 1b -- ⚠️ STILL OPEN (re-measured 2026-08-18), and it is now the ONLY thing between
//   the parked bodies and the tree. `rw::collision::AABBox` cannot be NAMED as a complete type
//   in any TU that also names the game's `Vector3`, because two definitions of
//   `rw::math::vpu::Vector3` exist:
//       vendor/renderware/include/rw/math/vpu/types.h:24   struct { float x,y,z,w; }
//           <- what BrnCommonTypes.h pulls, i.e. what `Vector3` IS
//       src/SDKs/EATech/include/rw/math/vpu/vector3.h:26   class  { VectorIntrinsic mV; }
//           <- what vendor/renderware/collision/AABBox.hpp:4 pulls
//   Both bodies declare TWO AABBox OBJECTS BY VALUE (the DWARF's own `lAccumulatedAABBox` and
//   `lVolumeAABBox`, the second being the 32-byte out-parameter GetBBox writes), so an
//   incomplete type is not enough and no include ordering avoids it.
//   The same hazard is already recorded at BrnPropManager.cpp:149-157.
//
//   THE EXACT MISSING LINE: vendor/renderware/collision/AABBox.hpp:4-5
//       #include "SDKs/EATech/include/rw/math/vpu/vector3.h"
//       #include "SDKs/EATech/include/rw/math/vpu/matrix44.h"
//   must both become the vendor POD home `#include "rw/math/vpu/types.h"`.
//   ⚠️ BUT NOT ON ITS OWN -- THE BLAST RADIUS IS MEASURED, NOT ESTIMATED:
//     * 14 files in the tree reach AABBox.hpp today (counted 2026-08-18 by grepping the whole of
//       b5-decomp/src + vendor for the include; the 15th hit is this banner). They span
//       the vendor/renderware/collision directory, SDKs/EATech/rwcollision/volume_debug_access.h,
//       GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.{h,cpp} and BrnPropManager.cpp.
//     * The waveQ2 rwcollision owner RAN the swap and then REVERTED it (AABBox.hpp md5 restored):
//       8 of the 11 TUs that compile AABBox.hpp fail, in THREE independent ways --
//       `.mV.mafLane[i]` member access (AABBox.cpp 4 sites, AggregateVolume.cpp 2,
//       ClusteredMeshQuery.cpp 2, VolumeBBoxQuery.cpp 4, CgsAABBoxBuilder.cpp 10), a missing 3-arg
//       `Vector3(x,y,z)` ctor on the POD (Capsule/Cylinder/TriangleVolume), and a missing
//       `VectorIntrinsic` (VolumeBBoxQuery.cpp:147). AggregateVolume.hpp ALSO pulls the EATech
//       matrix44.h on its own, so the swap does not even remove the clash from the collision
//       family: `Matrix44Affine`/`Mult` is a second duplicated pair and `VectorIntrinsic` a third.
//       This is a vocabulary collapse, not an include swap.
//   The costed work list is scratchpad/waveQ2/rwvol.owner.md §4.5 (six items; direction: the
//   vendor POD survives, the EATech class migrates). ONE line item is outside rwcollision
//   ownership and needs a separate owner grant:
//   GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.cpp (10 `.mV.mafLane` sites).
//
// -----------------------------------------------------------------------------------------
// THE BLOCK IS EXACTLY THAT AND NOTHING ELSE -- both bounds MEASURED against the CURRENT tree
// on 2026-08-18, with the probes regenerated from the parked file's own code section
// (scratchpad/waveQ2/probe_wQ2_09/):
//   * probe_land.cpp           (the parked code verbatim)                   -> STATUS=fail,
//     FIRST diagnostic `C2011 "rw::math::vpu::Vector3": Typneudefinition`, and 109 error lines.
//     ⚠️ THAT RUN BOUNDS NOTHING BY ITSELF (round-2 NIT -- the earlier banner argued from "of
//     the 109 error lines, ZERO mention GetBBox or HACKShouldMoveComOffset"): the compile ABORTS
//     at MSVC's error cap before it ever parses the bodies -- the log's last line is
//     `fatal error C1003: Mehr als 100 Fehler gefunden` from vector3_type_inline.h(282) -- so the
//     absence of those names proves only that the cap was hit. The upper bound comes from the
//     podshim probe below (STATUS=pass), and more strongly from
//     scratchpad/waveQ2/probe_verify_land_land-inertia/probe_post45.cpp, which compiles the parked
//     code against a REAL repointed AABBox.hpp and also passes.
//   * probe_land_podshim.cpp   (the same, with ONLY the AABBox.hpp include swapped for a
//     probe-local `class AABBox { Vector3 mMin; Vector3 mMax; };` over the vendor POD)
//                                                                            -> STATUS=pass.
//     So every other declaration these bodies need is landed and binding today:
//     Volume::GetBBox, PropTypeData::HACKShouldMoveComOffset, Matrix44Affine::SetIdentity,
//     Get{NumberOfVolumes,CollisionVolume,Mass} on both records, K_LAMPOST_INERTIA_BOX.
//   * probe_land_noaabbox.cpp  (the same, with the include simply deleted)  -> STATUS=fail
//     with exactly the two `C2079 undefined class "rw::collision::AABBox"` and the C2664 they
//     cause on GetBBox's third argument. Nothing else is missing.
//
// -----------------------------------------------------------------------------------------
// LINK-LEVEL, so nobody mistakes a green gate for a working game (AGENTS gotcha 12):
//   * There is NO inert boot gate and NO other definition of either function anywhere in
//     b5-decomp (grepped src + vendor, .cpp/.h/.hpp): landing the parked file creates no
//     LNK2005, and there is no gate for the conductor to retire.
//   * `Volume::GetBBox` is an SDK HEADER INLINE with no X360 address -- it is not an export
//     hole and must not be added to the ledger as a function. But the descriptors it
//     dispatches through are LINK STUBS today: volume.cpp's `gVolumeVTable[1..6]` point at
//     `gVolumeHandler_82F91*` symbols defined as single zero bytes in
//     SDKs/EATech/AptRenderLinkStubs.cpp, and `Volume::InitializeVTable` binds to the inert
//     gate at WorldLinkStubs.cpp:2573 rather than its real body (static/non-static mangling
//     mismatch -- rwvol.owner.md §7.5). So even after 1b is cleared these two bodies will
//     compile and link but NULL-DEREFERENCE at run time until real descriptors exist;
//     `SphereVolume::GetBBox @0x82BA8020` and `BoxVolume::GetBBox @0x82BA9FC8` have no home
//     in this tree at all. That is a separate TU of work, flagged here, not fixed here.
//
// ⚠️ LEDGER (unchanged from round 1, still true): progress/status.json carries
//    `status: reviewed` for BOTH GetPropInertia and GetPartInertia while NEITHER is
//    implemented -- coverage_check still lists both as missing. Do not count them toward any
//    wave total and do not close this TU on that basis.

// (No code. See the parked file named above.)
