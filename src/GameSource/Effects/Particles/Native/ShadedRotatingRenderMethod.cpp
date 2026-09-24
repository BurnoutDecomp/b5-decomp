// ============================================================================
// GameSource/Effects/Particles/Native/ShadedRotatingRenderMethod.cpp
//
// BrnParticle::Native::ShadedRotatingRenderMethod::BuildQuad  @0x8291E150 (202 instructions)
//
// ⭐ 2026-09-24 (FX-CRASHVFX): BODIED. This replaces an asserting stub whose banner said the
// function had "no faithful scalar lowering". It has one: every VMX lane below is independent,
// the only cross-lane moves are four vspltw broadcasts and two vperm corner gathers through the
// rodata control word at 0x82CDA350 (00010203 14151617 00010203 00010203 == A.x, B.y, A.x, A.x),
// and every constant the body gathers is a CRT-initialised splat whose writer thunk states the
// value outright. The whole body -- and CB4ParticleBank::Render around it -- was executed
// instruction for instruction in a VMX emulator (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu) on
// seeded inputs; tests/FxCrashVfxSimpleRender.cpp checks this lowering against those vertices.
//
// WHAT IT BUILDS: the four corners of one particle's quad, ROTATED about the particle's projected
// centre by rotVel * age, and four corner colours SHADED by the same rotation -- the corner that
// currently faces "up" is lit toward lighting.max and the one facing down toward lighting.min, so a
// spinning puff of smoke looks lit from above. The rotation's sine and cosine are one polynomial
// evaluated on a triangle wave (a quarter-period apart in lanes 0 and 1):
//
//     x    = angle / 2pi + (-0.25, 0, -0.25, 0)     vmaddfp  (0x8307A680, 0x8307A590)
//     fr   = x - floor(x)                           vrfim / vsubfp
//     t    = min(fr, 1 - fr) - 0.25                 in [-0.25, 0.25]
//     s    = t * (t^2 * (t^2 * -71.17897 + 40.897369) + -6.2780428)
//          = -sin(2 pi t)  (minimax), i.e. s.x = sin(angle), s.y = cos(angle)
//
// CONSTANTS -- every one is a .bss splat filled by a CRT thunk; the thunk and the rodata float it
// splats are cited (tools/re/findinit.py + ppcdis.py):
//     0x8307A680  splat(flt_82001C90 = 0.15915494 = 1/2pi)    thunk 0x82C6EB00
//     0x8307A590  (flt_8200D56C, flt_82001CC0, flt_8200D56C, flt_82001CC0) = (-0.25, 0, -0.25, 0)
//                                                             thunk 0x82C6EBC8
//     0x8307A3C0  splat(flt_82001C98 = 1.0)                   thunk 0x82C6EB78
//     0x8307A560  splat(flt_8200D56C = -0.25)                 thunk 0x82C6EBA0
//     0x8307A670  splat(flt_820F101C = -71.17897)             thunk 0x82C6F150
//     0x8307A3B0  splat(flt_820F1018 = 40.897369)             thunk 0x82C6F128
//     0x8307A5F0  splat(flt_820F1014 = -6.2780428)            thunk 0x82C6F100
//     flt_82010C20 = 255.0                                    (rodata, read directly)
// ============================================================================

#include "GameSource/Effects/Particles/Native/ShadedRotatingRenderMethod.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"   // CB4Particle

#include <cmath>     // std::floor (vrfim)
#include <cstring>   // std::memcpy -- the byte-explicit RGBA8 word

namespace BrnParticle
{
namespace Native
{
    namespace
    {
        typedef rw::math::vpu::Vector4 Vector4;

        const f32 KF_ONE_OVER_TWO_PI   = 0.15915493667125702f;   // flt_82001C90 -> 0x8307A680
        const f32 KF_QUARTER           = 0.25f;                  // -flt_8200D56C -> 0x8307A560 / 0x8307A590
        const f32 KF_ONE               = 1.0f;                   // flt_82001C98 -> 0x8307A3C0
        const f32 KF_ZERO              = 0.0f;                   // flt_82001CC0
        const f32 KF_POLY_C5           = -71.17897033691406f;    // flt_820F101C -> 0x8307A670
        const f32 KF_POLY_C3           = 40.897369384765625f;    // flt_820F1018 -> 0x8307A3B0
        const f32 KF_POLY_C1           = -6.278042793273926f;    // flt_820F1014 -> 0x8307A5F0
        const f32 KF_COLOUR_TO_BYTE    = 255.0f;                 // flt_82010C20

