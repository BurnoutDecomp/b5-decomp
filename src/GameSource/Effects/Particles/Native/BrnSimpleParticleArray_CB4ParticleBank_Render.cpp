// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleArray_CB4ParticleBank_Render.cpp
//
// The simple-particle RENDER half (the job side of the native simple particles):
//   BrnSimpleParticleArray::CB4ParticleBank::Render          @0x8291E600  (1,003 instructions)
//   SimpleParticleVertexBufferBuilder::BuildDispatchData     @0x8291F870  (   99 instructions)
// (ShadedRotatingRenderMethod::BuildQuad @0x8291E150 is ShadedRotatingRenderMethod.cpp;
//  NativeParticleVertex::VertexIterator::Write @0x8291E478 is BrnNativeParticleVertex.cpp.)
//
// ⭐ 2026-09-24 (FX-CRASHVFX): BODIED. This file used to be an asserting stub whose banner said the
// pipeline "does NOT lower faithfully to scalar C++ without inventing a per-lane formula". It does:
// the body is a straight-line evaluation of four particles per pass whose every lane is independent,
// and whose only cross-lane traffic is (a) the vperm/vsldoi TRANSPOSES that gather one lane out of
// four records (the controls at 0x82CDB430 / 0x82CDB450 / 0x82CDADB0..0x82CDADF0 / 0x82CDA3F0 all
// read out of the image, each selecting whole words) and (b) vspltw broadcasts. Lowered here one
// particle at a time, in the console's order. The whole function was executed on the console's own
// instruction words in a VMX emulator (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/exp_render.py) and
// tests/FxCrashVfxSimpleRender.cpp checks every vertex this lowering writes against that run.
//
// WHAT A SIMPLE PARTICLE IS ON SCREEN: a camera-facing quad, sized in SCREEN space (so a far puff is
// not a pinpoint -- the size is clamped to the array's max screen size), rotated about its own
// centre, lit top-to-bottom by the lighting range, faded in over the near fade distance and out over
// the far one, with its colour and size following a three-key (start, mid, end) ramp over its life.
// The age, the drag-shaped path, the colour and the size are all evaluated from the spawn record at
// render time: nothing is simulated.
//
// CONSTANTS (every one read out of the image):
//   flt_821012CC = 1.4142135 (sqrt 2)     the quad's half-diagonal       -> the quad builder +0x00
//   flt_82001C98 = 1.0                    `lfs f31` -- the reciprocal numerator, the tile extent
//   flt_82003F40 = 0.25                   the screen-size scale (x cot(hfov/2))
//   flt_82001D9C = 2.0                    the drag ramp's 2 * duration
//   flt_82001CC0 = 0.0                    the "no drag" vector and the UV origin
//   flt_82101380 = 0.015686275 (4/255)    the alpha below which a quad is not emitted
//   the lane-lifetime vector, a function-local static (guard dword_831BBEF0, value 0x831BBEE0)
//   built on first use from (1.0, flt_820054C8 = 0.8, flt_82005450 = 0.9, flt_82004C68 = 0.7)
//   flt_8201A7A8 = 1280.0 / flt_8201A7A4 = 720.0   BuildDispatchData's screen-size vector
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameSource/Effects/Particles/Native/ShadedRotatingRenderMethod.h"
#include "GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h"
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>   // std::fabs

namespace BrnParticle
{
namespace Native
{
    // [DIAG] NOT IN THE X360 BINARY -- see the header. DELETE-WHEN-STABLE.
    u32 gauSimpleParticleQuadsBuilt    = 0;
    u32 gauSimpleParticleBatchesBuilt  = 0;

    namespace
    {
        typedef rw::math::vpu::Vector4 Vector4;

        const f32 KF_SQRT_TWO            = 1.4142135381698608f;   // flt_821012CC
        const f32 KF_ONE                 = 1.0f;                  // flt_82001C98
        const f32 KF_ZERO                = 0.0f;                  // flt_82001CC0
        const f32 KF_HALF                = 0.5f;                  // vcfsx(splat 1, 1)
        const f32 KF_SCREEN_SIZE_SCALE   = 0.25f;                 // flt_82003F40
        const f32 KF_TWO                 = 2.0f;                  // flt_82001D9C
        const f32 KF_ALPHA_CULL          = 0.01568627543747425f;  // flt_82101380 (4/255)
        const f32 KF_SCREEN_WIDTH        = 1280.0f;               // flt_8201A7A8
        const f32 KF_SCREEN_HEIGHT       = 720.0f;                // flt_8201A7A4

