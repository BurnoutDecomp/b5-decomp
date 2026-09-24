// FX-TAILS-B (crash parity 2026-09-24, item 2): CollisionStateManager::SetCameraInfo @0x8269FFD0
// against the ARTIST machine code.
//
//   0x8269FFEC..0x826A0024  the camera's four transform rows (+0x00..+0x30) -> mCameraInfo (+0x8140)
//   0x826A002C..0x826A0030  mfFieldOfView (+0x8180) = the camera's FOV (+0x58)
//   0x826A0034..0x826A004C  mfCosineHalfFov (+0x8184) = frsp(cos((double)(FOV * flt_82002518)))
//                           -- `fmuls` (single), then the DOUBLE `cos` @0x82C096A0, then `frsp`
//   0x826A0050..0x826A0094  mfAspectRatio (+0x8188) = the camera's CURRENT flags (`ld 0x140`) bit 0
//                           (E_FLAG_WIDESCREEN, `clrldi 63`) ? SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN
//                           (unk_83005F30) : _NORMAL (unk_830085F0), lane x (`lfs 0(r11)`)
//   0x826A0098..0x826A00A8  mfZoom (+0x818C) = GetZoomFromFOVDegs(the camera's FOV)
// The two VecFloats (DWARF BrnCollisionStateManager.cpp:82 / :83, class statics) are .bss splats
// written by the CRT thunks 0x82C63278 (NORMAL <- 0x820AA254 = 0x3FAAAAAB, 4/3) and 0x82C63298
// (WIDESCREEN <- 0x820AA250 = 0x3FE38E39, 16/9).
//
// run_fxtailsb_camera_info.py extracts the PRODUCTION SetCameraInfo and the two static definitions
// (when the revision has them -- FXTAILSB_HAVE_STATICS) from BrnCollisionStateManager.cpp. The camera
// state is the real BrnCameraState.h; the camera is a fixture with the members SetCameraInfo reaches,
// including the camera's own mfAspectRatio (+0x5C), which the console never reads.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/BrnCameraState.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

static int giZoomCalls = 0;
static f32 gfZoomArgument = 0.0f;

namespace BrnDirector { namespace Camera {
// The director camera, as far as SetCameraInfo reads it (Camera.h: mTransform +0x00, mfFOV +0x58,
// mfAspectRatio +0x5C, mState +0x138 whose current flags are +0x140).
class Camera
{
public:
    Matrix44Affine mTransform;
    f32 mfFOV;
    f32 mfAspectRatio;
    CameraState mState;
    const Matrix44Affine& GetTransform() const { return mTransform; }
    f32 GetFOV() const { return mfFOV; }
    const CameraState& GetState() const { return mState; }
};
namespace Utils {
f32 GetZoomFromFOVDegs(f32 lfFieldOfView) { ++giZoomCalls; gfZoomArgument = lfFieldOfView; return 1000.0f / lfFieldOfView; }
}
} }

