// =========================================================================================
// PARKED (ROUND 2) -- scratchpad/waveQ2/parked/PropManager_09_GetPropInertia_GetPartInertia.cpp
//
// waveQ (breakable props, 2026-08-18) round-2 lander, BOTH functions:
//     BrnPhysics::Props::PropManager::GetPropInertia @ 0x82612640   (289 instructions)
//     BrnPhysics::Props::PropManager::GetPartInertia @ 0x82612AC8   (272 instructions)
// (COUNTED, not estimated -- I counted the exported instruction lines and cross-checked the
//  address arithmetic: (0x82612AC0-0x82612640)/4+1 == 289, (0x82612F04-0x82612AC8)/4+1 == 272.
//  The ROUND-1 park banner said "~180"/"~170"; that was wrong.)
//
// This is the intended drop-in for
//     b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_09.cpp
// It is parked, not landed, because of ONE remaining blocker that is not this TU's to fix.
//
// -----------------------------------------------------------------------------------------
// STATE OF THE TWO ROUND-1 BLOCKERS, RE-MEASURED 2026-08-18 BY THIS LANDER
// -----------------------------------------------------------------------------------------
// BLOCKER 1a -- CLOSED. `rw::collision::Volume::GetBBox` now exists, non-virtual, at
//   b5-decomp/src/SDKs/EATech/rwcollision/volume_debug_access.h:257
//       RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool lbTight, AABBox& lrBBox) const
//   dispatching through the rwcollision per-TYPE descriptor at `volume+0x40`, function pointer
//   at descriptor+0x04. (Landed by the waveQ2 rwcollision owner; scratchpad/waveQ2/rwvol.owner.md
//   §1.)  Do NOT re-request it. MEASURED closed here: zero diagnostics in the probes below
//   mention GetBBox, and the C2664 in probe_land_noaabbox.cpp quotes the landed signature
//   verbatim out of volume_debug_access.h(257).
//
// BLOCKER 1b -- STILL OPEN AS OF 2026-08-18, and it is the ONLY thing between this file and
//   the tree. `rw::collision::AABBox` cannot be NAMED as a complete type in any TU that also
//   names the game's `Vector3`, because two definitions of `rw::math::vpu::Vector3` exist:
//       b5-decomp/vendor/renderware/include/rw/math/vpu/types.h:24
//           struct alignas(16) Vector3 { float x, y, z, w; }   <- what BrnCommonTypes.h pulls,
//                                                                 i.e. what `Vector3` IS
//       b5-decomp/src/SDKs/EATech/include/rw/math/vpu/vector3.h:26
//           class Vector3 { VectorIntrinsic mV; }              <- what AABBox.hpp:4 pulls
//   Both bodies need TWO AABBox OBJECTS BY VALUE (`lAccumulatedAABBox` and `lVolumeAABBox` --
//   the DWARF's own local names; the second is the 32-byte out-parameter GetBBox writes), so
//   a forward declaration is not enough and no include ordering avoids it.
//
//   ⚠️ THE EXACT MISSING LINE, as the compiler names it:
//       b5-decomp/src/vendor/renderware/collision/AABBox.hpp:4
//           #include "SDKs/EATech/include/rw/math/vpu/vector3.h"
//       must become the vendor POD home
//           #include "rw/math/vpu/types.h"
//       (and :5's `matrix44.h` with it -- `AABBox::Transform` names `math::vpu::Matrix44Affine`).
//
//   ⚠️ BUT DO NOT LAND THAT ONE LINE ALONE. The waveQ2 rwcollision owner RAN exactly that
//   experiment and MEASURED the fallout: 8 of the 11 TUs that reach AABBox.hpp then fail, in
//   three independent ways (`.mV.mafLane[...]` member access; a missing 3-arg Vector3 ctor;
//   a missing `VectorIntrinsic`), plus an `AggregateVolume.hpp` path that pulls the EATech
//   `matrix44.h` independently so the clash does not even leave the collision family. The
//   complete costed work list is rwvol.owner.md §4.5 -- six items, one of which
//   (`GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.cpp`, 10 sites) sits outside the
//   rwcollision ownership and needs its own grant. THAT is the header_request: the §4.5
//   collapse, not a one-line repoint. (The hazard is already recorded in the tree at
//   GameSource/Physics/PropManager/BrnPropManager.cpp:149-157.)
//
// -----------------------------------------------------------------------------------------
// THE BLOCK IS EXACTLY THAT AND NOTHING ELSE -- MEASURED TODAY, BOTH BOUNDS
// (probes in scratchpad/waveQ2/probe_wQ2_09/, re-run by this lander)
// -----------------------------------------------------------------------------------------
//   probe_land.cpp        = this file's CODE verbatim (banner trimmed) -> STATUS=fail, first
//                           diagnostic `C2011 "rw::math::vpu::Vector3": Typneudefinition`,
//                           SDKs/EATech/include/rw/math/vpu/vector3.h(26) against
//                           vendor/renderware/include/rw/math/vpu/types.h(24). ZERO
//                           diagnostics mention GetBBox or HACKShouldMoveComOffset.
//   probe_land_podshim.cpp = the same code with ONLY the AABBox.hpp include swapped for a
//                           probe-local `class AABBox { public: Vector3 mMin; Vector3 mMax; };`
//                           over the VENDOR POD (i.e. the post-§4.5 world) -> STATUS=pass.
//                           So every other declaration this file needs is landed and binding:
//                           Volume::GetBBox, PropTypeData::HACKShouldMoveComOffset,
//                           Matrix44Affine::SetIdentity, Get{NumberOfVolumes,CollisionVolume,
//                           Mass} on BOTH type records, and K_LAMPOST_INERTIA_BOX.
//   probe_land_noaabbox.cpp = the same code with the AABBox include simply deleted ->
//                           STATUS=fail with exactly two `C2079 undefined class
//                           "rw::collision::AABBox"` (the two locals) and the one C2664 they
//                           cause on GetBBox's third argument (plus its cascade). Nothing
//                           else is missing.
//
// =========================================================================================
// SOURCE OF TRUTH -- WHAT IS MEASURED AND WHAT IS INFERRED
// (Raw asm read by this lander straight out of
//  .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82612640.json and 0x82612AC8.json, both walked in
//  full; dumps kept at scratchpad/waveQ2/probe_wQ2_09/raw_0x8261*.txt.)
// =========================================================================================
//
// MEASURED -- calling convention (from the ASM, not the pseudocode):
//   * Hidden-pointer return. `mr r29,r5` @0x82612664 parks lpType, `mr r25,r3` @0x8261266C
//     parks &result; the tail is `mr r3,r25` @0x82612A6C + `stvx128 v0,r0,r25` @0x82612AA0
//     (GetPartInertia: `mr r29,r5` @0x82612AEC, `mr r25,r3` @0x82612AF4, `mr r3,r25`
//     @0x82612DD8, `stvx128 v0,r0,r25` @0x82612EE4).
//   * r4 == `this` and is NEVER READ by either body -- the only writes to r4 are
//     `addi r4,r1,var_B0` (the &lIdentity argument). Both are effectively static helpers.
//     Hex-Rays renders both as `int f(int,int,int)`; that is the hidden-pointer artefact.
//
// MEASURED -- the volume walk:
//   * Volume count is a u8: PropTypeData +0x5E (`lbz r11,0x5E(r29)` @0x82612678 for the
//     pre-test AND `lbz r10,0x5E(r29)` @0x826128F8 AGAIN at the bottom of every iteration);
//     PropPartTypeData +0x2C (@0x82612B00 and @0x82612D80). The induction variable is
//     re-masked to 8 bits every iteration (`addi r10,r30,1` @0x826128E4 ->
//     `clrlwi r30,r11,24` @0x826128FC -> `cmplw cr6,r30,r10` @0x82612900).
//     -> the loop below therefore calls GetNumberOfVolumes() IN THE LOOP CONDITION and keeps
//        the counter a `u8`: that is what the console does. (Round-1 NIT: the round-1 park
//        hoisted the count into a loop-invariant local. Not hoisted here.)
//   * Volume run: PropTypeData +0x3C (`lwz r10,0x3C(r29)` @0x82612710);
//     PropPartTypeData +0x24 (`lwz r10,0x24(r29)` @0x82612B98). Stride 96
//     (`slwi r11,r30,1 ; add r11,r30,r11 ; slwi r11,r11,5` @0x8261270C/0x82612718/0x8261272C)
//     -- the size of the SERIALISED rw::collision::Volume record. ⚠️ CONSOLE VALUE,
//     DOCUMENTATION ONLY (AGENTS gotcha 1): the code below indexes through the committed
//     GetCollisionVolume(i) accessor and never spells 96 (nor 0x5E/0x3C/0x2C/0x24/0x38/0x20).
//   * GetBBox dispatch: `lwz r11,0x40(r3) ; lwz r11,4(r11) ; mtctr ; bctrl` at
//     0x82612750..0x8261275C (GetPartInertia 0x82612BD8..0x82612BE4), with r3 = the volume,
//     r4 = &lIdentity, r5 = 1 (`li r5,1` @0x8261271C; GetPartInertia @0x82612BA4),
//     r6 = &lVolumeAABBox. The RwBool result in r3 is NOT tested by either caller -- do not
//     add a check.
//     ⚠️ `volume+0x40` is the rwcollision per-TYPE DESCRIPTOR pointer (`maType`), NOT a C++
//     vptr; the function pointer is at descriptor+0x04. GetBBox MUST NOT be virtual --
//     `sizeof(rw::collision::Volume) == 96` is static_asserted (BrnPhysicsPropTypeData.h) and
//     a vptr would shift every field of a serialised wire record.
//     ⚠️ WHICH Volume: the 96-byte serialised record in
//     `SDKs/EATech/rwcollision/volume_debug_access.h` -- the one PropTypeData /
//     PropPartTypeData actually hold. The unrelated 128-byte `rw::collision::Volume`
//     placeholder in `vendor/renderware/collision/CollisionVolume.hpp` is a separate
//     pre-existing fork and has nothing to do with this call.
//   * The stack identity matrix is rebuilt EVERY iteration from four rows: three rdata rows
//     loaded via `w__math__vpu__detail__gIVector` (address formed @0x826126EC, loaded
//     @0x82612708), `unk_82181510` (@0x826126F4/@0x82612728) and `unk_82181520`
//     (@0x826126FC/@0x82612740), plus a `vspltisw128 v125,0` ZERO row
//     (`stvx128 v125,r0,r9` @0x8261274C). MEASURED here: the four stores and their order.
//     CROSS-CITED, not re-dumped by me: those three rdata rows decode to {1,0,0,0} /
//     {0,1,0,0} / {0,0,1,0} (already decoded in
//     GameSource/Director/Camera/Utils/CameraUtils.cpp:128-130). With a zero fourth row that
//     is exactly the committed `Matrix44Affine::SetIdentity()`, whose wAxis is {0,0,0,0} --
//     checked against vendor/renderware/include/rw/math/vpu/types.h:72-76, not assumed.
//     (`w__math__vpu__detail__gIVector` is an IDA-TRUNCATED symbol, AGENTS gotcha 6.)
//
// MEASURED -- the accumulate. All SIX per-lane selects walked individually by this lander,
// with the stack pointer table decoded first:
//     var_120 = the accumulator MAX row (v127)      var_130 = the accumulator MIN row (v126)
//     var_D0  = lVolumeAABBox.mMax (AABBox +0x10)   var_E0  = lVolumeAABBox.mMin (AABBox +0x00)
//   The twelve `stw` at 0x82612688..0x826126E8 build six POINTER PAIRS, and every pair is
//   {&volume row, &accumulator row}:
//     x-max var_100=&var_D0 / var_E4=&var_120     x-min var_FC=&var_E0 / var_EC=&var_130
//     y-max var_F0 =&var_D0 / var_F8=&var_120     y-min var_F4=&var_E0 / var_110=&var_130
//     z-max var_104=&var_D0 / var_E8=&var_120     z-min var_108=&var_E0 / var_10C=&var_130
//   Each lane then: splat both candidates, `vcmpgtfp.`, `mfocrf`+`extrwi`+`bne` to pick one
//   POINTER, `lvx128` the whole selected row back, and `vrlimi128` one lane into the
//   accumulator (masks 8/4/2 == x/y/z). That pointer-select shape is why the DWARF names the
//   source helpers Max/Min<VecFloatRef{X,Y,Z}> -- the REFERENCE-returning per-axis selects.
//   * BOTH accumulator rows start at ZERO: `vspltisw128 v125,0` @0x82612668 then
//     `vmr128 v126,v125` @0x82612674 and `vmr128 v127,v125` @0x8261267C. NOT +/-FLT_MAX.
//     Load-bearing: a type whose volumes all sit off the origin still accumulates a box that
//     contains the origin.
//   * MAX lanes -- TRUE selects the ACCUMULATOR pointer in all three:
//       x @0x82612780 `vcmpgtfp. v0,v12,v0`   v12=accum.max.x, v0 =vol.max.x  -> bne -> var_E4
//       y @0x826127C4 `vcmpgtfp. v12,v11,v12` v11=accum.max.y, v12=vol.max.y  -> bne -> var_F8
//       z @0x82612800 `vcmpgtfp. v13,v12,v13` v12=accum.max.z, v13=vol.max.z  -> bne -> var_E8
//     i.e. `(accum > vol) ? accum : vol`.
//   * MIN lanes -- TRUE also selects the ACCUMULATOR pointer, with the operands the other way:
//       x @0x82612848 `vcmpgtfp. v0,v11,v12`  v11=vol.min.x, v12=accum.min.x -> bne -> var_EC
//       y @0x82612888 `vcmpgtfp. v12,v12,v11` v12=vol.min.y, v11=accum.min.y -> bne -> var_110
//       z @0x826128C4 `vcmpgtfp. v13,v13,v12` v13=vol.min.z, v12=accum.min.z -> bne -> var_10C
//     i.e. `(vol > accum) ? accum : vol`.
//   * `vcmpgtfp` is an ORDERED greater-than: FALSE when either operand is a NaN, so the
//     branch then takes the OTHER pointer. Writing these as `(a > b) ? .. : ..` with the
//     operands in the asm's order reproduces the tie/NaN winner exactly; a library Max/Min or
//     an `fsel`-shaped clamp would not (PPC NaN polarity, AGENTS gotcha 4).
//   * (GetPartInertia's six twins are at 0x82612C08 / 0x82612C4C / 0x82612C88 and
//     0x82612CD0 / 0x82612D10 / 0x82612D4C, same operand order lane for lane.)
//
// MEASURED -- the dimension fold:
//   * `vspltisw v0,-1` @0x82612910 then `vslw v0,v0,v0` (== 0x80000000), then `vandc` on BOTH
//     corners @0x8261294C..0x82612978, then `vmaxfp` @0x8261295C/0x82612970/0x82612980:
//       x: vandc v13(accum.min.x) / vandc v12(accum.max.x) -> vmaxfp v13,v12,v13
//       y: vandc v12(accum.min.y) / vandc v11(accum.max.y) -> vmaxfp v12,v11,v12
//       z: vandc v11(accum.min.z) / vandc v0 (accum.max.z) -> vmaxfp v0 ,v0 ,v11
//     That is `d_i = Max( |min_i| , |max_i| )` -- a sign-bit clear, i.e. fabsf, which also
//     keeps a NaN a NaN where `x < 0 ? -x : x` would not.
//   * THERE IS NO SUBTRACTION. I grepped both full dumps: ZERO `vsubfp` in either body. So
//     d_i is a HALF-EXTENT about the ORIGIN -- not `max - min`, and not about the box centre.
//     (GetPartInertia twin: 0x82612DB8..0x82612E44.)
//
// MEASURED -- the fold and the mass:
//   * The scale is the rodata float flt_820065E0, loaded @0x826129F0, stored into THREE
//     separate stack slots (@0x826129F4/0x82612A0C/0x82612A14) whose other three lanes are
//     zeroed with `stw r31` and then splatted lane-0 into three registers -- i.e. the DWARF's
//     three distinct `operator/` calls, the source's `/ 3.0f` folded to a multiply.
//     Its VALUE, 0.33333334f (0x3EAAAAAB) == 1/3 and NOT 1/12, is NOT re-measured by me: it
//     is taken from the waveQ2 rwcollision owner's headless-IDA dump of the image
//     (rwvol.owner.md §3) and from three committed TUs that already decode the same address
//     (vendor/renderware/physics/FatBoxInertia.cpp, .../audio/core/CpuLoadBalancer.cpp,
//     GameSource/World/ShadowMap/BrnShadowMap.cpp).
//   * Self-check, and it is a strong one: 1/3 with a HALF-extent is the same physics as 1/12
//     with a FULL extent -- (m/12)(2h)^2 == (m/3)h^2. The abs/max reading of the dimensions
//     and the 1/3 reading of the constant confirm each other. BrnPropManager.h:386-407 now
//     carries both halves; do not "restore" either one alone.
//   * Squares @0x826129E4/0x826129EC/0x826129FC from the three splatted lanes of lDims, then
//     pairwise sums @0x82612A40/0x82612A50/0x82612A5C:
//       v6  = d.y^2 + d.z^2 -> multiplied by K @0x82612A7C -> lane x (`vrlimi128` mask 8)
//       v12 = d.x^2 + d.z^2 -> multiplied by K @0x82612A84 -> lane y (mask 4)
//       v0  = d.y^2 + d.x^2 -> multiplied by K @0x82612A8C -> lane z (mask 2)
//     Lane->axis read off the `vrlimi128` masks @0x82612A90/0x82612A94/0x82612A98
//     (GetPartInertia 0x82612ED0/0x82612ED8/0x82612EDC).
//   * Mass: `lfs f0,0x38(r29)` @0x82612A28 (PropTypeData::mfMass) and `lfs f0,0x20(r29)`
//     @0x82612E38 (PropPartTypeData::mfMass); each is stored to var_C0
//     (@0x82612A48 / @0x82612E7C), reloaded (@0x82612A80 / @0x82612EA4), splatted
//     (`vspltw v7,v7,0` @0x82612A88 / @0x82612EB0) and multiplied into ALL FOUR lanes
//     (`vmulfp128 v0,v11,v7` @0x82612A9C / `vmulfp128 v0,v9,v7` @0x82612EE0). Reached below
//     through the committed GetMass() on each record.
//
// MEASURED -- GetPropInertia ONLY, the substitution:
//   * `lwz r11,0x58(r29)` @0x82612924 (muSceneUriId) is compared against 0x6894C (immediate
//     built @0x8261291C/0x8261292C, compare @0x82612948) and, on a miss, against 0x68964
//     (@0x82612998/0x8261299C, compare @0x826129A0). The two arms materialise a 1/0 into r11
//     (@0x826129AC / @0x826129A4), truncate it `clrlwi r11,r11,24` @0x826129B0 and test it
//     @0x826129B4 -- the shape of an inlined bool-returning accessor, not a bare `||`.
//   * On a hit, `lvx128 v0,r0,r11` @0x826129C4 loads unk_82FB9420 (== K_LAMPOST_INERTIA_BOX)
//     straight into the register holding lDims, i.e. the accumulated dimensions are DISCARDED
//     and replaced WHOLESALE before the fold.
//   * That two-id predicate is exactly `PropTypeData::HACKShouldMoveComOffset()`
//     (DWARF BrnPhysicsPropTypeData.h:110; the DWARF places the call in GetPropInertia's outer
//     block at BrnPropManager.cpp:277). It is now a landed header inline at
//     BrnPhysicsPropTypeData.h:222 and is CALLED below rather than re-spelled inline -- the
//     two magic ids no longer appear in this file at all.
//     It is deliberately NARROWER than `IsLamppost()` @0x822A1A00, which tests EIGHT ids --
//     do not unify them.
//   * ⚠️ K_LAMPOST_INERTIA_BOX reads ALL-ZERO out of the shipped image (BrnPropManager.cpp:244
//     defines it zero for exactly that reason; independently re-dumped by the rwcollision
//     owner, rwvol.owner.md §3), so on the console these two prop types get a ZERO inertia
//     here. Faithful, not a bug in this body.
//   * GetPartInertia has NO such branch: I grepped its whole dump -- no `0x58(r29)` load, no
//     0x6894C/0x68964 immediate anywhere, no unk_82FB94xx reference. Parts are never
//     substituted.
//
// INFERRED -- called out so no later sweep mistakes any of it for a measurement:
//   * The returned Vector3's W lane. The console splices lanes x/y/z into a register loaded
//     from an UNWRITTEN stack slot. GetPropInertia: `lvx128 v11,r0,r7` with r7 == var_C0
//     @0x82612A44, and the mass `stfs f0,var_C0(r1)` lands @0x82612A48 -- ONE instruction
//     later (the ROUND-1 park said "four instructions"; that was wrong -- it is four BYTES).
//     GetPartInertia is the same pattern two instructions apart: `lvx128 v9,r0,r10` with
//     r10 == var_C0 (set @0x82612E68) @0x82612E74 vs `stfs f0,var_C0(r1)` @0x82612E7C.
//     The same stale slot also seeds lDims' W lane (`lvx128 v10,r0,r9` with r9 == var_C0 set
//     @0x82612914, load @0x82612944; GetPartInertia r8 == var_C0 set @0x82612D9C, load
//     @0x82612DCC). So W is stack garbage times the mass -- genuinely undefined in the
//     shipped build. Zeroed below rather than left indeterminate: a deliberate, documented
//     divergence on a lane nothing reads (AddPropToSim @0x826274D8 scales the result by
//     KVF_INERTIA_SCALE and posts it as a rigid-body inertia, whose W the simulation ignores).
//   * De-optimisations, per AGENTS.md: the six unrolled per-lane selects are re-rolled into
//     one axis loop, and the per-iteration identity matrix is built with SetIdentity() instead
//     of three rdata row loads plus a zero row. Note the DWARF shows the SOURCE was itself
//     per-axis (three distinct Max<VecFloatRef{X,Y,Z}> instantiations), so the loop re-rolls
//     the source's own hand-unrolling as well as the compiler's.
//   * The outlining of `AccumulateVolumeHalfExtents` / `FoldBoxInertia` is a PRESENTATION
//     choice -- the two emissions are instruction-for-instruction identical across that span
//     and the original almost certainly had it inline in both.
//   * ⚠️ EVERY LOCAL HELPER AND CONSTANT IDENTIFIER IN THIS FILE IS AUTHORED, NOT RECOVERED:
//     MaxLane, MinLane, MaxFp, Lane, KU_AXIS_COUNT, KF_BOX_INERTIA_SCALE,
//     AccumulateVolumeHalfExtents, FoldBoxInertia. The DWARF's OWN names for the helpers the
//     compiler folded here are Max/Min<VecFloatRef{X,Y,Z}>, Abs<VectorAxis{X,Y,Z}>,
//     Max<VecFloat>, operator/, operator*, operator+, operator*=, GetVector3_Zero and
//     GetMatrix44Affine_Identity -- all of which live in the EATech Vector3 vocabulary and are
//     unusable here until blocker 1b is collapsed. When it is, revisit this file and replace
//     the authored helpers with the real ones.
//
// MEASURED -- from the DecFIGS DWARF for this exact .cpp
// (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:148-352):
//   * Local names + source lines. GetPropInertia: lAccumulatedAABBox (:2681), lu8Vol (:2687),
//     lVolumeAABBox (:2690), lpVolume (:2692), lIdentity (:2694), lMax (:2699), lMin (:2700),
//     lDims (:2712), lInertia (:2723). GetPartInertia: the same nine at
//     :2743/:2750/:2753/:2755/:2757/:2762/:2763/:2775/:2780. Every local below carries its
//     DWARF name.
//   * The accumulator is ONE AABBox (lAccumulatedAABBox), not two loose Vector3s -- which is
//     why blocker 1b bites twice per body.
//   * `lpVolume` is spelled `VolRef::Volume*`, and volume.h:39 typedefs
//     `VolRef::Volume == rw::collision::Volume` -- i.e. the 96-byte SDK record.
//
// NO CONSOLE LITERAL IS USED AS A HOST VALUE ANYWHERE BELOW (AGENTS gotcha 1). Every X360
// offset, stride and record size quoted above lives in a comment; the C++ reaches its data by
// member name and by the committed accessors only. The ONLY numeric literal in the code is
// the 1/3 rodata float (and the axis count 3); the two graphics ids now live inside the
// header's HACKShouldMoveComOffset.
// =========================================================================================

