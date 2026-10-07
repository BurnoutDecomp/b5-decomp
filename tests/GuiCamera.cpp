// GetGuiCamera uses the real Camera construction/LookAt implementations. Reference
// matrices are derived independently from the raw ARTIST authored parameters.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GuiCameraReference.h"

static unsigned checks, failures, asserts;
static void Check(bool good, const char* why)
{
    ++checks;
    if (!good) { ++failures; std::printf("FAIL %s\n", why); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* expression, const char*, int)
{
    ++asserts;
    Check(std::strcmp(expression, "Bad GuiCamera type\n") == 0, "invalid selector uses the original diagnostic");
    return 0;
}
void* EndAssert() { return nullptr; }
} }

#include "gui_camera.inc"

static_assert(std::is_same<decltype(CgsGui::GetGuiCamera()), CgsGraphics::Camera>::value,
              "original no-argument function returns the full camera by value");
static_assert(CgsGui::E_GUICAMERA_FULLSCREENMAP == 0 && CgsGui::E_GUICAMERA_NORMAL == 1,
              "ARTIST compares selector values 0 and 1");
static f32 Float(u32 bits) { f32 value; std::memcpy(&value, &bits, sizeof(value)); return value; }
static f64 Double(u64 bits) { f64 value; std::memcpy(&value, &bits, sizeof(value)); return value; }
static u32 Bits(f32 value) { u32 bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
static bool Close(f32 a, f32 b)
{
    const f32 scale = std::fabs(b) > 1.0f ? std::fabs(b) : 1.0f;
    return std::isfinite(a) && std::fabs(a - b) <= 3.0e-6f * scale;
}
static void CheckCamera(const CgsGraphics::Camera& camera, unsigned selector)
{
    const auto& reference = K_REFERENCE_GUI_CAMERAS[selector];
    const Matrix44* matrices[] = {&camera.mView, &camera.mProjection, &camera.mViewProjection};
    for (unsigned matrix = 0; matrix < 3; ++matrix)
    {
        const Vector4* rows[] = {&matrices[matrix]->xAxis, &matrices[matrix]->yAxis,
                                &matrices[matrix]->zAxis, &matrices[matrix]->wAxis};
        for (unsigned row = 0; row < 4; ++row)
        {
            const f32 values[] = {rows[row]->x, rows[row]->y, rows[row]->z, rows[row]->w};
            for (unsigned lane = 0; lane < 4; ++lane)
                Check(Close(values[lane], Float(reference.mauMatrices[matrix * 16 + row * 4 + lane])),
                      "view, projection and combined matrix match original parameters");
        }
    }
    for (unsigned index = 0; index < 16; ++index)
    {
        const f64 expected = Double(reference.mauView64[index]);
        const f64 scale = std::fabs(expected) > 1.0 ? std::fabs(expected) : 1.0;
        Check(std::isfinite(camera.maClipState[index])
              && std::fabs(camera.maClipState[index] - expected) <= 2.0e-12 * scale,
              "complete double-precision view matrix matches authored eye/up/target");
    }
    for (unsigned index = 0; index < 9; ++index)
        Check(Close(camera.maProjectionScalars[index], Float(reference.mauScalars[index])),
              "all cached projection scalars match original camera construction");
    const unsigned authored[] = {0, 6, 7, 8};
    for (unsigned index : authored)
        Check(Bits(camera.maProjectionScalars[index]) == reference.mauScalars[index],
              "FOV, aspect and near/far constants retain their exact ARTIST bits");
    Check(camera.mProjection.zAxis.w == 1.0f && camera.mProjection.wAxis.w == 0.0f,
          "both ARTIST modes retain perspective projection");
}
int main()
{
    const CgsGui::EGuiCameraType selectors[] = {CgsGui::E_GUICAMERA_NORMAL,
        CgsGui::E_GUICAMERA_FULLSCREENMAP, CgsGui::E_GUICAMERA_NORMAL, CgsGui::E_GUICAMERA_FULLSCREENMAP};
    CgsGraphics::Camera reused;
    for (auto selector : selectors)
    {
        std::memset(&reused, 0xA5, sizeof(reused));
        Check(CgsGui::SetGuiCamera(selector) == selector, "selector setter retains its argument ABI");
        reused = CgsGui::GetGuiCamera();
        CheckCamera(reused, static_cast<unsigned>(selector));
        Check(CgsGui::gCurrentGuiCamera == selector, "getter leaves the original selected mode intact");
    }
    Check(asserts == 0, "both original modes satisfy the real camera assertions");
    const s32 invalidSelectors[] = {2, -1};
    for (s32 invalid : invalidSelectors)
    {
        CgsGui::gCurrentGuiCamera = static_cast<CgsGui::EGuiCameraType>(invalid);
        const unsigned before = asserts;
        (void)CgsGui::GetGuiCamera(); // Invalid return storage is intentionally not read.
        Check(asserts == before + 1, "invalid selector asserts once");
        Check(static_cast<s32>(CgsGui::gCurrentGuiCamera) == invalid, "invalid selector is not silently replaced");
    }
    std::printf("GuiCamera: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