        // The four particles of a pass live different lengths: lane k lives lifetime * this.
        // Function-local static on the console (guard dword_831BBEF0, vector 0x831BBEE0), built on
        // first use from (flt_82001C98, flt_820054C8, flt_82005450, flt_82004C68).
        const f32 KAF_LANE_LIFETIME_SCALE[4] = { 1.0f, 0.800000011920929f, 0.8999999761581421f,
                                                 0.699999988079071f };

        // "luNumberOfTiles <= 16": the stack table var_2B0 holds sixteen (u, v) pairs.
        const u32 KU_MAX_TILES = 16;

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

        // fsel fD, fA, fC, fB: fA >= 0.0 selects fC, anything else -- including a NaN -- selects fB.
        inline f32 Fsel(f32 lfTest, f32 lfIfNonNegative, f32 lfOtherwise)
        {
            return (lfTest >= 0.0f) ? lfIfNonNegative : lfOtherwise;
        }

        // vrefp + the two Newton-Raphson steps the console runs after it every time
        // (vnmsubfp e' = 1 - e*x ; vmaddfp e = e*e' + e, twice). The PC has no reciprocal
        // ESTIMATE, so the seed is the exact quotient; the two steps are kept because they are the
        // console's arithmetic (and leave an exact seed within an ulp of itself).
        inline f32 RefinedReciprocal(f32 lfValue)
        {
            f32 lfEstimate = KF_ONE / lfValue;
            f32 lfError    = KF_ONE - lfEstimate * lfValue;
            lfEstimate     = lfEstimate * lfError + lfEstimate;
            lfError        = KF_ONE - lfEstimate * lfValue;
            lfEstimate     = lfEstimate * lfError + lfEstimate;
            return lfEstimate;
        }

        inline Vector4 MakeVector(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            Vector4 lResult = { lfX, lfY, lfZ, lfW };
            return lResult;
        }
        inline Vector4 Splat(f32 lfValue)
        {
            return MakeVector(lfValue, lfValue, lfValue, lfValue);
        }

        // |x|: vandc against the vslw-built 0x80000000 splat clears the sign bit, NaN included --
        // which is exactly std::fabs.
        inline f32 AbsoluteValue(f32 lfValue)
        {
            return std::fabs(lfValue);
        }
    }

