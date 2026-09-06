// ============================================================================
// GameSource/Effects/Particles/Native/BrnSparkRenderer_Render.cpp
//
// The spark RENDER half:
//   SparkArray::CalculateSparkPosition            @0x8291FC78   (54 instr)
//   SparkArray::RenderBank                        @0x8291FD50   (1,033 instr)
//   SparkVertexBufferBuilder::BuildDispatchData   @0x82920D78   (101 instr)
//
// ================================ WHAT A SPARK IS, ON SCREEN ================================
// Not a billboard quad: a MOTION-BLURRED RIBBON. The particle module keeps a ring of up to
// eight camera snapshots (SparkFrameDataSet), and RenderBank evaluates the SAME spark at
// each of those snapshot times, transforms every sample into that snapshot's own view space,
// and stitches the samples into one triangle strip whose half-width is the spark's size times
// the array's mfSparkRadius. That is why the sparks off a scraping car read as streaks that
// bend with the camera rather than as dots.
//
// The whole geometry pipeline is the console's, in this order:
//   1. walk the frame ring while (frames[0].time - frames[i].time) < the blur window and the
//      timestamps still differ (max 8) -> luFrameCount, and record each frame's NORMALISED
//      age, which is the ribbon's u texture coordinate;
//   2. pack the four Vector4 colours into four D3D colour dwords, scaled by the white level
//      (x 255.9) and clamped;
//   3. build TWO view-projection matrices -- frames[0] and frames[luFrameCount-1] -- and use
//      them to frustum-cull each live spark by its newest and oldest sample (an index list);
//   4. for each surviving spark, evaluate CalculateSparkPosition at every ring frame,
//      transform into that frame's VIEW space, take a central-difference tangent, cross it
//      with the view-space position to get the ribbon's width direction, normalise, and emit
//      two vertices per sample.
//
// Everything below is store-for-store off that asm; the derivations are spelled at each step.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>

namespace BrnParticle
{
namespace Native
{

namespace
{
    typedef rw::math::vpu::Vector4 Vector4;

    inline Vector4 VecSplat(f32 lfValue)
    {
        Vector4 lResult = { lfValue, lfValue, lfValue, lfValue };
        return lResult;
    }
    inline Vector4 VecAdd(const Vector4& lrA, const Vector4& lrB)
    {
        Vector4 lResult = { lrA.x + lrB.x, lrA.y + lrB.y, lrA.z + lrB.z, lrA.w + lrB.w };
        return lResult;
    }
    inline Vector4 VecSub(const Vector4& lrA, const Vector4& lrB)
    {
        Vector4 lResult = { lrA.x - lrB.x, lrA.y - lrB.y, lrA.z - lrB.z, lrA.w - lrB.w };
        return lResult;
    }
    inline Vector4 VecMul(const Vector4& lrA, const Vector4& lrB)
    {
        Vector4 lResult = { lrA.x * lrB.x, lrA.y * lrB.y, lrA.z * lrB.z, lrA.w * lrB.w };
        return lResult;
    }
    inline Vector4 VecScale(const Vector4& lrA, f32 lfScale)
    {
        Vector4 lResult = { lrA.x * lfScale, lrA.y * lfScale, lrA.z * lfScale, lrA.w * lfScale };
        return lResult;
    }

    // vmaddfp on four lanes: A * splat(s) + B.
    inline Vector4 VecMulAdd(const Vector4& lrA, f32 lfScale, const Vector4& lrB)
    {
        Vector4 lResult = { lrA.x * lfScale + lrB.x, lrA.y * lfScale + lrB.y,
                            lrA.z * lfScale + lrB.z, lrA.w * lfScale + lrB.w };
        return lResult;
    }

    // The console's row-vector transform: out = m.xAxis*v.x + m.yAxis*v.y + m.zAxis*v.z + m.wAxis
    // (four vmaddfp against splatted lanes, wAxis seeded first).
    inline Vector4 TransformPoint(const Vector4& lrRow0, const Vector4& lrRow1,
                                  const Vector4& lrRow2, const Vector4& lrRow3,
                                  const Vector4& lrPoint)
    {
        Vector4 lResult = lrRow3;
        lResult = VecMulAdd(lrRow0, lrPoint.x, lResult);
        lResult = VecMulAdd(lrRow1, lrPoint.y, lResult);
        lResult = VecMulAdd(lrRow2, lrPoint.z, lResult);
        return lResult;
    }

