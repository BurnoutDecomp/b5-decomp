// ARTIST ImRendererSet/GUI module IO: host-width pointers and the original camera semantics.
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"

static unsigned suChecks = 0, suFailures = 0, suAsserts = 0;
static void Check(bool lbPass, const char* lpcMessage)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL %s\n", lpcMessage); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++suAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

#include "gui_im_renderer_set.inc"

using CgsGui::ImRendererSet;
using CgsGraphics::Camera;
using ModuleInput = CgsGui::CgsGuiModuleIO::InputBuffer;
using ViewInput = CgsGui::ViewIO::InputBuffer;

static_assert(std::is_same<ImRendererSet, CgsGui::CgsGuiModuleIO::ImRendererSet>::value,
              "module IO uses the canonical renderer set");
static_assert(std::is_same<ImRendererSet, CgsGui::ViewIO::ImRendererSet>::value,
              "view IO uses the canonical renderer set");

static void FillCamera(Camera& lrCamera, float lfSeed)
{
    Matrix44* lapMatrices[] = {&lrCamera.mView, &lrCamera.mProjection, &lrCamera.mViewProjection};
    for (unsigned luMatrix = 0; luMatrix < 3; ++luMatrix)
    {
        Vector4* lapRows[] = {&lapMatrices[luMatrix]->xAxis, &lapMatrices[luMatrix]->yAxis,
                             &lapMatrices[luMatrix]->zAxis, &lapMatrices[luMatrix]->wAxis};
        for (unsigned luRow = 0; luRow < 4; ++luRow)
            *lapRows[luRow] = Vector4{lfSeed + luMatrix * 16 + luRow * 4,
                lfSeed + luMatrix * 16 + luRow * 4 + 1, lfSeed + luMatrix * 16 + luRow * 4 + 2,
                lfSeed + luMatrix * 16 + luRow * 4 + 3};
    }
    for (unsigned luIndex = 0; luIndex < 16; ++luIndex)
        lrCamera.maClipState[luIndex] = static_cast<double>(lfSeed) + 1000 + luIndex * 0.125;
    for (unsigned luIndex = 0; luIndex < 9; ++luIndex)
        lrCamera.maProjectionScalars[luIndex] = lfSeed + 200 + luIndex;
}

static bool SameCamera(const Camera& lrLeft, const Camera& lrRight)
{
    return std::memcmp(&lrLeft.mView, &lrRight.mView, sizeof(Matrix44)) == 0
        && std::memcmp(&lrLeft.mProjection, &lrRight.mProjection, sizeof(Matrix44)) == 0
        && std::memcmp(&lrLeft.mViewProjection, &lrRight.mViewProjection, sizeof(Matrix44)) == 0
        && std::memcmp(lrLeft.maClipState, lrRight.maClipState, sizeof(lrLeft.maClipState)) == 0
        && std::memcmp(lrLeft.maProjectionScalars, lrRight.maProjectionScalars,
                       sizeof(lrLeft.maProjectionScalars)) == 0;
}

static void FillPointers(ImRendererSet& lrSet, uintptr_t luBase)
{
    lrSet.mpIm2dRenderBuffer = reinterpret_cast<decltype(lrSet.mpIm2dRenderBuffer)>(luBase + 0x101);
    lrSet.mpIm3dRenderBuffer = reinterpret_cast<decltype(lrSet.mpIm3dRenderBuffer)>(luBase + 0x202);
    lrSet.mpIm3dRenderBufferUntex = reinterpret_cast<decltype(lrSet.mpIm3dRenderBufferUntex)>(luBase + 0x303);
    lrSet.mpIm3dRenderBufferRacePosition = reinterpret_cast<decltype(lrSet.mpIm3dRenderBufferRacePosition)>(luBase + 0x404);
    lrSet.mpIm3dRenderBufferMenusAndHud = reinterpret_cast<decltype(lrSet.mpIm3dRenderBufferMenusAndHud)>(luBase + 0x505);
}

static void CheckPointers(const ImRendererSet& lrActual, const ImRendererSet& lrExpected)
{
    Check(lrActual.mpIm2dRenderBuffer == lrExpected.mpIm2dRenderBuffer, "complete Im2d pointer survives publication");
    Check(lrActual.mpIm3dRenderBuffer == lrExpected.mpIm3dRenderBuffer, "complete Im3d pointer survives publication");
    Check(lrActual.mpIm3dRenderBufferUntex == lrExpected.mpIm3dRenderBufferUntex, "complete untextured pointer survives publication");
    Check(lrActual.mpIm3dRenderBufferRacePosition == lrExpected.mpIm3dRenderBufferRacePosition, "complete race-position pointer survives publication");
    Check(lrActual.mpIm3dRenderBufferMenusAndHud == lrExpected.mpIm3dRenderBufferMenusAndHud, "complete menus/HUD pointer survives publication");
}