    // =============================================================================================
    // CB4ParticleBank::Render @0x8291E600.
    // =============================================================================================
    void BrnSimpleParticleArray::CB4ParticleBank::Render(NativeParticleVertex::VertexIterator& lrIterator,
                                                         const BrnSimpleParticleArray& lrArray,
                                                         const SimpleParticleRenderState& lrState) const
    {
        // (top - current) / stride -- the lwz 8 / lwz 0 / lwz 0xC / divwu at 0x8291E63C..0x8291E668.
        const u32 luNumFreeVertices = lrIterator.GetVerticesFree();

        const CB4ParticleArrayStandardParams& lrParams = *lrArray.mpStandardParams;
        const CB4ParticleArrayXenon&          lrData   = lrArray.mParticleData;

        // ---- the quad builder, var_470 (0x8291E678..0x8291E6F0) --------------------------------
        // The lighting range is [min(1, lighting.min), min(1, lighting.max)]; the builder carries its
        // middle (w = 1.0) and its half-width (w = 0.0).
        ShadedRotatingRenderMethod lRenderMethod;
        {
            const f32 lfLightingMin = VMin(KF_ONE, lrParams.mLightingMinMax.x);
            const f32 lfLightingMax = VMin(KF_ONE, lrParams.mLightingMinMax.y);
            const f32 lfHalfRange   = (lfLightingMax - lfLightingMin) * KF_HALF;
            const f32 lfMiddle      = lfLightingMin + lfHalfRange;
            lRenderMethod.mvCornerScale   = Splat(KF_SQRT_TWO);
            lRenderMethod.mvLightingBase  = MakeVector(lfMiddle, lfMiddle, lfMiddle, KF_ONE);
            lRenderMethod.mvLightingRange = MakeVector(lfHalfRange, lfHalfRange, lfHalfRange, KF_ZERO);
        }

        // "luNumFreeVertices >= ( muNumParticles * 4 )" (BrnSimpleParticleRenderer.cpp:487). The
        // console prints it (CgsDev::Assert::PrintStringed, twice) and then RETURNS -- the bank is
        // not drawn at all when the batch cannot hold every one of its quads.
        CGS_ASSERT(luNumFreeVertices >= muNumParticles * 4u,
                   "luNumFreeVertices >= ( muNumParticles * 4 )");
        if (luNumFreeVertices < muNumParticles * 4u)
            return;

        // ---- the camera terms (0x8291E768..0x8291E8D8) -----------------------------------------
        const CgsGraphics::Camera& lrCamera = lrState.mCamera;
        const f32 lfCotHorizontal = lrCamera.maProjectionScalars[1];   // +0x144 m_oneOverTanHalfFovHorizontal
        const f32 lfCotVertical   = lrCamera.maProjectionScalars[4];   // +0x150 m_oneOverTanHalfFovVertical
        const f32 lfCameraNear    = lrCamera.maProjectionScalars[7];   // +0x15C m_nearClipPlane
        const f32 lfCameraFar     = lrCamera.maProjectionScalars[8];   // +0x160 m_farClipPlane

        // Every distance is in "zoomed" units: scaled by cot(hfov/2), so a narrower lens pulls the
        // clip and fade planes out with it.
        const f32 lfNearFade = lrData.mrNearFade * lfCotHorizontal;
        const f32 lfFarFade  = lrData.mrFarFade  * lfCotHorizontal;
        const f32 lfNearClip = lrData.mrNearClip * lfCotHorizontal;
        const f32 lfFarClip  = lrData.mrFarClip  * lfCotHorizontal;
        const f32 lfOneOverNearFade = KF_ONE / lfNearFade;   // fdivs f31 / f9
        const f32 lfOneOverFarFade  = KF_ONE / lfFarFade;
        // fsel f13, (camNear - nearClip), camNear, nearClip   -> the larger (a NaN picks nearClip)
        // fsel f13, (camFar - farClip),   farClip, camFar     -> the smaller (a NaN picks camFar)
        const f32 lfNearPlane = Fsel(lfCameraNear - lfNearClip, lfCameraNear, lfNearClip);
        const f32 lfFarPlane  = Fsel(lfCameraFar - lfFarClip, lfFarClip, lfCameraFar);
        const f32 lfScreenScale = lfCotHorizontal * KF_SCREEN_SIZE_SCALE;
        const f32 lfAspect      = lfCotVertical / lfCotHorizontal;
        // The max screen size is authored in PIXELS of a 1280-wide screen: size.w * (1 / 1280).
        const f32 lfMaxScreenSize = lrData.mSizeParams.w * RefinedReciprocal(lrState.mvScreenSize.x);

        // ---- the per-lane lifetimes (0x8291E8D4..0x8291E9B4) -----------------------------------
        f32 lafLifetime[4];
        f32 lafMidLifetime[4];
        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            lafLifetime[luLane]    = KAF_LANE_LIFETIME_SCALE[luLane] * lrData.mrLifeTime;
            lafMidLifetime[luLane] = lafLifetime[luLane] * lrData.mrMidTime;
        }

        // ---- the drag ramp (0x8291E9B0..0x8291EA30) --------------------------------------------
        // The velocity scale ramps linearly from initial to terminal over the duration and then
        // holds, so the distance travelled is
        //     lo = min(age, dur) ; hi = max(age, dur)
        //     d  = lo*lo*(term - init)/(2*dur) + lo*init + hi*term - term*dur
        // With drag off the vector is (0, 1, 0, 0) and the duration 0, i.e. d = age.
        f32 lfDragInitial  = lrParams.mrDragInitialVelocityScale;
        f32 lfDragTerminal = lrParams.mrDragTerminalVelocityScale;
        f32 lfDragDuration = lrParams.mrDragDuration;
        f32 lfDragSlope    = (lfDragTerminal - lfDragInitial) / (lfDragDuration * KF_TWO);
        f32 lfDragArea     = lfDragTerminal * lfDragDuration;
        if (!lrParams.mbUseDrag)
        {
            lfDragDuration = KF_ZERO;
            lfDragInitial  = KF_ZERO;
            lfDragTerminal = KF_ONE;
            lfDragSlope    = KF_ZERO;
            lfDragArea     = KF_ZERO;
        }