// Include set: rwvol.owner.md §5, verbatim.
#include "GameSource/Physics/PropManager/BrnPropManager.h"        // PropManager, K_LAMPOST_INERTIA_BOX
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"   // PropTypeData, PropPartTypeData
                                                                  //   (this already pulls
                                                                  //    volume_debug_access.h, so
                                                                  //    Volume::GetBBox is in scope)
// NOT included: vendor/renderware/collision/AABBox.hpp. It pulls SDKs/EATech/include/rw/math/vpu/
// vector3.h (the SDK Vector3 CLASS) into namespace rw::math::vpu, which this TU already has as
// the vendor 4-lane POD via BrnCommonTypes.h -- the two cannot coexist in one TU (measured,
// rwvol.owner.md s4.5; 8 of 11 AABBox.hpp consumers break under the naive repoint). What
// this TU needs from AABBox is ONLY its byte image: two 16-byte float4 rows, mMin @+0x00 and
// mMax @+0x10 (AABBox.hpp:  Vector3 mMin; Vector3 mMax;  over a 16-byte VectorIntrinsic).
// So GetBBox's out-parameter is an opaque 32-byte 16-aligned local viewed as AABBox& (the
// class is only forward-declared here), and the lanes are read as f32[8]. That layout is
// PINNED by PropManager_wQ4_03_embed_check.cpp (a separate TU that CAN include AABBox.hpp:
// sizeof(AABBox)==32, offsetof(mMax)==16, offsetof(mMin.mV)==0), so a change to AABBox
// breaks the gate rather than this reader. LANDED 2026-08-18 (wave Q4 integration) from
// scratchpad/waveQ2/parked/PropManager_09_GetPropInertia_GetPartInertia.cpp; the code below
// is that body with the two AABBox locals expressed through the opaque image.
namespace rw { namespace collision { class AABBox; } }
                                                                  //   <-- ONLY after rwvol.owner.md §4
