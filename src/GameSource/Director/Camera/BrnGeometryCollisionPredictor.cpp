// ============================================================================
// GameSource/Director/Camera/BrnGeometryCollisionPredictor.cpp
//
// Compilation home for BrnDirector::Camera::GeometryCollisionPredictor (DWARF
// BrnCollisionPolicy.h:133):
//   - GeometryCollisionPredictor::GetTimeUntilCollision     @0x821F36C0
//   - GeometryCollisionPredictor::GenerateSceneQueries      (inlined on the console; FX-DIRECTOR2 2026-09-25)
//   - GeometryCollisionPredictor::ProcessSceneQueryResults  @0x8220E1B8 (FX-DIRECTOR2 2026-09-25)
//
// Driven by BrnDirector::Camera::VisibilityCollisionPolicy::GenerateSceneQueries /
// ::ProcessSceneQueryResults.
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"
#include "GameSource/Director/Camera/Camera.h"                   // Camera (the position lane)
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"   // BrnDirector::SceneQueryInterface::LineTestNearest

namespace BrnDirector
{
namespace Camera
{

// KF_LOOKAHEAD_TIME_FLOAT (DWARF :208). The body reads its VecFloat twin at 0x82FAA9A0, which the
// dyn-init at 0x82C49170 fills with the splat of the float at 0x82001B6C == 0x3F800000.
const f32 GeometryCollisionPredictor::KF_LOOKAHEAD_TIME_FLOAT = 1.0f;

// ----------------------------------------------------------------------------
// BrnDirector::Camera::GeometryCollisionPredictor::GetTimeUntilCollision @0x821F36C0
//   lbz   r11, 0x64(this)      ; mbWillCollide
//   cmplwi r11, 0              ; assert it is set (a collision was predicted)
//   bne   skip                 ; (asm asserts when CLEAR, i.e. !mbWillCollide)
//   ... Begin/Fire/End assert ...
//   lfs   f1, 0x60(this)       ; return mfTimeUntilCollision
// ----------------------------------------------------------------------------
f32 GeometryCollisionPredictor::GetTimeUntilCollision() const
{
    CGS_ASSERT(mbWillCollide, "mbWillCollide");   // lbz 0x64; assert set
    return mfTimeUntilCollision;                  // lfs f1, 0x60(this)
}

// ----------------------------------------------------------------------------
// GeometryCollisionPredictor::GenerateSceneQueries (DWARF BrnCollisionPolicy.h:192) -- no
// out-of-line X360 copy; VisibilityCollisionPolicy::GenerateSceneQueries inlines it
// (0x82240468..0x822404AC):
//   start = camera.w                                        (lvx128 v1, camera, 0x30)
//   end   = mVelocity * KF_LOOKAHEAD_TIME + start           (vmaddfp v2 = v13 * v0 + v1 -- the raw
//                                                            fields D=2 A=13 B=1 C=0, vmx128.py)
//   request->LineTestNearest(mLineTest, 2 (world), 0xFF, start, end,
//                            0xFFFFFFFF (@0x82CDA790, no entity excluded), 0 (E_EXCLUDE_ENTITY_ONLY))
// The timestep and the Random are part of the DWARF signature and unused.
// ----------------------------------------------------------------------------
void GeometryCollisionPredictor::GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep,
                                                      CgsNumeric::Random& lrRandom,
                                                      const SceneQueryInterface* lpRequestInterface)
{
    (void)lfTimestep;
    (void)lrRandom;

    const rw::math::vpu::Vector3& lrStart = lrCamera.mTransform.wAxis;

    rw::math::vpu::Vector3 lEnd;
    lEnd.x = mVelocity.x * KF_LOOKAHEAD_TIME_FLOAT + lrStart.x;
    lEnd.y = mVelocity.y * KF_LOOKAHEAD_TIME_FLOAT + lrStart.y;
    lEnd.z = mVelocity.z * KF_LOOKAHEAD_TIME_FLOAT + lrStart.z;
    lEnd.w = mVelocity.w * KF_LOOKAHEAD_TIME_FLOAT + lrStart.w;

    lpRequestInterface->LineTestNearest(mLineTest, 2u, 0xFFu, lrStart, lEnd,
                                        CgsSceneManager::EntityId(0xFFFFFFFFu),
                                        CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
}

// ----------------------------------------------------------------------------
// GeometryCollisionPredictor::ProcessSceneQueryResults @0x8220E1B8 (DWARF BrnCollisionPolicy.h:196)
//   assert mLineTest.HasPackage()                     (BrnCollisionPolicy.cpp:538, 0x21A)
//   hit  -> mfTimeUntilCollision = the hit's line parameter (+0x50 == package +0x30 -- a fraction of
//           the 1 s lookahead line, i.e. seconds); mbWillCollide = true; box emptied
//   miss -> box emptied; mbWillCollide = false
// The timestep is part of the DWARF signature and unused.
// ----------------------------------------------------------------------------
void GeometryCollisionPredictor::ProcessSceneQueryResults(f32 lfTimestep)
{
    (void)lfTimestep;
    CGS_ASSERT(mLineTest.HasPackage(), "mLineTest.HasPackage()");                   // :538

    if (mLineTest.GetPackage().mbIntersection)
    {
        mfTimeUntilCollision = mLineTest.GetPackage().mfLineParam;                  // lfs 0x50 ; stfs 0x60
        mbWillCollide        = true;                                                // stb 1, 0x64
        mLineTest.Clear();                                                          // stw 0, 0x10
    }
    else
    {
        mLineTest.Clear();                                                          // stw 0, 0x10
        mbWillCollide = false;                                                      // stb 0, 0x64
    }
}

} // namespace Camera
} // namespace BrnDirector