    // flt_82101590 -- the console's colour scale. 255.9, NOT 255: the fctidz truncation that
    // follows it rounds 1.0 down to 255 with a plain 255 multiplier, which is the standard
    // "just under 256" float->byte convention.
    const f32 KF_COLOUR_SCALE = 255.89999389648438f;

    // unk_8210145C -- FLT_EPSILON, the degenerate-tangent guard in the normalise step.
    const f32 KF_TANGENT_EPSILON = 1.1920928955078125e-07f;

    // The console's four-lane float->byte pack: fctidz per lane, then
    //   (alpha << 24) | (blue << 16) | (green << 8) | red
    // built by the `rotlwi 8 / or / slwi 8 / or / slwi 8 / or` chain at 0x8291FF7C..0x8291FFC4.
    inline u32 PackColour(const Vector4& lrColour)
    {
        const u32 luRed   = static_cast<u32>(static_cast<s64>(lrColour.x)) & 0xFFu;
        const u32 luGreen = static_cast<u32>(static_cast<s64>(lrColour.y)) & 0xFFu;
        const u32 luBlue  = static_cast<u32>(static_cast<s64>(lrColour.z)) & 0xFFu;
        const u32 luAlpha = static_cast<u32>(static_cast<s64>(lrColour.w)) & 0xFFu;
        return (luAlpha << 24) | (luBlue << 16) | (luGreen << 8) | luRed;
    }

    // The byte re-order the vertex path applies to that word before it reaches
    // NativeParticleVertex::VertexIterator::Write (0x82920BAC..0x82920BF0). Written out as
    // the console's own six extract/insert steps rather than guessed as a swap:
    //   b0 = word >> 24 ; b1 = (word >> 16) & 0xFF ; b2 = (word >> 8) & 0xFF ; b3 = word & 0xFF
    //   r3  = b2 ; insrwi r3, b3, 8, 16   -> r3  = (b2 & 0xFF00FFFF) | (b3 << 8)  == (b3<<8)|b2
    //   r30 = b1 ; insrwi r30, r3, 24, 0  -> r30 = (r3 << 8) | b1  (r3 masked to 16 bits first)
    //   r30 = (r30 << 8) | b0
    // i.e. the result is ((b3<<8 | b2) << 16) | (b1 << 8) | b0 == a full byte reversal of the
    // packed word.
    inline u32 SwizzleVertexColour(u32 luPackedColour)
    {
        const u32 lu0 = (luPackedColour >> 24) & 0xFFu;
        const u32 lu1 = (luPackedColour >> 16) & 0xFFu;
        const u32 lu2 = (luPackedColour >> 8) & 0xFFu;
        const u32 lu3 = luPackedColour & 0xFFu;
        return (lu3 << 24) | (lu2 << 16) | (lu1 << 8) | lu0;
    }