#include <cmath>                                                  // fabsf (the vandc sign-bit clear)

namespace BrnPhysics
{
namespace Props
{
namespace
{
    // -------------------------------------------------------------------------------------
    // The two per-lane selects the emission performs, written so their NaN behaviour is the
    // console's. Operand order is preserved from the asm, so the tie/NaN winner is preserved.
    //   MaxLane: `vcmpgtfp. v0, v12(accum), v0(vol)`  @0x82612780 -> TRUE picks the accumulator.
    //   MinLane: `vcmpgtfp. v0, v11(vol), v12(accum)` @0x82612848 -> TRUE picks the accumulator.
    // AUTHORED names (see the INFERRED block).
    // -------------------------------------------------------------------------------------
    inline f32 MaxLane( f32 lfAccumulated, f32 lfCandidate )
    {
        return ( lfAccumulated > lfCandidate ) ? lfAccumulated : lfCandidate;
    }

    inline f32 MinLane( f32 lfCandidate, f32 lfAccumulated )
    {
        return ( lfCandidate > lfAccumulated ) ? lfAccumulated : lfCandidate;
    }

    // The final `vmaxfp v13, v12, v13` -- AltiVec vmaxfp is `(a > b) ? a : b`, i.e. the same
    // ordered compare, so it reduces the same way.
    inline f32 MaxFp( f32 lfA, f32 lfB )
    {
        return ( lfA > lfB ) ? lfA : lfB;
    }