        // ---- the colour keys, white-levelled (0x8291EA34..0x8291EA94) --------------------------
        const f32 lfWhite = lrState.mfWhiteLevel;
        const Vector4 lStartColour = MakeVector(lrData.mStartColour.x * lfWhite, lrData.mStartColour.y * lfWhite,
                                                lrData.mStartColour.z * lfWhite, lrData.mStartColour.w * KF_ONE);
        const Vector4 lMidColour   = MakeVector(lrData.mMidColour.x * lfWhite, lrData.mMidColour.y * lfWhite,
                                                lrData.mMidColour.z * lfWhite, lrData.mMidColour.w * KF_ONE);
        const Vector4 lEndColour   = MakeVector(lrData.mEndColour.x * lfWhite, lrData.mEndColour.y * lfWhite,
                                                lrData.mEndColour.z * lfWhite, lrData.mEndColour.w * KF_ONE);

        // ---- the tile table, var_2B0 (0x8291EAF4..0x8291ECA4) ----------------------------------
        const u32 luTilesWide     = lrParams.muTilesWide;
        const u32 luTilesHigh     = lrParams.muTilesHigh;
        const u32 luNumberOfTiles = luTilesHigh * luTilesWide;
        CGS_ASSERT(luNumberOfTiles <= KU_MAX_TILES, "luNumberOfTiles <= 16");   // :608
        // FLAG PC memory safety: the console prints the assert above and carries on, writing past the
        // sixteen-entry stack table when an array is authored with more tiles. The host writes only
        // what fits; no shipped nativeparticleparams collection has more than sixteen.
        // ⚠ AND A ZERO TILE COUNT TRAPS ON THE CONSOLE: the tile index is `divwu` by it, guarded by
        // `twllei r25, 0` (0x8291F36C). The host has no trap to take, so it asserts and draws nothing.
        CGS_ASSERT(luNumberOfTiles != 0u, "twllei r25, 0 -- a zero tile count traps on the console");
        if (luNumberOfTiles == 0u)
            return;

        f32 lafTileU[KU_MAX_TILES];
        f32 lafTileV[KU_MAX_TILES];
        for (u32 luRow = 0; luRow < luTilesHigh; ++luRow)
        {
            const f32 lfRowV = static_cast<f32>(luRow) / static_cast<f32>(luTilesHigh);
            for (u32 luColumn = 0; luColumn < luTilesWide; ++luColumn)
            {
                const u32 luTile = luRow * luTilesWide + luColumn;
                if (luTile < KU_MAX_TILES)
                {
                    lafTileU[luTile] = static_cast<f32>(luColumn) / static_cast<f32>(luTilesWide);
                    lafTileV[luTile] = lfRowV;
                }
            }
        }
        // One tile's UV extent is 1/tilesWide on BOTH axes (`fdivs f31, f31, f0` with f0 the tiles
        // WIDE count is the only extent the body forms; a non-square atlas samples outside its tile
        // vertically -- the console's quirk, kept). The origin terms are that extent times 0.0.
        const f32 lfTileExtent = KF_ONE / static_cast<f32>(luTilesWide);
        const f32 lfTileOriginU = lfTileExtent * KF_ZERO;   // f30
        const f32 lfTileOriginV = lfTileExtent * KF_ZERO;   // f29

        const Vector4 lAcceleration = MakeVector(lrData.mAcceleration.x, lrData.mAcceleration.y,
                                                 lrData.mAcceleration.z, lrData.mAcceleration.w);
        const Vector4& lrRow0 = lrCamera.mViewProjection.xAxis;   // camera +0x80
        const Vector4& lrRow1 = lrCamera.mViewProjection.yAxis;   // +0x90
        const Vector4& lrRow2 = lrCamera.mViewProjection.zAxis;   // +0xA0
        const Vector4& lrRow3 = lrCamera.mViewProjection.wAxis;   // +0xB0

