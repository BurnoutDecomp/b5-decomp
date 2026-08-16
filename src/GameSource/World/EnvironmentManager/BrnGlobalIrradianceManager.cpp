#include "GameSource/World/EnvironmentManager/BrnGlobalIrradianceManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <math.h>                                    // sqrtf (rw::math::vpu::Normalize)

// =============================================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte-matching):
//
//   BrnWorld::GlobalIrradianceManager::ComputeIrradiance        @0x827CA978  DWARF h:54
//   BrnWorld::GlobalIrradianceManager::ComputeFrameCoeffs       @0x827C5188  DWARF h:87 / cpp:211
//   BrnWorld::GlobalIrradianceManager::UpdateCoefficients       @0x827BEF70  DWARF h:71 / cpp:304
//   BrnWorld::GlobalIrradianceManager::ComputeIrradianceMatrix  @0x827B1160  DWARF h:77 / cpp:266
//   BrnWorld::GlobalIrradianceManager::GetIrradianceMatrix      @0x827B0500  DWARF h:57
//
// WHAT THE CHAIN IS. EnvironmentManager::GenerateShaderConstants @0x827D0098 is the ONLY caller
// of ComputeIrradiance (grep proof in the group report). It hands over the key-light direction
// and the blended keyframe's six fill colours, each pre-multiplied by
// LightingData::mfAmbientIrradianceScale (`lfs f0, 0x670(r31)` splatted and vmulfp'd into every
// colour at 0x827D05C0..0x827D061C). ComputeIrradiance then:
//   1. ComputeFrameCoeffs -- build the fill rig (three orthogonal axes derived from the key
//      light) and project the six colours onto the nine order-2 SH basis functions;
//   2. ComputeIrradianceMatrix x3 -- fold the nine coefficients into one 4x4 quadric per colour
//      channel, so that E(n) = n^T M n with n = (nx, ny, nz, 1);
//   3. cache M[3][3] of each channel as mAverageIrradianceColour (the n-independent term).
// WorldModule::SetupShaderConstantsBeforeRendering then repacks the three matrices into the two
// shader quadrics c18/c19 (sub_827B0790, the `worldconst` group).
//
// ---------------------------------------------------------------------------------------------
// THE TWELVE CONSTANTS -- WHERE THEY COME FROM (the load-bearing find of this TU)
// ---------------------------------------------------------------------------------------------
// The console keeps them as twelve `const VecFloat` file-scope globals -- DWARF
// references/DecFIGS/dwarfdump/.../BrnGlobalIrradianceManager.cpp lines 37-51 names every one:
//   KF_UPDATE_COEFF_C1..C5 (:37-41), KF_ONE (:43), KF_THREE (:44),
//   KF_COMPUTE_MATRIX_C1..C5 (:47-51).
// A VecFloat is a broadcast 16-byte lane register, so each one lives in .data and is filled by a
// CRT dynamic initialiser; the twelve 40-byte initialiser blocks sit back to back at
// 0x82C6AA60, AA88, AAB0, AAD8, AB00, AB28, AB50, AB78, ABA0, ABC8, ABF0, AC18 (exactly 0x28
// apart, in source order), and the twelve .data slots they write are, in that same order:
//   0x8300F600 0x8300F3A0 0x8300F4C0 0x8300FA60 0x8300F650 0x8300F420
//   0x8300E9E0 0x8300E210 0x8300EA60 0x8300FB10 0x8300F5F0 0x8300FAB0
// (from the DATA_DUMP xrefs_to of each slot). Their IMAGE BYTES ARE ZERO -- they are runtime
// tables -- so the values below are NOT read out of the image. They are recovered three ways
// that all agree:
//
//   (a) ALGEBRAIC FORM. UpdateCoefficients' seventh basis term is literally
//       `unk_8300FA60 * (unk_8300E9E0 * z*z - unk_8300F420)` (0x827BF05C-0x827BF098) and its
//       ninth is `unk_8300F650 * (x*x - y*y)` (0x827BF09C-0x827BF0B0); ComputeIrradianceMatrix's
//       last element is `unk_8300F5F0 * c[0] - unk_8300FAB0 * c[6]` (0x827B1458-0x827B1494).
//       Those are Y20 = C4*(3z^2 - 1), Y22 = C5*(x^2 - y^2) and M[3][3] = c4*L00 - c5*L20.
//   (b) THE DWARF NAMES. KF_ONE and KF_THREE are *named* 1 and 3, and the source-order mapping
//       above independently lands KF_ONE on 0x8300F420 and KF_THREE on 0x8300E9E0 -- which is
//       exactly the pair (a) requires. Two independent derivations agreeing pins the whole
//       name<->address table.
//   (c) NUMERIC ORACLE. Feeding the shipped noon keyframe ENV_KF_Paradise_ingame_junk_city_1200
//       (build/game/ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE, embedded verbatim in
//       BrnEnvironmentKeyframeBringUp.cpp) -- its six LightingData fills at keyframe
//       +0x1A0/+0x1B0/+0x180/+0x190/+0x160/+0x170, x mfAmbientIrradianceScale = 0.4 (+0x1C0) --
//       and the bring-up key-light direction (0.406, -0.812, 0.419) through these four bodies
//       reproduces ALL 21 numbers WorldModule::PublishWorldShadingConstantsBringUp hard-codes
//       for IrradianceQuadricA/B (BrnWorldModule.cpp:4317-4325) to the last printed digit,
//       including the two ~1e-4 terms 0.000131 and -0.000384 that no wrong constant survives.
//       The harness + its output are in the group report.
//
// They are the published Ramamoorthi & Hanrahan (SIGGRAPH 2001) constants:
//   UPDATE_COEFF   = the real SH basis normalisers  0.282095 / 0.488603 / 1.092548 /
//                    0.315392 / 0.546274
//   COMPUTE_MATRIX = the irradiance-matrix coefficients c1..c5
//                    0.429043 / 0.511664 / 0.743125 / 0.886227 / 0.247708
// [FLAG] The twelve initialiser blocks themselves were NOT disassembled (the region
// 0x82C6AA08..0x82C6AC48 has no exported function -- IDA never made one there), so the byte
// image of each source float is unconfirmed; see the CONDUCTOR DUMP REQUEST in the report.
//
// ---------------------------------------------------------------------------------------------
// PC DEVIATIONS
// ---------------------------------------------------------------------------------------------
// * The console bodies are pure VMX: every scalar is a broadcast lane register, every component
//   read is an `lvsl`-built splat, and every matrix element write is a `vperm` insert through the
//   rw::math::vpu 4x4 component-mask table at 0x8327F140. The tree's Vector3/Matrix44 are the
//   portable POD lane structs (vendor/renderware/include/rw/math/vpu/types.h), so all of that
//   lowers to named-member float math -- the same de-optimisation CgsTriangleBox.cpp and
//   CgsPolygonSoupTests.cpp already use. NO permute-mask table is reconstructed here: it is MSVC
//   codegen for `Matrix44::GetElem(i,j) = x` / `Vector3::Set(x,y,z)`, which the DWARF .cpp dump
//   names explicitly, not data the algorithm reads.
// * `Normalize` is the SDK's full-precision normalise (the console spells it vrsqrtefp + two
//   Newton-Raphson refinements at 0x827C5310-0x827C5334 and 0x827C5350-0x827C5374); sqrtf here.
// * The twelve constants are file-local `const f32` rather than external `const VecFloat`: they
//   are only ever multiplied lane-wise against a splat, so the broadcast is redundant once the
//   math is scalar, and internal linkage keeps them out of the link.
// =============================================================================================