    // Vector3 is four adjacent f32 lanes {x,y,z,w} (rw/math/vpu/types.h:24) and lane 0/1/2 ==
    // x/y/z. This is the re-roll of the source's own VectorAxisX/Y/Z templates, not a layout
    // guess.
    inline f32&       Lane( Vector3& lrVector, u32 luAxis )       { return ( &lrVector.x )[ luAxis ]; }
    inline const f32& Lane( const Vector3& lrVector, u32 luAxis ) { return ( &lrVector.x )[ luAxis ]; }

    static const u32 KU_AXIS_COUNT = 3u;

    // flt_820065E0 == 0.33333334f (0x3EAAAAAB) -- the reciprocal the compiler folded the
    // source's `/ 3.0f` into (the DWARF's three rw::math::vpu::operator/ calls). Written as
    // the emitted MULTIPLY because that, not an exact division by three, is what the shipped
    // image computes. 1/3 rather than 1/12 is correct precisely because the dimensions below
    // are HALF-extents: (m/12)*(2h)^2 == (m/3)*h^2.
    static const f32 KF_BOX_INERTIA_SCALE = 0.33333334f;             // flt_820065E0

    // -------------------------------------------------------------------------------------
    // The shared half of both bodies: walk a volume run under an identity transform,
    // accumulate the per-lane extremes of every volume's bounding box, and reduce that to the
    // per-axis half-extent the inertia fold consumes. Outlined as a PRESENTATION choice --
    // the two emissions are instruction-for-instruction identical here and only the accessor
    // supplying the count and the volumes differs.
    // -------------------------------------------------------------------------------------
    // The byte image of rw::collision::AABBox (mMin @+0x00, mMax @+0x10, 16-byte float4 rows)
    // over the vendor POD Vector3 this TU already speaks -- see the banner; layout pinned by
    // PropManager_wQ4_03_embed_check.cpp.
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;
        Vector3 mMax;
    };
    static_assert( sizeof( AABBoxRows ) == 32, "AABBox image is two 16-byte rows" );