        // vmaxfp / vminfp: a NaN operand propagates (AltiVec "the result is a QNaN").
        inline f32 VMax(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            return (lfA > lfB) ? lfA : lfB;
        }
        inline f32 VMin(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            return (lfA < lfB) ? lfA : lfB;
        }

        // vctuxs x, 0: truncate toward zero, saturate to [0, 2^32 - 1], NaN -> 0.
        inline u32 ConvertToUnsignedSaturate(f32 lfValue)
        {
            if (!(lfValue > 0.0f))
                return 0u;
            if (lfValue >= 4294967295.0f)
                return 0xFFFFFFFFu;
            return static_cast<u32>(lfValue);
        }

        // One corner colour: clamp to [0, 1] (vmaxfp against 0 then vminfp against 1.0), scale by
        // 255 (flt_82010C20, splatted through the stack), vctuxs, then the clrlwi/insrwi chain
        // that packs the four LOW BYTES into (R << 24) | (G << 16) | (B << 8) | A -- a big-endian
        // word whose memory image is [R][G][B][A]. The vertex declaration reads it as UBYTE4N, so
        // what matters is that MEMORY IMAGE; it is written byte by byte so the order is R,G,B,A on
        // either endianness (the same correction BrnSparkRenderer_Render.cpp's
        // VertexColourWordRGBA carries, for the same element word).
        inline u32 PackCornerColour(const Vector4& lrColour)
        {
            const f32 lafLanes[4] = { lrColour.x, lrColour.y, lrColour.z, lrColour.w };
            u8 lau8Bytes[4];
            for (u32 luLane = 0; luLane < 4; ++luLane)
            {
                const f32 lfClamped = VMin(VMax(lafLanes[luLane], KF_ZERO), KF_ONE);
                lau8Bytes[luLane] =
                    static_cast<u8>(ConvertToUnsignedSaturate(lfClamped * KF_COLOUR_TO_BYTE) & 0xFFu);
            }
            u32 luWord = 0;
            std::memcpy(&luWord, lau8Bytes, sizeof(luWord));
            return luWord;
        }

        // The rotation polynomial on one lane (see the banner).
        inline f32 RotationLane(f32 lfAngle, f32 lfPhase)
        {
            const f32 lfX        = lfAngle * KF_ONE_OVER_TWO_PI + lfPhase;
            const f32 lfFraction = lfX - std::floor(lfX);
            const f32 lfTriangle = VMin(lfFraction, KF_ONE - lfFraction) + (-KF_QUARTER);
            const f32 lfSquared  = lfTriangle * lfTriangle;
            const f32 lfInner    = lfSquared * KF_POLY_C5 + KF_POLY_C3;
            const f32 lfPoly     = lfSquared * lfInner + KF_POLY_C1;
            return lfPoly * lfTriangle;
        }

        inline Vector4 MakeVector(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            Vector4 lResult = { lfX, lfY, lfZ, lfW };
            return lResult;
        }
    }