namespace BrnWorld
{

namespace
{
    // --- DWARF BrnGlobalIrradianceManager.cpp:37-41 -- the order-2 real SH basis normalisers.
    // Console: const VecFloat, .data slots 0x8300F600 / F3A0 / F4C0 / FA60 / F650.
    const f32 KF_UPDATE_COEFF_C1 = 0.28209479f; // Y00                     = C1  (image word 3E906EC1 @flt_820CD6FC, DUMP_REQUESTS.md)
    const f32 KF_UPDATE_COEFF_C2 = 0.488603f;   // Y1-1 / Y10 / Y11        = C2 * {y, z, x}
    const f32 KF_UPDATE_COEFF_C3 = 1.092548f;   // Y2-2 / Y2-1 / Y21       = C3 * {xy, yz, xz}
    const f32 KF_UPDATE_COEFF_C4 = 0.315392f;   // Y20                     = C4 * (3z^2 - 1)
    const f32 KF_UPDATE_COEFF_C5 = 0.546274f;   // Y22                     = C5 * (x^2 - y^2)

    // --- DWARF :43-44. Console: .data slots 0x8300F420 (ONE) and 0x8300E9E0 (THREE); they are
    // separate named globals because on VMX they have to be materialised as lane registers.
    const f32 KF_ONE   = 1.0f;
    const f32 KF_THREE = 3.0f;