    template< typename TTypeData >
    Vector3 AccumulateVolumeHalfExtents( const TTypeData* lpType )
    {
        // DWARF lAccumulatedAABBox. BOTH rows start at ZERO (`vspltisw128 v125,0` @0x82612668
        // then `vmr128 v126,v125` @0x82612674 / `vmr128 v127,v125` @0x8261267C) -- measured,
        // and load-bearing.
        AABBoxRows lAccumulatedAABBox;            // DWARF lAccumulatedAABBox (the AABBox image, see banner)
        lAccumulatedAABBox.mMin.SetZero();
        lAccumulatedAABBox.mMax.SetZero();

        // DWARF lu8Vol -- a u8 induction variable (the emission re-masks it `clrlwi r30,r11,24`
        // @0x826128FC every iteration, so the wrap behaviour is a u8's). The count is RE-READ
        // from the type record every iteration, exactly as the console does
        // (`lbz ...,0x5E(r29)` @0x82612678 and again @0x826128F8; GetPartInertia
        // `lbz ...,0x2C(r29)` @0x82612B00 and again @0x82612D80) -- deliberately NOT hoisted
        // into a loop-invariant local.
        for ( u8 lu8Vol = 0u; lu8Vol < lpType->GetNumberOfVolumes(); ++lu8Vol )
        {
            // DWARF lIdentity, rebuilt every iteration (the four `stvx128` at the top of the
            // loop body, @0x82612720/0x82612738/0x82612744/0x8261274C). Three rdata rows
            // {1,0,0,0} / {0,1,0,0} / {0,0,1,0} plus a zero row -- exactly SetIdentity(),
            // whose wAxis is {0,0,0,0}.
            Matrix44Affine lIdentity;
            lIdentity.SetIdentity();

            // DWARF lpVolume / lVolumeAABBox. Dispatched through the rwcollision per-TYPE
            // descriptor at volume+0x40, function pointer at descriptor+0x04
            // (`lwz r11,0x40(r3) ; lwz r11,4(r11) ; mtctr ; bctrl` @0x82612750..0x8261275C;
            // GetPartInertia @0x82612BD8..0x82612BE4). The `1` is the RwBool `tight` flag
            // (`li r5,1` @0x8261271C / @0x82612BA4); the RwBool result in r3 is not tested by
            // either caller.
            AABBoxRows                 lVolumeAABBox;   // DWARF lVolumeAABBox: GetBBox's 32-byte out-param
            ::rw::collision::Volume*   lpVolume = lpType->GetCollisionVolume( lu8Vol );
            lpVolume->GetBBox( &lIdentity, 1, *reinterpret_cast< ::rw::collision::AABBox* >( &lVolumeAABBox ) );

            // DWARF lMax (:2699/:2762) and lMin (:2700/:2763). Six unrolled reference selects
            // in the emission -- three Max<VecFloatRef{X,Y,Z}> against the volume box's MAX
            // row and three Min<VecFloatRef{X,Y,Z}> against its MIN row -- re-rolled here into
            // one axis loop. (The console updates the accumulator lane by lane in place and
            // re-splats from the UPDATED accumulator; the snapshot below is equivalent
            // because no lane reads another lane.)
            Vector3 lMax = lAccumulatedAABBox.mMax;
            Vector3 lMin = lAccumulatedAABBox.mMin;

            for ( u32 luAxis = 0u; luAxis < KU_AXIS_COUNT; ++luAxis )
            {
                Lane( lMax, luAxis ) = MaxLane( Lane( lMax, luAxis ),
                                                Lane( lVolumeAABBox.mMax, luAxis ) );
                Lane( lMin, luAxis ) = MinLane( Lane( lVolumeAABBox.mMin, luAxis ),
                                                Lane( lMin, luAxis ) );
            }

            lAccumulatedAABBox.mMax = lMax;
            lAccumulatedAABBox.mMin = lMin;
        }

        // DWARF lDims (:2712/:2775): per axis, Max( Abs(min_i), Abs(max_i) ). The emission is
        // `vandc` against 0x80000000 (a sign-bit clear, i.e. fabsf) followed by `vmaxfp`.
        // ⚠️ THERE IS NO (max - min) HERE -- neither body emits a single `vsubfp` (grepped,
        // zero hits in both dumps). This is a half-extent about the ORIGIN, which is what
        // pairs with the 1/3 scale below.
        Vector3 lDims;
        lDims.SetZero();
        for ( u32 luAxis = 0u; luAxis < KU_AXIS_COUNT; ++luAxis )
        {
            Lane( lDims, luAxis ) = MaxFp( fabsf( Lane( lAccumulatedAABBox.mMax, luAxis ) ),
                                           fabsf( Lane( lAccumulatedAABBox.mMin, luAxis ) ) );
        }

        return lDims;
    }