    void ShadedRotatingRenderMethod::BuildQuad(rw::math::vpu::Vector4* lpaPositions,
                                               u32* lpauColours,
                                               const CB4Particle* lpParticle,
                                               const rw::math::vpu::Matrix44* /*lpViewMatrix*/,
                                               const rw::math::vpu::Vector4& lvPosition,
                                               const rw::math::vpu::Vector4& lvSize,
                                               const rw::math::vpu::Vector4& lvColour,
                                               const rw::math::vpu::Vector4& lvAge,
                                               const rw::math::vpu::Vector4& lvAlpha) const
    {
        // vrlimi128 v12, v5, 1, 0 -- the colour's alpha lane is REPLACED by the caller's alpha.
        Vector4 lColour = lvColour;
        lColour.w = lvAlpha.w;

        // v30 = splat(record.mVelocityRotation.w) * v4 (splat age); lanes 2/3 repeat lanes 0/1.
        const f32 lfRotationalVelocity = lpParticle->mVelocityRotation.GetPlus();
        const f32 lfAngle0 = lfRotationalVelocity * lvAge.x;
        const f32 lfAngle1 = lfRotationalVelocity * lvAge.y;

        const f32 lfSin = RotationLane(lfAngle0, -KF_QUARTER);   // lane 0 (phase -0.25)
        const f32 lfCos = RotationLane(lfAngle1, KF_ZERO);       // lane 1 (phase 0)

        // v11 = s * this->mvCornerScale (splat sqrt 2): the rotated half-diagonal.
        const f32 lfR0 = lfSin * mvCornerScale.x;
        const f32 lfR1 = lfCos * mvCornerScale.y;

        // The four corners, through the A.x/B.y/A.x/A.x gather at 0x82CDA350 with lane 2 then
        // zeroed (vrlimi128 ..., v13 = 0, 2, 0). c2 and c3 are c0 and c1 with EVERY lane's sign
        // flipped (vxor against the vslw-built 0x80000000 splat) -- so their z is -0.0.
        Vector4 laCorners[4];
        laCorners[0] = MakeVector(-lfR0, lfR1, KF_ZERO, -lfR0);
        laCorners[1] = MakeVector(lfR1, lfR0, KF_ZERO, lfR1);
        laCorners[2] = MakeVector(-laCorners[0].x, -laCorners[0].y, -laCorners[0].z, -laCorners[0].w);
        laCorners[3] = MakeVector(-laCorners[1].x, -laCorners[1].y, -laCorners[1].z, -laCorners[1].w);

        // positions[k] = corner_k * size + position (a vmulfp128 then a vaddfp, stored per corner).
        for (u32 luCorner = 0; luCorner < 4; ++luCorner)
        {
            const Vector4& lrCorner = laCorners[luCorner];
            lpaPositions[luCorner] = MakeVector(lrCorner.x * lvSize.x + lvPosition.x,
                                                lrCorner.y * lvSize.y + lvPosition.y,
                                                lrCorner.z * lvSize.z + lvPosition.z,
                                                lrCorner.w * lvSize.w + lvPosition.w);
        }

        // THE SHADING. range * splat(sin) and range * splat(cos) around the lighting base:
        //   colours[0] = col * (base + range*cos)   colours[1] = col * (base + range*sin)
        //   colours[2] = col * (base - range*cos)   colours[3] = col * (base - range*sin)
        // (0x8291E2E4..0x8291E330; the stores to 0/4/8/0xC(r5) at 0x8291E3F8 / E3B8 / E428 / E46C.)
        const Vector4& lrBase  = mvLightingBase;
        const Vector4& lrRange = mvLightingRange;
        const Vector4 lRangeSin = MakeVector(lrRange.x * lfSin, lrRange.y * lfSin,
                                             lrRange.z * lfSin, lrRange.w * lfSin);
        const Vector4 lRangeCos = MakeVector(lrRange.x * lfCos, lrRange.y * lfCos,
                                             lrRange.z * lfCos, lrRange.w * lfCos);

        const Vector4 laShade[4] =
        {
            MakeVector(lrBase.x + lRangeCos.x, lrBase.y + lRangeCos.y, lrBase.z + lRangeCos.z, lrBase.w + lRangeCos.w),
            MakeVector(lrBase.x + lRangeSin.x, lrBase.y + lRangeSin.y, lrBase.z + lRangeSin.z, lrBase.w + lRangeSin.w),
            MakeVector(lrBase.x - lRangeCos.x, lrBase.y - lRangeCos.y, lrBase.z - lRangeCos.z, lrBase.w - lRangeCos.w),
            MakeVector(lrBase.x - lRangeSin.x, lrBase.y - lRangeSin.y, lrBase.z - lRangeSin.z, lrBase.w - lRangeSin.w),
        };

        for (u32 luCorner = 0; luCorner < 4; ++luCorner)
        {
            const Vector4& lrShade = laShade[luCorner];
            lpauColours[luCorner] = PackCornerColour(MakeVector(lColour.x * lrShade.x,
                                                                lColour.y * lrShade.y,
                                                                lColour.z * lrShade.z,
                                                                lColour.w * lrShade.w));
        }
    }
}
}