    // --- DWARF :47-51 -- Ramamoorthi's c1..c5. Console: .data slots
    // 0x8300E210 / EA60 / FB10 / F5F0 / FAB0.
    const f32 KF_COMPUTE_MATRIX_C1 = 0.429043f;
    const f32 KF_COMPUTE_MATRIX_C2 = 0.511664f;
    const f32 KF_COMPUTE_MATRIX_C3 = 0.743125f;
    const f32 KF_COMPUTE_MATRIX_C4 = 0.886227f;
    const f32 KF_COMPUTE_MATRIX_C5 = 0.247708f;

    // ComputeFrameCoeffs' two sanity bounds on the key light. Console: flt_820224B0 (0.99f,
    // 0x3F7D70A4) and flt_820CC338 (-0.99f, 0xBF7D70A4); the assert strings name the value.
    // INFERRED name (the constant is unnamed in the DWARF).
    const f32 KF_KEY_LIGHT_Y_LIMIT = 0.99f;

    // rw::math::vpu::Normalize, lowered. The console does vmsum3fp128 (dot3) + vrsqrtefp with
    // two Newton-Raphson steps; the refined reciprocal square root is full float precision.
    inline Vector3 Normalize( const Vector3& lrV )
    {
        const f32 lfLenSq = lrV.x * lrV.x + lrV.y * lrV.y + lrV.z * lrV.z;
        const f32 lfInvLen = 1.0f / sqrtf( lfLenSq );
        Vector3 lResult;
        lResult.x = lrV.x * lfInvLen;
        lResult.y = lrV.y * lfInvLen;
        lResult.z = lrV.z * lfInvLen;
        lResult.w = lrV.w * lfInvLen;   // the console multiplies the whole register
        return lResult;
    }

    // rw::math::vpu::Cross, lowered (the console builds it out of two YZX vpermwi128 + a
    // vnmsubfp at 0x827C52F8-0x827C5348, then re-permutes -- the classic 3-op VMX cross).
    inline Vector3 Cross( const Vector3& lrA, const Vector3& lrB )
    {
        Vector3 lResult;
        lResult.x = lrA.y * lrB.z - lrA.z * lrB.y;
        lResult.y = lrA.z * lrB.x - lrA.x * lrB.z;
        lResult.z = lrA.x * lrB.y - lrA.y * lrB.x;
        lResult.w = 0.0f;
        return lResult;
    }

    // Unary minus. The console flips the sign bit of all four lanes
    // (`vspltisw v7,-1 ; vslw v0,v7,v7` builds 0x80000000, then vxor128).
    inline Vector3 Negate( const Vector3& lrV )
    {
        Vector3 lResult;
        lResult.x = -lrV.x;
        lResult.y = -lrV.y;
        lResult.z = -lrV.z;
        lResult.w = -lrV.w;
        return lResult;
    }

    // rw::math::vpu::Vector3::operator[] -- the console splats lane `liChannel` with an
    // lvsl(4*liChannel)-derived permute control (0x827B1168 `slwi r10, r4, 2`, then
    // `lvsl v0,0,r10 ; vspltw v,v0,0 ; vperm`).
    inline f32 Component( const Vector3& lrV, s32 liChannel )
    {
        return ( liChannel == 0 ) ? lrV.x
             : ( liChannel == 1 ) ? lrV.y
                                  : lrV.z;
    }