    // -------------------------------------------------------------------------------------
    // The solid-box inertia fold, shared by both bodies verbatim (DWARF lInertia):
    //     lInertia.x = (d.y^2 + d.z^2) / 3;    lane x, vrlimi mask 8
    //     lInertia.y = (d.x^2 + d.z^2) / 3;    lane y, mask 4
    //     lInertia.z = (d.x^2 + d.y^2) / 3;    lane z, mask 2
    //     lInertia  *= lfMass;                 DWARF operator*=; `vmulfp128 v0,v11,v7`
    // Lane->axis read off the vrlimi128 masks @0x82612A90/0x82612A94/0x82612A98
    // (GetPartInertia 0x82612ED0/0x82612ED8/0x82612EDC).
    // -------------------------------------------------------------------------------------
    Vector3 FoldBoxInertia( const Vector3& lrDims, f32 lfMass )
    {
        const f32 lfXSq = lrDims.x * lrDims.x;
        const f32 lfYSq = lrDims.y * lrDims.y;
        const f32 lfZSq = lrDims.z * lrDims.z;

        Vector3 lInertia;
        lInertia.x = ( lfYSq + lfZSq ) * KF_BOX_INERTIA_SCALE;
        lInertia.y = ( lfXSq + lfZSq ) * KF_BOX_INERTIA_SCALE;
        lInertia.z = ( lfXSq + lfYSq ) * KF_BOX_INERTIA_SCALE;

        lInertia.x *= lfMass;
        lInertia.y *= lfMass;
        lInertia.z *= lfMass;

        // ⚠️ INFERRED, not measured -- see the W-lane note in the banner. The console's W lane
        // is `<stale var_C0> * lfMass` (the vector is loaded ONE instruction before the mass is
        // stored into that slot in GetPropInertia @0x82612A44/0x82612A48, and two instructions
        // before it in GetPartInertia @0x82612E74/0x82612E7C), so it is undefined in the
        // shipped build. Zeroed here; nothing reads it.
        lInertia.w = 0.0f;

        return lInertia;
    }
}

// =========================================================================================
// BrnPhysics::Props::PropManager::GetPropInertia @ 0x82612640   (289 instructions)
// DWARF BrnPropManager.h:222, definition at BrnPropManager.cpp:2678.
//
// Only caller: AddPropToSim @0x826274D8 (single xref; the call is at 0x826275EC), which then
// scales the result by KVF_INERTIA_SCALE @0x82627604 -- that scale is NOT applied here.
// =========================================================================================
Vector3 PropManager::GetPropInertia( const PropTypeData* lpType )
{
    // Volume count u8 @+0x5E, volume run @+0x3C, both via the committed accessors.
    Vector3 lDims = AccumulateVolumeHalfExtents( lpType );

    // The inlined PropTypeData::HACKShouldMoveComOffset() (DWARF BrnPhysicsPropTypeData.h:110;
    // landed as a header inline at BrnPhysicsPropTypeData.h:222). `lwz r11,0x58(r29)`
    // @0x82612924 == muSceneUriId, compared against 0x6894C @0x82612948 then 0x68964
    // @0x826129A0; on a match the accumulated dimensions are DISCARDED and replaced wholesale
    // by the rdata vector at unk_82FB9420 (`lvx128 v0,r0,r11` @0x826129C4). It is NOT
    // IsLamppost(), which tests eight ids -- do not unify them.
    //
    // ⚠️ K_LAMPOST_INERTIA_BOX reads ALL-ZERO out of the shipped image, so on the console these
    // two prop types get a ZERO inertia here. Faithful, not a bug in this body.
    if ( lpType->HACKShouldMoveComOffset() )
    {
        lDims = K_LAMPOST_INERTIA_BOX;
    }

    // `lfs f0, 0x38(r29)` @0x82612A28 == mfMass.
    return FoldBoxInertia( lDims, lpType->GetMass() );
}

// =========================================================================================
// BrnPhysics::Props::PropManager::GetPartInertia @ 0x82612AC8   (272 instructions)
// DWARF BrnPropManager.h:226, definition at BrnPropManager.cpp:2740.
//
// The same function as GetPropInertia modulo exactly four things, all checked against the raw
// asm rather than assumed from the sibling:
//   * the volume count is the u8 at PropPartTypeData +0x2C (`lbz r11,0x2C(r29)` @0x82612B00
//     and again @0x82612D80) instead of +0x5E;
//   * the volume run is at +0x24 (`lwz r10,0x24(r29)` @0x82612B98) instead of +0x3C;
//   * the mass is at +0x20 (`lfs f0,0x20(r29)` @0x82612E38) instead of +0x38;
//   * and THERE IS NO HACKShouldMoveComOffset / lamppost branch -- grepped the whole dump: no
//     `0x58(r29)` load, no 0x6894C/0x68964 immediate, no unk_82FB94xx reference anywhere.
// Same hidden-pointer return, same unread `this`. Every offset above is reached through the
// committed PropPartTypeData accessors, so no console offset survives into the host code.
// =========================================================================================
Vector3 PropManager::GetPartInertia( const PropPartTypeData* lpType )
{
    const Vector3 lDims = AccumulateVolumeHalfExtents( lpType );

    // `lfs f0, 0x20(r29)` @0x82612E38 == mfMass.
    return FoldBoxInertia( lDims, lpType->GetMass() );
}

}   // namespace Props
}   // namespace BrnPhysics
