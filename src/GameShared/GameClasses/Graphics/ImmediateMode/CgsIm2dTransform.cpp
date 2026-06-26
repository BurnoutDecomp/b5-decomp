// CgsGraphics::Im2dTransform - the screen-space transform an Im2d batch renders through.
// This TU homes the class-wide aspect-correction matrix storage and the in-place
// aspect-ratio fold.
//
// TransformByAspectRatio @0x823DB448: folds the current display aspect ratio into the
// transform in place. The X360 builds a 3x3 scalar (rw::math::fpu) affine from the
// transform's Right/Up basis + Origin, multiplies it by the class aspect-correction
// matrix (msAspectCorrectionMatrix, the static @0x830110F0), and stores the basis and
// origin rows of the product back into the transform.
//
// The console writes the two destination registers with quad-word lvx128/stvx128 pairs
// (16 bytes each into mRightUp@+0x10 and mOriginXYZ@+0x00); this reconstruction performs
// the equivalent four-lane scalar stores at the same offsets/widths. The IsValid<float>
// guards (@0x8231A5C8, the per-lane x==x && y==y && z==z self-equality NaN test) and their
// baked assert operands (file CgsIm2dTransform.h, lines 206 and 219; messages
// "IsValid(lMatrix)" and "IsValid(lSrc)") are preserved exactly: each guard fires the
// assert when any row of the matrix/product carries a NaN lane.
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsGraphics
{
    // X360 static @0x830110F0 (nine floats). Zero-initialised here; populated at runtime
    // by Im2dTransform::SetAspectCorrectionMatrix(const Matrix33&) @0x827F98C0.
    rw::math::fpu::Matrix33Template<float> Im2dTransform::msAspectCorrectionMatrix = {};

    void Im2dTransform::TransformByAspectRatio()
    {
        using rw::math::fpu::Matrix33Template;
        using rw::math::fpu::Vector3Template;

        // Validate the aspect-correction matrix (lMatrix), one Vector3 row at a time
        // (rows @+0x00/+0x0C/+0x18 of the static). @0x823DB46C..0x823DB494.
        const Vector3Template<float> lMatrixRow0(
            msAspectCorrectionMatrix.mRow0X, msAspectCorrectionMatrix.mRow0Y, msAspectCorrectionMatrix.mRow0Z);
        const Vector3Template<float> lMatrixRow1(
            msAspectCorrectionMatrix.mRow1X, msAspectCorrectionMatrix.mRow1Y, msAspectCorrectionMatrix.mRow1Z);
        const Vector3Template<float> lMatrixRow2(
            msAspectCorrectionMatrix.mRow2X, msAspectCorrectionMatrix.mRow2Y, msAspectCorrectionMatrix.mRow2Z);
        if (!rw::math::fpu::IsValid(lMatrixRow0)
            || !rw::math::fpu::IsValid(lMatrixRow1)
            || !rw::math::fpu::IsValid(lMatrixRow2))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "IsValid(lMatrix)",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsIm2dTransform.h",
                206);
            CgsDev::Assert::EndAssert();
        }

        // Build the source affine 3x3 from the transform: Right=(mRightUp.x,mRightUp.z),
        // Up=(mRightUp.y,mRightUp.w), Origin=(mOriginXYZ.x,mOriginXYZ.y); the implicit
        // homogeneous column is (0,0,1). @0x823DB4E0..0x823DB534.
        Matrix33Template<float> lTransform;
        lTransform.mRow0X = mRightUp.x;
        lTransform.mRow0Y = mRightUp.z;
        lTransform.mRow0Z = 0.0f;
        lTransform.mRow1X = mRightUp.y;
        lTransform.mRow1Y = mRightUp.w;
        lTransform.mRow1Z = 0.0f;
        lTransform.mRow2X = mOriginXYZ.x;
        lTransform.mRow2Y = mOriginXYZ.y;
        lTransform.mRow2Z = 1.0f;

        // lSrc = lTransform * lMatrix (the aspect-ratio fold). @0x823DB538.
        const Matrix33Template<float> lSrc =
            rw::math::fpu::Mult(lTransform, msAspectCorrectionMatrix);

        // Validate the product (lSrc), one row at a time. @0x823DB588..0x823DB5B0.
        const Vector3Template<float> lSrcRow0(lSrc.mRow0X, lSrc.mRow0Y, lSrc.mRow0Z);
        const Vector3Template<float> lSrcRow1(lSrc.mRow1X, lSrc.mRow1Y, lSrc.mRow1Z);
        const Vector3Template<float> lSrcRow2(lSrc.mRow2X, lSrc.mRow2Y, lSrc.mRow2Z);
        if (!rw::math::fpu::IsValid(lSrcRow0)
            || !rw::math::fpu::IsValid(lSrcRow1)
            || !rw::math::fpu::IsValid(lSrcRow2))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "IsValid(lSrc)",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsIm2dTransform.h",
                219);
            CgsDev::Assert::EndAssert();
        }

        // Store the product's basis rows back into mRightUp (16B @+0x10) and its origin row
        // into mOriginXYZ (16B @+0x00), preserving mOriginXYZ.z and clearing the 4th origin
        // lane. @0x823DB5F0..0x823DB628 (stvx128 to r30=this+0x10 and r31=this).
        mRightUp.x = lSrc.mRow0X;
        mRightUp.y = lSrc.mRow0Y;
        mRightUp.z = lSrc.mRow1X;
        mRightUp.w = lSrc.mRow1Y;

        mOriginXYZ.x = lSrc.mRow2X;
        mOriginXYZ.y = lSrc.mRow2Y;
        // mOriginXYZ.z preserved (reloaded from [8](this) and re-stored).
        mOriginXYZ.w = 0.0f;
    }
}