    // One `vmaddfp` of the accumulate loop: coeff += colour * basis, all four lanes.
    inline void AccumulateCoefficient( Vector3& lrCoeff, const Vector3& lrColour, f32 lfBasis )
    {
        lrCoeff.x += lrColour.x * lfBasis;
        lrCoeff.y += lrColour.y * lfBasis;
        lrCoeff.z += lrColour.z * lfBasis;
        lrCoeff.w += lrColour.w * lfBasis;
    }
}

// ---------------------------------------------------------------------------------------------
// UpdateCoefficients @0x827BEF70
//
// One directional radiance sample folded into the nine coefficients. Straight-line VMX, no
// branches: three splats of the direction (`vspltw v12/v0/v13, v2, 0/1/2` = x/y/z), then nine
// `lvx coeff ; <build basis> ; vmaddfp ; stvx coeff` groups at this+0xD0, +0xE0, +0xF0, +0x100,
// +0x110, +0x120, +0x140, +0x130, +0x150 (note the asm emits index 7 before index 6, purely a
// scheduling artefact -- both write their own slot).
//
// ORIENTATION. `lVector` is the direction the light arrives FROM, not the direction it travels.
// Proof from the shipped data rather than from convention: ComputeFrameCoeffs pairs the fill at
// LightingData+0x60 with (0,-1,0) and the fill at +0x70 with (0,+1,0), and in the noon keyframe
// those two colours are (0.256, 0.247, 0.231) -- a dark ground bounce -- and
// (0.693, 0.838, 0.895) -- sky blue. Only the "arrives from" reading puts the blue above.
//
// [FLAG] IDA prints the VA-form operands in ENCODING order (vD, vA, vB, vC) while the PPC
// mnemonic is `vmaddfp vD,vA,vC,vB`; i.e. `vmaddfp v11, v1, v10, v11` is v11 = v1*v11 + v10 =
// colour*basis + oldCoeff, NOT colour*oldCoeff + basis. Cross-checked on the vnmsubfp/vmaddfp
// pair inside ComputeFrameCoeffs' Normalize, where only this reading yields the textbook
// Newton-Raphson step y' = y + 0.5*y*(1 - x*y*y).
// ---------------------------------------------------------------------------------------------
void
GlobalIrradianceManager::UpdateCoefficients( const Vector3& lColour, const Vector3& lVector )
{
    const f32 lfX = lVector.x;
    const f32 lfY = lVector.y;
    const f32 lfZ = lVector.z;

    // The nine order-2 real spherical harmonics evaluated at lVector, in the storage order the
    // matrix builder expects (L00, L1-1, L10, L11, L2-2, L2-1, L20, L21, L22).
    const f32 lafBasis[E_NUM_COEFFS] =
    {
        KF_UPDATE_COEFF_C1,
        KF_UPDATE_COEFF_C2 * lfY,
        KF_UPDATE_COEFF_C2 * lfZ,
        KF_UPDATE_COEFF_C2 * lfX,
        KF_UPDATE_COEFF_C3 * lfX * lfY,
        KF_UPDATE_COEFF_C3 * lfY * lfZ,
        KF_UPDATE_COEFF_C4 * ( KF_THREE * lfZ * lfZ - KF_ONE ),
        KF_UPDATE_COEFF_C3 * lfX * lfZ,
        KF_UPDATE_COEFF_C5 * ( lfX * lfX - lfY * lfY ),
    };

    for ( s32 liCoeff = 0; liCoeff < E_NUM_COEFFS; ++liCoeff )
    {
        AccumulateCoefficient( maFrameIrradianceCoeffs[liCoeff], lColour, lafBasis[liCoeff] );
    }
}

// ---------------------------------------------------------------------------------------------
// ComputeFrameCoeffs @0x827C5188
//
// Builds the fill rig and projects the six authored fill colours onto the SH basis.
//   lUpDirection      = (0, 1, 0)                                    -- literal, cpp:215
//   lSunFillDirection = Normalize(keyLight.x, 0, keyLight.z)         -- cpp:220
//   lRightDirection   = Normalize(Cross(lUpDirection, lSunFillDirection))  -- cpp:225
// then the coefficients are cleared (memset(this+0xD0, 0, 0x90) at 0x827C5378) and the six
// samples are pushed in the console's own order, each axis once positive and once negated.
//
// The two asserts are the X360's, verbatim -- their file/line in the console image is
// BrnGlobalIrradianceManager.cpp:218 / :219 (`li r5, 0xDA` / `0xDB`); CGS_ASSERT injects the PC
// __FILE__/__LINE__ instead, exactly as every other reconstructed assert in this tree does.
// They exist because a key light within 0.99 of straight up/down makes the horizontal
// projection degenerate and Normalize would divide by ~0.
// ---------------------------------------------------------------------------------------------
void
GlobalIrradianceManager::ComputeFrameCoeffs( const Vector3& lKeyLightDirection,
                                             const Vector3& lSunFillColour,
                                             const Vector3& lShadowFillColour,
                                             const Vector3& lRightFillColour,
                                             const Vector3& lLeftFillColour,
                                             const Vector3& lUpFillColour,
                                             const Vector3& lDownFillColour )
{
    CGS_ASSERT( lKeyLightDirection.y <  KF_KEY_LIGHT_Y_LIMIT, "lKeyLightDirection.Y() < 0.99f" );
    CGS_ASSERT( lKeyLightDirection.y > -KF_KEY_LIGHT_Y_LIMIT, "lKeyLightDirection.Y() > -0.99f" );

    const Vector3 lUpDirection = { 0.0f, 1.0f, 0.0f, 0.0f };

    Vector3 lSunFillDirection = { lKeyLightDirection.x, 0.0f, lKeyLightDirection.z, 0.0f };
    lSunFillDirection = Normalize( lSunFillDirection );

    const Vector3 lRightDirection = Normalize( Cross( lUpDirection, lSunFillDirection ) );

    for ( s32 liCoeff = 0; liCoeff < E_NUM_COEFFS; ++liCoeff )
    {
        maFrameIrradianceCoeffs[liCoeff].SetZero();
    }

    UpdateCoefficients( lSunFillColour,    Negate( lSunFillDirection ) );
    UpdateCoefficients( lShadowFillColour, lSunFillDirection );
    UpdateCoefficients( lUpFillColour,     Negate( lUpDirection ) );
    UpdateCoefficients( lDownFillColour,   lUpDirection );
    UpdateCoefficients( lRightFillColour,  Negate( lRightDirection ) );
    UpdateCoefficients( lLeftFillColour,   lRightDirection );
}

// ---------------------------------------------------------------------------------------------
// ComputeIrradianceMatrix @0x827B1160
//
// The Ramamoorthi & Hanrahan irradiance quadric for one colour channel, element for element as
// the asm writes it (row 0 words 0..3, row 1 words 0..3, row 2 words 0..3, row 3 words 0..3 --
// all sixteen are written, so assigning whole rows is equivalent to the vperm inserts):
//
//   [ c1*L22   c1*L2-2  c1*L21   c2*L11         ]
//   [ c1*L2-2 -c1*L22   c1*L2-1  c2*L1-1        ]
//   [ c1*L21   c1*L2-1  c3*L20   c2*L10         ]
//   [ c2*L11   c2*L1-1  c2*L10   c4*L00-c5*L20  ]
//
// so that E(n) = [n 1] * M * [n 1]^T. The sign flip on M[1][1] is the console's
// `vspltisw v0,-1 ; vslw v0,v0,v0 ; vxor v0,v12,v0` at 0x827B126C-0x827B12BC (build 0x80000000,
// xor it into KF_COMPUTE_MATRIX_C1) -- a negation, not a different constant.
// `this` is unused; the console overwrites r3 at 0x827B1170.
// ---------------------------------------------------------------------------------------------
void
GlobalIrradianceManager::ComputeIrradianceMatrix( s32 liChannel,
                                                  Vector3* lpIrradianceCoeffs,
                                                  Matrix44& lrMatrixOut )
{
    const f32 lfL00  = Component( lpIrradianceCoeffs[0], liChannel );
    const f32 lfL1m1 = Component( lpIrradianceCoeffs[1], liChannel );
    const f32 lfL10  = Component( lpIrradianceCoeffs[2], liChannel );
    const f32 lfL11  = Component( lpIrradianceCoeffs[3], liChannel );
    const f32 lfL2m2 = Component( lpIrradianceCoeffs[4], liChannel );
    const f32 lfL2m1 = Component( lpIrradianceCoeffs[5], liChannel );
    const f32 lfL20  = Component( lpIrradianceCoeffs[6], liChannel );
    const f32 lfL21  = Component( lpIrradianceCoeffs[7], liChannel );
    const f32 lfL22  = Component( lpIrradianceCoeffs[8], liChannel );

    lrMatrixOut.xAxis.x =  KF_COMPUTE_MATRIX_C1 * lfL22;
    lrMatrixOut.xAxis.y =  KF_COMPUTE_MATRIX_C1 * lfL2m2;
    lrMatrixOut.xAxis.z =  KF_COMPUTE_MATRIX_C1 * lfL21;
    lrMatrixOut.xAxis.w =  KF_COMPUTE_MATRIX_C2 * lfL11;

    lrMatrixOut.yAxis.x =  KF_COMPUTE_MATRIX_C1 * lfL2m2;
    lrMatrixOut.yAxis.y = -KF_COMPUTE_MATRIX_C1 * lfL22;
    lrMatrixOut.yAxis.z =  KF_COMPUTE_MATRIX_C1 * lfL2m1;
    lrMatrixOut.yAxis.w =  KF_COMPUTE_MATRIX_C2 * lfL1m1;

    lrMatrixOut.zAxis.x =  KF_COMPUTE_MATRIX_C1 * lfL21;
    lrMatrixOut.zAxis.y =  KF_COMPUTE_MATRIX_C1 * lfL2m1;
    lrMatrixOut.zAxis.z =  KF_COMPUTE_MATRIX_C3 * lfL20;
    lrMatrixOut.zAxis.w =  KF_COMPUTE_MATRIX_C2 * lfL10;

    lrMatrixOut.wAxis.x =  KF_COMPUTE_MATRIX_C2 * lfL11;
    lrMatrixOut.wAxis.y =  KF_COMPUTE_MATRIX_C2 * lfL1m1;
    lrMatrixOut.wAxis.z =  KF_COMPUTE_MATRIX_C2 * lfL10;
    lrMatrixOut.wAxis.w =  KF_COMPUTE_MATRIX_C4 * lfL00 - KF_COMPUTE_MATRIX_C5 * lfL20;
}

// ---------------------------------------------------------------------------------------------
// ComputeIrradiance @0x827CA978
//
// The public entry point. Note the console emits `addi r5, r31, 0xD0` only before the FIRST
// ComputeIrradianceMatrix call: that callee never writes r5, so the coefficient pointer survives
// into calls 2 and 3. All three therefore receive &maFrameIrradianceCoeffs[0].
//
// The tail is one splat-per-channel and a vperm/vrlimi128 merge that stores
// (M0[3][3], M1[3][3], M2[3][3], M0[3][3]) to this+0xC0 -- i.e. the DWARF's
// `mAverageIrradianceColour.Set(m0.GetElem(3,3), m1.GetElem(3,3), m2.GetElem(3,3))`, whose W
// lane the permute happens to leave holding the RED value. That W is never read (the only
// consumer splats channels 0..2), but it is reproduced here so the register matches the console.
// ---------------------------------------------------------------------------------------------
void
GlobalIrradianceManager::ComputeIrradiance( const Vector3& lKeyLightDirection,
                                            const Vector3& lSunFillColour,
                                            const Vector3& lShadowFillColour,
                                            const Vector3& lRightFillColour,
                                            const Vector3& lLeftFillColour,
                                            const Vector3& lTopFillColour,
                                            const Vector3& lBottomFillColour )
{
    ComputeFrameCoeffs( lKeyLightDirection, lSunFillColour, lShadowFillColour,
                        lRightFillColour, lLeftFillColour, lTopFillColour, lBottomFillColour );

    for ( s32 liColour = 0; liColour < E_NUM_COLOURS; ++liColour )
    {
        ComputeIrradianceMatrix( liColour, maFrameIrradianceCoeffs, maIrradianceMatrix[liColour] );
    }

    mAverageIrradianceColour.x = maIrradianceMatrix[0].wAxis.w;
    mAverageIrradianceColour.y = maIrradianceMatrix[1].wAxis.w;
    mAverageIrradianceColour.z = maIrradianceMatrix[2].wAxis.w;
    mAverageIrradianceColour.w = maIrradianceMatrix[0].wAxis.w;
}

// ---------------------------------------------------------------------------------------------
// GetIrradianceMatrix @0x827B0500  -- UNCHANGED from the committed reconstruction apart from the
// member rename (maIrradianceMatrices -> the DWARF's maIrradianceMatrix).
//
// The X360 body asserts the colour channel is in range, then copies the requested 64-byte
// irradiance matrix out with four lvx128/stvx128 (16-byte vector) loads/stores. Those vector ops
// are a straight memory copy of a Matrix44 (four 16-byte rows) -- there is no vmaddfp/vsel/permute
// math -- so they lower faithfully to a scalar struct copy here. Called by
// BrnWorld::EnvironmentSettings::EnvironmentManager::GenerateShaderConstants (three times, once
// per channel, at 0x827D0630 / 0x827D0680 / 0x827D06C4).
// ---------------------------------------------------------------------------------------------
Matrix44&
GlobalIrradianceManager::GetIrradianceMatrix( Matrix44& lrOutMatrix, u8 lu8Colour ) const
{
    // X360: if (lu8Colour >= 3u) Begin/Fire/EndAssert("lu8Colour < 3", "...h", 100);
    CGS_ASSERT( lu8Colour < E_NUM_COLOURS, "lu8Colour < 3" );

    // r11 = (lu8Colour << 6) + this  ->  &maIrradianceMatrix[lu8Colour]
    // four lvx128/stvx128 == copy the 64-byte Matrix44 row-for-row.
    lrOutMatrix = maIrradianceMatrix[lu8Colour];
    return lrOutMatrix;
}

}