    // One ribbon sample: the console's 0x30-byte stack record (var_720, stride 0x30).
    struct SparkRibbonPoint
    {
        Vector4 mViewPosition;   // +0x00
        Vector4 mTangent;        // +0x10 (a central difference, then normalise(p x t))
        f32     mfAge;           // +0x20
        f32     maPad[3];
    };
}

// =============================================================================================
// SparkArray::CalculateSparkPosition @0x8291FC78 -- a pure-VMX leaf, no branches, no calls.
//
// The spark's velocity is scaled by a value that ramps linearly from mfDragInitialVelocityScale
// to mfDragTerminalVelocityScale over mfDragDuration and then holds. Integrating that ramp
// gives the displacement factor the console evaluates here, spelled with min/max so both
// phases are one expression:
//
//     lo = min(t, dur) ; hi = max(t, dur)
//     s  = lo*init + hi*term + lo*lo*(term-init)/(2*dur) - dur*term
//     p  = position + velocity*s + halfGravity*t*t
//
// Check it against SpawnSpark's own model: s(0) == 0 (0 + dur*term + 0 - dur*term) and
// s(dur) == dur*(init+term)/2, the average of the ramp. lDragParams is the vector
// (init, term, (term-init)/(2*dur), dur*term) RenderBank builds once per bank.
//
// ⭐ THE LAST TWO INSTRUCTIONS ARE THE BOUNCE. The tail compares splat(p.y) with splat(p.w)
// and vperms the larger into lane 1 through [unk_8327F140 + 0x40 + srcword*0x10] -- the
// engine lane-insert table's LANE 1 row, source word 1 when y wins and 3 when w does. Because
// halfGravity is (0, g/2, 0, g/2) and mVelocity.w is the post-bounce vertical velocity
// SpawnSpark solved, the w lane is a SECOND ballistic track: the spark that has already
// bounced. Taking the max is what makes the spark skip off the ground instead of sinking
// through it. This is the independent confirmation that SpawnSpark's w lanes were read right.
// =============================================================================================
rw::math::vpu::Vector4 SparkArray::CalculateSparkPosition(
    rw::math::vpu::Vector4::InParam lPosition,
    rw::math::vpu::Vector4::InParam lVelocity,
    rw::math::vpu::Vector4::InParam lAge,
    rw::math::vpu::Vector4::InParam lHalfGravity,
    rw::math::vpu::Vector4::InParam lDragDuration,
    rw::math::vpu::Vector4::InParam lDragParams)
{
    // vmaxfp / vminfp, per lane.
    Vector4 lHi = { (lAge.x > lDragDuration.x) ? lAge.x : lDragDuration.x,
                    (lAge.y > lDragDuration.y) ? lAge.y : lDragDuration.y,
                    (lAge.z > lDragDuration.z) ? lAge.z : lDragDuration.z,
                    (lAge.w > lDragDuration.w) ? lAge.w : lDragDuration.w };
    Vector4 lLo = { (lAge.x < lDragDuration.x) ? lAge.x : lDragDuration.x,
                    (lAge.y < lDragDuration.y) ? lAge.y : lDragDuration.y,
                    (lAge.z < lDragDuration.z) ? lAge.z : lDragDuration.z,
                    (lAge.w < lDragDuration.w) ? lAge.w : lDragDuration.w };

    const Vector4 lInitial  = VecSplat(lDragParams.x);
    const Vector4 lTerminal = VecSplat(lDragParams.y);
    const Vector4 lHalfSlope= VecSplat(lDragParams.z);
    const Vector4 lRampArea = VecSplat(lDragParams.w);

    const Vector4 lAgeSquared = VecMul(lAge, lAge);
    const Vector4 lLoSquared  = VecMul(lLo, lLo);

    Vector4 lScale = VecAdd(VecMul(lLo, lInitial), VecMul(lHi, lTerminal));
    lScale = VecAdd(VecMul(lLoSquared, lHalfSlope), lScale);
    lScale = VecSub(lScale, lRampArea);

    Vector4 lResult = VecAdd(VecMul(lVelocity, lScale), lPosition);
    lResult = VecAdd(VecMul(lHalfGravity, lAgeSquared), lResult);

    // The lane-1 insert: y := max(y, w) -- the un-bounced and bounced tracks.
    if (!(lResult.y > lResult.w))
        lResult.y = lResult.w;

    return lResult;
}

// =============================================================================================
// SparkArray::RenderBank @0x8291FD50.
//
// Register contract, taken from BuildDispatchData's own call setup (0x82920E40..0x82920E84):
//   r3 this  r4 the vertex iterator  r5 the locked buffer  r6 the batch  r7 the bank
//   r8 <eaten by f1>  r9 the frame-data set    f1 mfSparkRadius  f2 the blur window
//   f3 the white level
// r5 and r6 are never read by the body -- the batch's vertex count is filled by EndBatch, not
// here -- but they are the console's parameters and are spelled as such.
// =============================================================================================
void SparkArray::RenderBank(NativeParticleVertex::VertexIterator& lrIterator,
                            EffectsVertexBufferLocked*            lpLockedBuffer,
                            SparkBatch*                           lpBatch,
                            const SparkBank&                      lrBank,
                            f32                                   lfSparkRadius,
                            const SparkFrameDataSet&              lrFrameData,
                            f32                                   lfMotionBlurWindow,
                            f32                                   lfWhiteLevel) const
{
    (void)lpLockedBuffer;   // r5 -- passed, never read by the X360 body
    (void)lpBatch;          // r6 -- likewise

    // `lwz r11, 0(r24) ; cmplwi ; beq -> exit`: an empty bank draws nothing.
    if (lrBank.mpBuckets == 0)
        return;

    // ---- 1. the motion-blur frame window ----------------------------------------------------
    // The gravity vector every CalculateSparkPosition call is handed: (0, g/2, 0, g/2). The
    // duplicated w lane is what makes the bounce track fall at the same rate as the direct one.
    const f32     lfHalfGravity  = mfGravityStrength * 0.5f;
    const Vector4 lHalfGravityVec = { 0.0f, lfHalfGravity, 0.0f, lfHalfGravity };

    const f32 lfNewestTime = lrFrameData.GetFrame(0).mfTimeStamp;

    u32 luLastFrame = 0;
    while (true)
    {
        if ((lfNewestTime - lrFrameData.GetFrame(luLastFrame).mfTimeStamp) >= lfMotionBlurWindow)
            break;
        // Two identical timestamps mean the ring has not been advanced that far yet.
        if (lrFrameData.GetFrame(luLastFrame).mfTimeStamp
            == lrFrameData.GetFrame(luLastFrame + 1).mfTimeStamp)
            break;
        ++luLastFrame;
        if (luLastFrame >= 7)
            break;
    }
    if (luLastFrame == 0)
        return;

    const u32 luFrameCount = luLastFrame + 1;

    // The per-frame u texture coordinate: age / window, so the ribbon's texture runs from the
    // head (0) to the tail (1).
    const f32 lfInverseWindow = 1.0f / lfMotionBlurWindow;
    f32 lafFrameU[knSparksMaxBlurFrames];
    for (u32 luFrame = 0; luFrame < luFrameCount; ++luFrame)
    {
        CGS_ASSERT(luFrame < knSparksMaxBlurFrames, "luFrameId < knSparksMaxBlurFrames");
        lafFrameU[luFrame] =
            (lfNewestTime - lrFrameData.GetFrame(luFrame).mfTimeStamp) * lfInverseWindow;
    }

    // ---- 2. the four packed colours ---------------------------------------------------------
    // colour * (wl*255.9, wl*255.9, wl*255.9, 255.9), clamped to (255.9, 255.9, 255.9, 255.9)
    // -- the alpha lane is NOT scaled by the white level, only the three colour lanes are.
    const f32     lfWhiteScale = lfWhiteLevel * KF_COLOUR_SCALE;
    const Vector4 lColourScale = { lfWhiteScale, lfWhiteScale, lfWhiteScale, KF_COLOUR_SCALE };
    const Vector4 lColourCeil  = { KF_COLOUR_SCALE, KF_COLOUR_SCALE, KF_COLOUR_SCALE, KF_COLOUR_SCALE };

    f32 lafLifetimes[4];
    u32 lauPackedColours[4];
    for (u32 luColour = 0; luColour < 4; ++luColour)
    {
        Vector4 lScaled = VecMul(maColours[luColour], lColourScale);
        lScaled.x = (lScaled.x < lColourCeil.x) ? lScaled.x : lColourCeil.x;   // vminfp
        lScaled.y = (lScaled.y < lColourCeil.y) ? lScaled.y : lColourCeil.y;
        lScaled.z = (lScaled.z < lColourCeil.z) ? lScaled.z : lColourCeil.z;
        lScaled.w = (lScaled.w < lColourCeil.w) ? lScaled.w : lColourCeil.w;
        lauPackedColours[luColour] = PackColour(lScaled);
        lafLifetimes[luColour]     = mafLifetimes[luColour];   // the stack copy at var_7D0
    }

    // ---- 3. the drag-parameter vector, built once ------------------------------------------
    const Vector4 lDragParams = { mfDragInitialVelocityScale,
                                  mfDragTerminalVelocityScale,
                                  (mfDragTerminalVelocityScale - mfDragInitialVelocityScale)
                                      / (mfDragDuration * 2.0f),
                                  mfDragDuration * mfDragTerminalVelocityScale };
    const Vector4 lDragDurationVec = VecSplat(mfDragDuration);

    // The two ages the cull pass evaluates at: the newest frame and frames[luLastFrame - 1].
    // (The console really does use luLastFrame - 1 here and 0..luLastFrame in the vertex pass;
    // reproduced, not tidied.)
    CGS_ASSERT((luLastFrame - 1) < knSparksMaxBlurFrames, "luFrameId < knSparksMaxBlurFrames");
    const f32 lfOldestCullTime = lrFrameData.GetFrame(luLastFrame - 1).mfTimeStamp;

    // ---- 4. the two view-projection matrices used for culling -------------------------------
    // A Matrix44Affine's rows carry no w, so the console vrlimi's 0 into rows 0..2 and 1 into
    // row 3 before concatenating with the projection.
    const SparkFrameData& lrNewestFrame = lrFrameData.GetFrame(0);
    CGS_ASSERT((luLastFrame - 1) < knSparksMaxBlurFrames, "luFrameId < knSparksMaxBlurFrames");
    const SparkFrameData& lrOldestFrame = lrFrameData.GetFrame(luLastFrame - 1);

    Vector4 laCullMatrixA[4];
    Vector4 laCullMatrixB[4];
    {
        const rw::math::vpu::Matrix44& lrProjection = lrNewestFrame.mProjectionMatrix;

        const Vector4 laViewA[4] = {
            { lrNewestFrame.mViewMatrix.xAxis.x, lrNewestFrame.mViewMatrix.xAxis.y, lrNewestFrame.mViewMatrix.xAxis.z, 0.0f },
            { lrNewestFrame.mViewMatrix.yAxis.x, lrNewestFrame.mViewMatrix.yAxis.y, lrNewestFrame.mViewMatrix.yAxis.z, 0.0f },
            { lrNewestFrame.mViewMatrix.zAxis.x, lrNewestFrame.mViewMatrix.zAxis.y, lrNewestFrame.mViewMatrix.zAxis.z, 0.0f },
            { lrNewestFrame.mViewMatrix.wAxis.x, lrNewestFrame.mViewMatrix.wAxis.y, lrNewestFrame.mViewMatrix.wAxis.z, 1.0f },
        };
        const Vector4 laViewB[4] = {
            { lrOldestFrame.mViewMatrix.xAxis.x, lrOldestFrame.mViewMatrix.xAxis.y, lrOldestFrame.mViewMatrix.xAxis.z, 0.0f },
            { lrOldestFrame.mViewMatrix.yAxis.x, lrOldestFrame.mViewMatrix.yAxis.y, lrOldestFrame.mViewMatrix.yAxis.z, 0.0f },
            { lrOldestFrame.mViewMatrix.zAxis.x, lrOldestFrame.mViewMatrix.zAxis.y, lrOldestFrame.mViewMatrix.zAxis.z, 0.0f },
            { lrOldestFrame.mViewMatrix.wAxis.x, lrOldestFrame.mViewMatrix.wAxis.y, lrOldestFrame.mViewMatrix.wAxis.z, 1.0f },
        };

        for (u32 luRow = 0; luRow < 4; ++luRow)
        {
            laCullMatrixA[luRow] = VecMulAdd(lrProjection.wAxis, laViewA[luRow].w,
                                   VecMulAdd(lrProjection.zAxis, laViewA[luRow].z,
                                   VecMulAdd(lrProjection.yAxis, laViewA[luRow].y,
                                             VecScale(lrProjection.xAxis, laViewA[luRow].x))));
            laCullMatrixB[luRow] = VecMulAdd(lrProjection.wAxis, laViewB[luRow].w,
                                   VecMulAdd(lrProjection.zAxis, laViewB[luRow].z,
                                   VecMulAdd(lrProjection.yAxis, laViewB[luRow].y,
                                             VecScale(lrProjection.xAxis, laViewB[luRow].x))));
        }
    }

    // ---- 5. per bucket ----------------------------------------------------------------------
    for (const SparkBucket* lpBucket = lrBank.mpBuckets;
         lpBucket != 0;
         lpBucket = static_cast<const SparkBucket*>(lpBucket->mpNextBucket))
    {
        // ---- 5a. cull, into an index list --------------------------------------------------
        u32 lauVisible[SparkBucket::KuMaxNumParticles];
        u32 luVisibleCount = 0;

        for (u32 luParticle = 0;
             luParticle < lpBucket->mu16NumberOfParticlesInBucket;
             ++luParticle)
        {
            CGS_ASSERT(lpBucket->mu16NumberOfParticlesInBucket <= SparkBucket::KuMaxNumParticles,
                       "mu16NumberOfParticlesInBucket <= KuMaxNumParticles");
            const u32 luIndex = luParticle & 0xFFu;
            CGS_ASSERT(luIndex < SparkBucket::KuMaxNumParticles, "luParticle < KuMaxNumParticles");

            const f32 lfBirthTime = lpBucket->maParticleBirthTimes[luIndex];
            // Not born yet in this ring's newest frame.
            if (lfBirthTime > lfNewestTime)
                continue;
            // Dead, with the whole blur window already past it.
            if ((lafLifetimes[luParticle & 3] + lfBirthTime + lfMotionBlurWindow) <= lfNewestTime)
                continue;

            CGS_ASSERT(luIndex < SparkBucket::KuMaxNumParticles, "luParticle < KuMaxNumParticles");
            const BrnSpark& lrSpark = lpBucket->maParticleData[luIndex];

            const Vector4 lNewest = CalculateSparkPosition(
                lrSpark.mPosition, lrSpark.mVelocity, VecSplat(lfNewestTime - lfBirthTime),
                lHalfGravityVec, lDragDurationVec, lDragParams);
            const Vector4 lOldest = CalculateSparkPosition(
                lrSpark.mPosition, lrSpark.mVelocity, VecSplat(lfOldestCullTime - lfBirthTime),
                lHalfGravityVec, lDragDurationVec, lDragParams);

            const Vector4 lClipA = TransformPoint(laCullMatrixA[0], laCullMatrixA[1],
                                                  laCullMatrixA[2], laCullMatrixA[3], lNewest);
            const Vector4 lClipB = TransformPoint(laCullMatrixB[0], laCullMatrixB[1],
                                                  laCullMatrixB[2], laCullMatrixB[3], lOldest);

            // Six frustum tests, each "reject only if BOTH ends fail": behind the camera,
            // left of -w, right of +w, below -w, above +w. The console spells each pair as
            // vcmpgtfp./vcmpgefp. on splatted lanes with the sign flipped by vxor against
            // 0x80000000 (`vslw v9, -1, -1`).
            if (!(lClipA.w > 0.0f) && !(lClipB.w > 0.0f))
                continue;
            if (!(lClipA.x >= -lClipA.w) && !(lClipB.x >= -lClipB.w))
                continue;
            if (!(lClipA.w >= lClipA.x) && !(lClipB.w >= lClipB.x))
                continue;
            if (!(lClipA.y >= -lClipA.w) && !(lClipB.y >= -lClipB.w))
                continue;
            if (!(lClipA.w >= lClipA.y) && !(lClipB.w >= lClipB.y))
                continue;

            CGS_ASSERT(luVisibleCount < SparkBucket::KuMaxNumParticles,
                       "luNumSparksToRender < KU_SPARK_BUCKET_SIZE");
            lauVisible[luVisibleCount] = luParticle;
            ++luVisibleCount;
        }

        // ---- 5b. is there room? --------------------------------------------------------------
        // (luFrameCount + 1) * visible * 2 vertices; the console bails out of the WHOLE bank,
        // not just this bucket, and says so on the debug print stream.
        const u32 luVerticesFree   = lrIterator.GetVerticesFree();
        const u32 luVerticesNeeded = (luFrameCount + 1) * luVisibleCount * 2;
        if (luVerticesFree < luVerticesNeeded)
        {
            CgsDev::Log::WriteToLog("Sparks: Particle vertex buffer full.");
            return;
        }
        if (luVisibleCount == 0)
            continue;

        // ---- 5c. per visible spark: build and emit the ribbon --------------------------------
        for (u32 luVisible = 0; luVisible < luVisibleCount; ++luVisible)
        {
            const u32 luParticle = lauVisible[luVisible];
            const u32 luIndex    = luParticle & 0xFFu;
            CGS_ASSERT(luIndex < SparkBucket::KuMaxNumParticles, "luParticle < KuMaxNumParticles");

            const f32 lfLifetime  = lafLifetimes[luParticle & 3];
            const f32 lfBirthTime = lpBucket->maParticleBirthTimes[luIndex];

            CGS_ASSERT(luIndex < SparkBucket::KuMaxNumParticles, "luParticle < KuMaxNumParticles");
            const BrnSpark& lrSpark = lpBucket->maParticleData[luIndex];

            // (i) one sample per ring frame, in THAT frame's view space.
            SparkRibbonPoint laPoints[knSparksMaxBlurFrames];
            for (u32 luFrame = 0; luFrame < luFrameCount; ++luFrame)
            {
                CGS_ASSERT(luFrame < knSparksMaxBlurFrames, "luFrameId < knSparksMaxBlurFrames");
                const SparkFrameData& lrFrame = lrFrameData.GetFrame(luFrame);
                const f32 lfAge = lrFrame.mfTimeStamp - lfBirthTime;
                laPoints[luFrame].mfAge = lfAge;

                CGS_ASSERT(luFrame < knSparksMaxBlurFrames, "luFrameId < knSparksMaxBlurFrames");
                const Vector4 lWorld = CalculateSparkPosition(
                    lrSpark.mPosition, lrSpark.mVelocity, VecSplat(lfAge),
                    lHalfGravityVec, lDragDurationVec, lDragParams);

                const rw::math::vpu::Matrix44Affine& lrView = lrFrame.mViewMatrix;
                const Vector4 laViewRows[4] = {
                    { lrView.xAxis.x, lrView.xAxis.y, lrView.xAxis.z, lrView.xAxis.w },
                    { lrView.yAxis.x, lrView.yAxis.y, lrView.yAxis.z, lrView.yAxis.w },
                    { lrView.zAxis.x, lrView.zAxis.y, lrView.zAxis.z, lrView.zAxis.w },
                    { lrView.wAxis.x, lrView.wAxis.y, lrView.wAxis.z, lrView.wAxis.w },
                };
                laPoints[luFrame].mViewPosition = TransformPoint(
                    laViewRows[0], laViewRows[1], laViewRows[2], laViewRows[3], lWorld);
            }

            // (ii) the streak direction: a central difference along the ring, forward at the
            // head and backward at the tail.
            laPoints[0].mTangent = VecSub(laPoints[1].mViewPosition, laPoints[0].mViewPosition);
            for (u32 luFrame = 1; luFrame + 1 < luFrameCount; ++luFrame)
            {
                laPoints[luFrame].mTangent = VecSub(laPoints[luFrame + 1].mViewPosition,
                                                    laPoints[luFrame - 1].mViewPosition);
            }
            laPoints[luFrameCount - 1].mTangent =
                VecSub(laPoints[luFrameCount - 1].mViewPosition,
                       laPoints[luFrameCount - 2].mViewPosition);

            // (iii) the ribbon's width direction: normalize(viewPosition x tangent). The
            // console builds the cross product with two vpermwi128 0x63 word rotations
            // (y,z,x,w) and one vnmsubfp, then normalises with vrsqrtefp plus TWO
            // Newton-Raphson refinements. A tangent whose cross product is entirely under
            // FLT_EPSILON is left alone (degenerate: the spark has not moved).
            for (u32 luFrame = 0; luFrame < luFrameCount; ++luFrame)
            {
                const Vector4& lrP = laPoints[luFrame].mViewPosition;
                const Vector4& lrT = laPoints[luFrame].mTangent;
                Vector4 lCross = { lrP.y * lrT.z - lrP.z * lrT.y,
                                   lrP.z * lrT.x - lrP.x * lrT.z,
                                   lrP.x * lrT.y - lrP.y * lrT.x,
                                   0.0f };
                const f32 lfLenSquared = lCross.x * lCross.x + lCross.y * lCross.y
                                       + lCross.z * lCross.z;
                if (std::fabs(lCross.x) > KF_TANGENT_EPSILON
                    || std::fabs(lCross.y) > KF_TANGENT_EPSILON
                    || std::fabs(lCross.z) > KF_TANGENT_EPSILON)
                {
                    if (lfLenSquared > 0.0f)
                        laPoints[luFrame].mTangent = VecScale(lCross, 1.0f / std::sqrt(lfLenSquared));
                }
            }

            // (iv) the two ribbon edges, for every sample still inside the spark's lifetime.
            const f32 lfHalfWidth = lrSpark.mfSize * lfSparkRadius;
            Vector4 laEdgeA[knSparksMaxBlurFrames + 1];
            Vector4 laEdgeB[knSparksMaxBlurFrames + 1];
            f32     lafEdgeU[knSparksMaxBlurFrames + 1];
            u32     luEdgeCount = 0;
            for (u32 luFrame = 0; luFrame < luFrameCount; ++luFrame)
            {
                const f32 lfAge = laPoints[luFrame].mfAge;
                if (lfAge >= lfLifetime)
                    continue;
                if (lfAge <= 0.0f)
                    continue;

                const Vector4 lOffset = VecScale(laPoints[luFrame].mTangent, lfHalfWidth);
                laEdgeA[luEdgeCount]  = VecAdd(laPoints[luFrame].mViewPosition, lOffset);
                laEdgeB[luEdgeCount]  = VecSub(laPoints[luFrame].mViewPosition, lOffset);
                lafEdgeU[luEdgeCount] = lafFrameU[luFrame];
                ++luEdgeCount;
            }
            if (luEdgeCount < 2)
                continue;

            // (v) the tail is CLIPPED back to u == 1: the last pair is lerped between the last
            // two samples so the ribbon ends exactly at the blur window rather than past it.
            const u32 luLast = luEdgeCount - 1;
            const f32 lfULast = (lafEdgeU[luLast] > 1.0f) ? 1.0f : lafEdgeU[luLast];
            {
                const f32 lfUPrev = lafEdgeU[luLast - 1];
                f32 lfT = (1.0f - lfUPrev) / (lafEdgeU[luLast] - lfUPrev);
                if (lfT > 1.0f)
                    lfT = 1.0f;
                laEdgeA[luLast] = VecAdd(VecScale(VecSub(laEdgeA[luLast], laEdgeA[luLast - 1]), lfT),
                                         laEdgeA[luLast - 1]);
                laEdgeB[luLast] = VecAdd(VecScale(VecSub(laEdgeB[luLast], laEdgeB[luLast - 1]), lfT),
                                         laEdgeB[luLast - 1]);
                lafEdgeU[luLast] = lfULast;
            }

            // (vi) emit. The strip opens with a duplicate of edge A's first vertex and closes
            // with a duplicate of edge B's last, so consecutive sparks in the same batch are
            // joined by degenerate triangles.
            const u32 luColourWord = SwizzleVertexColour(lauPackedColours[luParticle & 3]);
            f32 lafUv[2];

            lafUv[0] = lafEdgeU[0];
            lafUv[1] = 0.0f;
            lrIterator.Write(laEdgeA[0], reinterpret_cast<const int*>(&luColourWord), lafUv);

            for (u32 luEdge = 0; luEdge < luEdgeCount; ++luEdge)
            {
                lafUv[0] = lafEdgeU[luEdge];
                lafUv[1] = 0.0f;
                lrIterator.Write(laEdgeA[luEdge], reinterpret_cast<const int*>(&luColourWord), lafUv);
                lafUv[1] = 1.0f;
                lrIterator.Write(laEdgeB[luEdge], reinterpret_cast<const int*>(&luColourWord), lafUv);
            }

            lafUv[0] = lafEdgeU[luLast];
            lafUv[1] = 1.0f;
            lrIterator.Write(laEdgeB[luLast], reinterpret_cast<const int*>(&luColourWord), lafUv);
        }
    }
}

// =============================================================================================
// SparkVertexBufferBuilder::BuildDispatchData @0x82920D78.
//
// One batch per spark array. The regular bank always renders; the crash bank renders into the
// SAME batch when the caller asks for it, so a crash spark and an ordinary spark of the same
// array share one draw and one texture.
//
// lfMotionBlurScale is the caller's quality dial: the window handed to RenderBank is
// mfMotionBlurTime * (scale * 0.875 + 0.125) (`fmadds f29, f31, flt_8200D534, flt_82101594`),
// i.e. between an eighth and the whole of the array's authored blur time.
// =============================================================================================
void SparkVertexBufferBuilder::BuildDispatchData(EffectsVertexBufferLocked* lpLockedBuffer,
                                                 SparkBatchArray&           lrBatches,
                                                 SparkArray*                lpSparkArrays,
                                                 u32                        luNumSparkArrays,
                                                 f32                        lfMotionBlurScale,
                                                 const SparkFrameDataSet&   lrFrameData,
                                                 f32                        lfWhiteLevel,
                                                 bool                       lbRenderCrashBanks)
{
    CGS_ASSERT(luNumSparkArrays == eSparkArray_Max, "luNumSparkArrays == eSparkArray_Max");

    const f32 lfBlurFraction = (lfMotionBlurScale * 0.875f) + 0.125f;

    for (u32 luArray = 0; luArray < luNumSparkArrays; ++luArray)
    {
        SparkArray& lrArray = lpSparkArrays[luArray];

        NativeParticleVertex::VertexIterator lIterator;
        SparkBatch                           lBatch;
        lpLockedBuffer->BeginBatch(lIterator, lBatch, NativeParticleVertex::GetStride());

        const f32 lfBlurWindow = lrArray.mfMotionBlurTime * lfBlurFraction;

        lrArray.RenderBank(lIterator, lpLockedBuffer, &lBatch, lrArray.mRegularBank,
                           lrArray.mfSparkRadius, lrFrameData, lfBlurWindow, lfWhiteLevel);
        if (lbRenderCrashBanks)
        {
            lrArray.RenderBank(lIterator, lpLockedBuffer, &lBatch, lrArray.mCrashBank,
                               lrArray.mfSparkRadius, lrFrameData, lfBlurWindow, lfWhiteLevel);
        }

        lpLockedBuffer->EndBatch(lIterator, lBatch, NativeParticleVertex::GetStride());

        if (lBatch.muVertexCount != 0)
        {
            lBatch.meArrayId = static_cast<ESparkArrayID>(luArray);
            lrBatches.Append(lBatch);
        }
    }
}

} // namespace Native
} // namespace BrnParticle