        // ---- the particles: blocks of sixteen, passes of four ----------------------------------
        // The tile index is the particle's position in the WHOLE bank (r21, +4 per pass whether or
        // not the pass drew anything), so a record keeps its tile for its whole life.
        const u32 luNumBlocks = muNumParticles >> 4;
        u32 luParticleIndex = 0;
        for (u32 luBlock = 0; luBlock < luNumBlocks; ++luBlock)
        {
            const CB4Particle* const lpBlock = mpaParticles + luBlock * 16u;

            // The sixteen ages, time - spawn time (the w-lane transposes + vsubfp128 at 0x8291ED5C..
            // 0x8291EE3C, stored to var_370..var_340).
            f32 lafAges[16];
            for (u32 luParticle = 0; luParticle < 16; ++luParticle)
                lafAges[luParticle] = lrState.mfCurrentTime - lpBlock[luParticle].mPositionTime.GetPlus();

            for (u32 luPass = 0; luPass < 4; ++luPass, luParticleIndex += 4)
            {
                const CB4Particle* const lpPass = lpBlock + luPass * 4u;
                const f32* const lpfAges = lafAges + luPass * 4u;

                // alive = age > 0 && !(age >= lifetime_lane) -- vcmpgtfp128 / vcmpgefp128 / vnot / vand.
                bool labAlive[4];
                bool lbAnyAlive = false;
                for (u32 luLane = 0; luLane < 4; ++luLane)
                {
                    labAlive[luLane] = (lpfAges[luLane] > KF_ZERO) && !(lpfAges[luLane] >= lafLifetime[luLane]);
                    lbAnyAlive = lbAnyAlive || labAlive[luLane];
                }
                if (!lbAnyAlive)
                    continue;   // loc_8291F808

                for (u32 luLane = 0; luLane < 4; ++luLane)
                {
                    const CB4Particle& lrParticle = lpPass[luLane];
                    const f32 lfAge = lpfAges[luLane];

                    // The path: drag-shaped distance along the spawn velocity, plus the gravity term.
                    const f32 lfHigh = VMax(lfAge, lfDragDuration);
                    const f32 lfLow  = VMin(lfAge, lfDragDuration);
                    f32 lfDistance = lfLow * lfDragInitial + lfHigh * lfDragTerminal;
                    lfDistance     = (lfLow * lfLow) * lfDragSlope + lfDistance;
                    lfDistance     = lfDistance - lfDragArea;

                    const f32 lfAgeSquared = lfAge * lfAge;
                    const Vector4 lPosition = MakeVector(
                        lAcceleration.x * lfAgeSquared + (lrParticle.mVelocityRotation.x * lfDistance + lrParticle.mPositionTime.x),
                        lAcceleration.y * lfAgeSquared + (lrParticle.mVelocityRotation.y * lfDistance + lrParticle.mPositionTime.y),
                        lAcceleration.z * lfAgeSquared + (lrParticle.mVelocityRotation.z * lfDistance + lrParticle.mPositionTime.z),
                        lAcceleration.w * lfAgeSquared + (lrParticle.mVelocityRotation.w * lfDistance + lrParticle.mPositionTime.w));

                    // The three-key ramp: before the mid key the segment runs start -> mid over
                    // [0, mid); after it, end -> mid over [life, mid) (the fraction runs BACKWARDS
                    // from the end key, which is the same line).
                    const bool lbBeforeMid     = !(lfAge >= lafMidLifetime[luLane]);
                    const f32  lfSegmentOrigin = lbBeforeMid ? KF_ZERO : lafLifetime[luLane];
                    const f32  lfSegmentSpan   = lafMidLifetime[luLane] - lfSegmentOrigin;
                    const f32  lfFraction      = RefinedReciprocal(lfSegmentSpan) * (lfAge - lfSegmentOrigin);

                    const Vector4& lrBaseColour = lbBeforeMid ? lStartColour : lEndColour;
                    const Vector4 lColour = MakeVector(
                        (lMidColour.x - lrBaseColour.x) * lfFraction + lrBaseColour.x,
                        (lMidColour.y - lrBaseColour.y) * lfFraction + lrBaseColour.y,
                        (lMidColour.z - lrBaseColour.z) * lfFraction + lrBaseColour.z,
                        (lMidColour.w - lrBaseColour.w) * lfFraction + lrBaseColour.w);

                    const f32 lfBaseSize = lbBeforeMid ? lrData.mSizeParams.x : lrData.mSizeParams.z;
                    const f32 lfSize = (lfFraction * (lrData.mSizeParams.y - lfBaseSize) + lfBaseSize)
                                     * lrParticle.mScaleAlpha.z;

                    // The projection: row-vector clip = row3 + row0*x + row1*y + row2*z, then the
                    // divide by w through the refined reciprocal.
                    Vector4 lClip = lrRow3;
                    lClip = MakeVector(lrRow0.x * lPosition.x + lClip.x, lrRow0.y * lPosition.x + lClip.y,
                                       lrRow0.z * lPosition.x + lClip.z, lrRow0.w * lPosition.x + lClip.w);
                    lClip = MakeVector(lrRow1.x * lPosition.y + lClip.x, lrRow1.y * lPosition.y + lClip.y,
                                       lrRow1.z * lPosition.y + lClip.z, lrRow1.w * lPosition.y + lClip.w);
                    lClip = MakeVector(lrRow2.x * lPosition.z + lClip.x, lrRow2.y * lPosition.z + lClip.y,
                                       lrRow2.z * lPosition.z + lClip.z, lrRow2.w * lPosition.z + lClip.w);
                    const f32 lfDepth = lClip.w;
                    const f32 lfOneOverDepth = RefinedReciprocal(lfDepth) * KF_ONE;
                    const Vector4 lProjected = MakeVector(lClip.x * lfOneOverDepth, lClip.y * lfOneOverDepth,
                                                          lClip.z * lfOneOverDepth, lClip.w * lfOneOverDepth);

                    // Screen-space size, clamped to the max, and its vertical twin.
                    const f32 lfSizeX = VMin((lfSize * lfOneOverDepth) * lfScreenScale, lfMaxScreenSize);
                    const f32 lfSizeY = lfSizeX * lfAspect;

                    // The fade: in over the near fade distance, out over the far one, capped at 1.
                    const f32 lfFade = VMin(VMin((lfDepth - lfNearPlane) * lfOneOverNearFade,
                                                 (lfFarPlane - lfDepth) * lfOneOverFarFade),
                                            KF_ONE);
                    const f32 lfAlpha = (lrParticle.mScaleAlpha.w * lColour.w) * lfFade;

                    // The visibility mask, verbatim: note the depth term is an OR -- (w > near) OR
                    // NOT (w >= far) -- so a particle nearer than the near plane still passes it and
                    // is removed only by its (negative) fade alpha. Off-screen means the quad lies
                    // wholly beyond an edge: max(|x| - sizeX, |y| - sizeY) >= 1.
                    const f32 lfEdge = VMax(AbsoluteValue(lProjected.x) - lfSizeX,
                                            AbsoluteValue(lProjected.y) - lfSizeY);
                    const bool lbVisible = labAlive[luLane]
                                        && ((lfDepth > lfNearPlane) || !(lfDepth >= lfFarPlane))
                                        && !(lfEdge >= KF_ONE)
                                        && (lfAlpha > KF_ALPHA_CULL);
                    if (!lbVisible)
                        continue;

                    // ---- the quad (0x8291F308..0x8291F400 for lane 0; lanes 1-3 identical) ----
                    Vector4 laPositions[4];
                    u32     lauColours[4];
                    lRenderMethod.BuildQuad(laPositions, lauColours, &lrParticle, &lrCamera.mView,
                                            lProjected, MakeVector(lfSizeX, lfSizeY, KF_ZERO, lfSizeX),
                                            lColour, Splat(lfAge), Splat(lfAlpha));

                    const u32 luTile = (luParticleIndex + luLane) % luNumberOfTiles;
                    const f32 lfTileU = (luTile < KU_MAX_TILES) ? lafTileU[luTile] : KF_ZERO;
                    const f32 lfTileV = (luTile < KU_MAX_TILES) ? lafTileV[luTile] : KF_ZERO;

                    const f32 lafUv0[2] = { lfTileU + lfTileOriginU, lfTileV + lfTileOriginV };
                    const f32 lafUv1[2] = { lfTileU + lfTileExtent,  lfTileV + lfTileOriginV };
                    const f32 lafUv2[2] = { lfTileU + lfTileExtent,  lfTileV + lfTileExtent  };
                    const f32 lafUv3[2] = { lfTileU + lfTileOriginU, lfTileV + lfTileExtent  };
                    lrIterator.Write(laPositions[0], reinterpret_cast<const int*>(&lauColours[0]), lafUv0);
                    lrIterator.Write(laPositions[1], reinterpret_cast<const int*>(&lauColours[1]), lafUv1);
                    lrIterator.Write(laPositions[2], reinterpret_cast<const int*>(&lauColours[2]), lafUv2);
                    lrIterator.Write(laPositions[3], reinterpret_cast<const int*>(&lauColours[3]), lafUv3);

                    ++gauSimpleParticleQuadsBuilt;   // [diag]
                }
            }
        }
    }