static void CheckConstructed(const ImRendererSet& lrSet, const Camera& lrExpected,
                             decltype(ImRendererSet::mpIm3dRenderBuffer) lpUntouched)
{
    Check(lrSet.mpIm2dRenderBuffer == nullptr && lrSet.mpIm3dRenderBufferUntex == nullptr
          && lrSet.mpIm3dRenderBufferRacePosition == nullptr
          && lrSet.mpIm3dRenderBufferMenusAndHud == nullptr, "Construct clears the four original renderer slots");
    Check(lrSet.mpIm3dRenderBuffer == lpUntouched, "Construct preserves the original slot-one pointer");
    Check(SameCamera(lrSet.mCamera, lrExpected), "Construct calls the real graphics-camera reset");
    Check(lrSet.mCamera.maProjectionScalars[0] == 1.5707964f
          && lrSet.mCamera.maProjectionScalars[6] == 1.7777778f
          && lrSet.mCamera.maProjectionScalars[7] == 0.1f
          && lrSet.mCamera.maProjectionScalars[8] == 1000.0f,
          "camera reset uses original FOV, aspect and clip distances");
    Check(lrSet.mCamera.mView.xAxis.x == 1.0f && lrSet.mCamera.mView.yAxis.y == 1.0f
          && lrSet.mCamera.mView.zAxis.z == 1.0f && lrSet.mCamera.mView.wAxis.x == 0.0f,
          "camera reset initializes view transform at the origin");
}

int main()
{
    const uintptr_t kauHighBits[] = {UINT64_C(0x1234567800000000), UINT64_C(0x00007FF900000000)};
    for (uintptr_t luBase : kauHighBits)
    {
        ImRendererSet lSet;
        std::memset(&lSet, 0xA5, sizeof(lSet));
        FillPointers(lSet, luBase);
        FillCamera(lSet.mCamera, 17.5f);
        auto* lpSlotOne = lSet.mpIm3dRenderBuffer;
        Camera lDefault = lSet.mCamera;
        lDefault.Construct();
        lSet.Construct();
        CheckConstructed(lSet, lDefault, lpSlotOne);

        ModuleInput lInput;
        std::memset(&lInput, 0x5A, sizeof(lInput));
        FillPointers(lInput.mRendererSet, luBase);
        FillCamera(lInput.mRendererSet.mCamera, 17.5f);
        lInput.mpSnapShotBuffer = &lSet;
        lInput.Construct();
        CheckConstructed(lInput.mRendererSet, lDefault, lpSlotOne);
        Check(lInput.mpSnapShotBuffer == &lSet, "Construct preserves snapshot-buffer storage");
        Check(lInput.mInputQueue.CgsModule::VariableEventQueue<32768,16>::GetLength() == 0
              && lInput.mInputQueue.mbIsConstructed,
              "Construct resets the real inbound event queue");
        Check(!lInput.IsBufferLocked(), "Construct resets stale module IO locks");

        Camera lPublishedCamera;
        FillCamera(lPublishedCamera, 123.25f);
        ImRendererSet lPublishedSet;
        FillPointers(lPublishedSet, luBase + 0x10000);
        FillCamera(lPublishedSet.mCamera, -50.5f);
        lInput.LockForWrite();
        lInput.SetCamera(lPublishedCamera);
        lInput.SetImRenderers(lPublishedSet);
        CheckPointers(lInput.mRendererSet, lPublishedSet);
        Check(SameCamera(lInput.mRendererSet.mCamera, lPublishedCamera),
              "module renderer publication preserves the entire destination camera");
        lInput.SetImRenderers(lInput.mRendererSet);
        CheckPointers(lInput.mRendererSet, lPublishedSet);
        Check(SameCamera(lInput.mRendererSet.mCamera, lPublishedCamera),
              "self-publication retains all camera fields");
        lInput.UnlockForWrite();
        lInput.LockForRead();
        const auto& lrReadSet = lInput.GetImRenderers();
        CheckPointers(lrReadSet, lPublishedSet);
        Check(SameCamera(lrReadSet.mCamera, lPublishedCamera), "module getter exposes the preserved camera");
        lInput.UnlockForRead();

        ViewInput lView;
        lView.CgsModule::IOBuffer::Construct();
        FillCamera(lView.mRendererSet.mCamera, 10000.0f);
        lView.LockForWrite();
        lView.SetImRenderers(lrReadSet);
        lView.UnlockForWrite();
        lView.LockForRead();
        CheckPointers(lView.GetImRenderers(), lPublishedSet);
        Check(SameCamera(lView.GetImRenderers().mCamera, lPublishedCamera),
              "view publication takes the source's whole camera");
        lView.UnlockForRead();

        const unsigned luAssertsBefore = suAsserts;
        lInput.SetImRenderers(lPublishedSet);
        Check(suAsserts == luAssertsBefore + 1, "module setter checks its original write lock");
        CheckPointers(lInput.mRendererSet, lPublishedSet);
        Check(SameCamera(lInput.mRendererSet.mCamera, lPublishedCamera),
              "non-gating assertion preserves original setter side effects");
    }
    Check(suAsserts == 2, "only the two deliberate lock violations assert");
    std::printf("GuiImRendererSet: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