namespace BrnSound { namespace Logic { namespace Collision {

struct CameraInfo
{
    Matrix44Affine mTransform;
    f32 mfFieldOfView, mfCosineHalfFov, mfAspectRatio, mfZoom;
};

class CollisionStateManager
{
public:
    CameraInfo mCameraInfo = {};
    static VecFloat SKF32_CAMERA_ASPECT_RATIO_NORMAL;
    static VecFloat SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN;
    void SetCameraInfo(const BrnDirector::Camera::Camera& lrCamera);
};

#include "fxtailsb_aspect_statics.inc"

#include "fxtailsb_set_camera_info.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using namespace BrnSound::Logic::Collision;
using BrnDirector::Camera::CameraState;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}
static u32 Bits(f32 lfValue) { u32 lu; std::memcpy(&lu, &lfValue, 4); return lu; }
static Vector3 V(f32 x, f32 y, f32 z, f32 w) { Vector3 l = { x, y, z, w }; return l; }

static BrnDirector::Camera::Camera MakeCamera(f32 lfFov, u64 lu64Current, u64 lu64Previous, u64 lu64Head, f32 lfDecoy)
{
    BrnDirector::Camera::Camera l;
    l.mTransform.xAxis = V(1, 2, 3, 4);
    l.mTransform.yAxis = V(5, 6, 7, 8);
    l.mTransform.zAxis = V(9, 10, 11, 12);
    l.mTransform.wAxis = V(13, 14, 15, 16);
    l.mfFOV = lfFov;
    l.mfAspectRatio = lfDecoy;
    l.mState.mHeadFlags.SetBitField(0, lu64Head);
    l.mState.mCurrentFlags.SetBitField(0, lu64Current);
    l.mState.mPreviousFlags.SetBitField(0, lu64Previous);
    return l;
}

static const u32 KU_WIDESCREEN_BITS = 0x3FE38E39u;   // 0x820AA250, 16/9
static const u32 KU_NORMAL_BITS = 0x3FAAAAABu;       // 0x820AA254, 4/3

int main()
{
    CollisionStateManager lMgr;

    // ---- the aspect ratio: E_FLAG_WIDESCREEN of the CURRENT flag set --------------------------------
    lMgr.SetCameraInfo(MakeCamera(70.0f, 1ull << CameraState::E_FLAG_WIDESCREEN, 0, 0, 2.5f));
    Check(Bits(lMgr.mCameraInfo.mfAspectRatio) == KU_WIDESCREEN_BITS,
          "aspect: E_FLAG_WIDESCREEN set -> SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN (16/9, 0x3FE38E39), not the camera's own 2.5");
    lMgr.SetCameraInfo(MakeCamera(70.0f, 0, 0, 0, 2.5f));
    Check(Bits(lMgr.mCameraInfo.mfAspectRatio) == KU_NORMAL_BITS,
          "aspect: E_FLAG_WIDESCREEN clear -> SKF32_CAMERA_ASPECT_RATIO_NORMAL (4/3, 0x3FAAAAAB)");
    lMgr.SetCameraInfo(MakeCamera(70.0f, 0, 1ull, 1ull, 16.0f / 9.0f));
    Check(Bits(lMgr.mCameraInfo.mfAspectRatio) == KU_NORMAL_BITS,
          "aspect: only the CURRENT set is read (`ld 0x140`) -- bit 0 in the previous / head sets does not count");
    lMgr.SetCameraInfo(MakeCamera(70.0f, 0x3FFFFFFEull, 0, 0, 1.0f));
    Check(Bits(lMgr.mCameraInfo.mfAspectRatio) == KU_NORMAL_BITS,
          "aspect: only bit 0 decides (`clrldi 63`): every other flag set, bit 0 clear -> NORMAL");
    lMgr.SetCameraInfo(MakeCamera(70.0f, 0x3FFFFFFFull, 0, 0, 1.0f));
    Check(Bits(lMgr.mCameraInfo.mfAspectRatio) == KU_WIDESCREEN_BITS,
          "aspect: every flag set -> WIDESCREEN");

    // ---- the transform rows, the FOV and the zoom -------------------------------------------------
    giZoomCalls = 0;
    lMgr.SetCameraInfo(MakeCamera(63.5f, 0, 0, 0, 1.0f));
    const Matrix44Affine& lrT = lMgr.mCameraInfo.mTransform;
    Check(lrT.xAxis.x == 1 && lrT.xAxis.w == 4 && lrT.yAxis.y == 6 && lrT.zAxis.z == 11 && lrT.wAxis.x == 13 &&
              lrT.wAxis.w == 16,
          "rows: the camera's four transform rows are copied in order (+0x00..+0x30 -> +0x8140..+0x8170)");
    Check(lMgr.mCameraInfo.mfFieldOfView == 63.5f, "fov: mfFieldOfView = the camera's FOV (+0x58)");
    Check(giZoomCalls == 1 && gfZoomArgument == 63.5f && lMgr.mCameraInfo.mfZoom == 1000.0f / 63.5f,
          "zoom: mfZoom = GetZoomFromFOVDegs(the camera's FOV), called once");

    // ---- the cosine of the half FOV: the double cos, rounded once (`bl cos ; frsp`) --------------
    // The three FOVs are ones where cosf (single precision) and the console's double cos + frsp
    // round to neighbouring floats; 60 is a control both agree on.
    struct CosCase { f32 mfFov; u32 muConsoleBits; };
    const CosCase kaCases[] = { { 93.0f, 0x3F303801u }, { 90.37f, 0x3F346F14u }, { 94.61f, 0x3F2D97ABu } };
    for (const CosCase& lrCase : kaCases)
    {
        lMgr.SetCameraInfo(MakeCamera(lrCase.mfFov, 0, 0, 0, 1.0f));
        char lacLabel[160];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "cos: FOV %.2f -> frsp(cos((double)(FOV * 0.008726646f))) = 0x%08X (got 0x%08X)",
                      static_cast<double>(lrCase.mfFov), lrCase.muConsoleBits, Bits(lMgr.mCameraInfo.mfCosineHalfFov));
        Check(Bits(lMgr.mCameraInfo.mfCosineHalfFov) == lrCase.muConsoleBits, lacLabel);
    }
    lMgr.SetCameraInfo(MakeCamera(60.0f, 0, 0, 0, 1.0f));
    Check(lMgr.mCameraInfo.mfCosineHalfFov ==
              static_cast<f32>(std::cos(static_cast<double>(60.0f * 0.0087266462f))),
          "cos: FOV 60 -> cos(30 degrees) through the image's pi/360 (flt_82002518 = 0x3C0EFA35)");

    // ---- the two class statics carry the image's splats --------------------------------------------
#ifdef FXTAILSB_HAVE_STATICS
    const VecFloat& lrN = CollisionStateManager::SKF32_CAMERA_ASPECT_RATIO_NORMAL;
    const VecFloat& lrW = CollisionStateManager::SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN;
    Check(Bits(lrN.x) == KU_NORMAL_BITS && Bits(lrN.y) == KU_NORMAL_BITS && Bits(lrN.z) == KU_NORMAL_BITS &&
              Bits(lrN.w) == KU_NORMAL_BITS && Bits(lrW.x) == KU_WIDESCREEN_BITS && Bits(lrW.y) == KU_WIDESCREEN_BITS &&
              Bits(lrW.z) == KU_WIDESCREEN_BITS && Bits(lrW.w) == KU_WIDESCREEN_BITS,
          "statics: SKF32_CAMERA_ASPECT_RATIO_NORMAL = splat(0x3FAAAAAB), _WIDESCREEN = splat(0x3FE38E39)");
#else
    Check(false, "statics: SKF32_CAMERA_ASPECT_RATIO_NORMAL / _WIDESCREEN are not defined in this revision");
#endif

    std::printf("FxTailsBCameraInfo: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