    // =============================================================================================
    // SimpleParticleVertexBufferBuilder::BuildDispatchData @0x8291F870.
    //
    // One batch per listed type. A bank is walked only while it can still hold a live particle:
    // `time - mrLastSpawnTime < lifetime` (fcmpu + bge, so an unordered compare WALKS it), where
    // mrLastSpawnTime is the newest spawn time SpawnParticle has written into the bank. The crash
    // bank renders into the SAME batch when asked, so a crash particle and an ordinary one of the
    // same type share one draw, one texture and one blend.
    // =============================================================================================
    void SimpleParticleVertexBufferBuilder::BuildDispatchData(EffectsVertexBufferLocked* lpLockedBuffer,
                                                              SimpleParticleBatchArray&  lrBatches,
                                                              BrnSimpleParticleArray*    lpArrays,
                                                              const u32*                 lpauParticleTypes,
                                                              u32                        luNumParticleTypes,
                                                              f32                        lfCurrentTime,
                                                              const CgsGraphics::Camera& lrCamera,
                                                              f32                        lfWhiteLevel,
                                                              bool                       lbRenderCrashBanks)
    {
        SimpleParticleRenderState lState;
        lState.mCamera        = lrCamera;
        lState.mvScreenSize   = MakeVector(KF_SCREEN_WIDTH, KF_SCREEN_HEIGHT, KF_ZERO, KF_ZERO);
        lState.mfCurrentTime  = lfCurrentTime;
        lState.mfWhiteLevel   = lfWhiteLevel;

        for (u32 luEntry = 0; luEntry < luNumParticleTypes; ++luEntry)
        {
            const u32 luParticleType = lpauParticleTypes[luEntry];
            BrnSimpleParticleArray& lrArray = lpArrays[luParticleType];

            NativeParticleVertex::VertexIterator lIterator;
            SimpleParticleBatch                  lBatch;
            lpLockedBuffer->BeginBatch(lIterator, lBatch, NativeParticleVertex::GetStride());

            if (!((lfCurrentTime - lrArray.mBankRegular.mrLastSpawnTime) >= lrArray.mParticleData.mrLifeTime))
                lrArray.mBankRegular.Render(lIterator, lrArray, lState);
            if (lbRenderCrashBanks
                && !((lfCurrentTime - lrArray.mBankCrash.mrLastSpawnTime) >= lrArray.mParticleData.mrLifeTime))
            {
                lrArray.mBankCrash.Render(lIterator, lrArray, lState);
            }

            lpLockedBuffer->EndBatch(lIterator, lBatch, NativeParticleVertex::GetStride());

            if (lBatch.muVertexCount != 0)
            {
                lBatch.meBlendMode = static_cast<u32>(lrArray.mpStandardParams->meBlendMode);
                CGS_ASSERT(lBatch.meBlendMode < static_cast<u32>(eParticleBlendMax),
                           "lBatch.meBlendMode < AttribSys::Enums::ParticleBlend::eParticleBlendMax");   // :1300
                lBatch.meParticleType = luParticleType;
                lrBatches.Append(lBatch);
                ++gauSimpleParticleBatchesBuilt;   // [diag]
            }
        }
    }
}
}
