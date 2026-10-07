#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

static unsigned guChecks, guFailures;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guFailures; return 0; }
void* EndAssert() { return 0; }
} }
namespace CgsGraphics {
#include "playtest_gui_camera_copy.inc"
}
namespace CgsGui { namespace CgsGuiModuleIO {
#include "playtest_gui_camera_input.inc"
} namespace ViewIO {
#include "playtest_gui_camera_view_input.inc"
} }
struct ViewCopy {
    CgsGui::ImRendererSet mImRenderers;
    void Apply(const CgsGui::ViewIO::ImRendererSet& lrRenderers) {
#include "playtest_gui_camera_view_copy.inc"
    }
};
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
int main() {
    CgsGraphics::Camera lCamera;
    std::memset(&lCamera, 0, sizeof(lCamera));
    lCamera.mView.wAxis.x = 17.5f;
    lCamera.mProjection.xAxis.x = 2.0f;
    lCamera.mViewProjection.zAxis.w = 3.0f;
    lCamera.maClipState[15] = 9876.25;
    lCamera.maProjectionScalars[8] = 1200.0f;
    CgsGui::CgsGuiModuleIO::InputBuffer lInput;
    lInput.mxStatusFlags.Clear();
    lInput.mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    lInput.SetCamera(lCamera);
    Check(lInput.mRendererSet.mCamera.mView.wAxis.x == 17.5f, "module input receives complete view transform");
    CgsGui::CgsGuiModuleIO::ImRendererSet lOther = {};
    lOther.mCamera.mView.wAxis.x = -100.0f;
    lOther.mpIm2dRenderBuffer = reinterpret_cast<CgsGraphics::Im2dRenderBuffer*>(UINT64_C(0x123456780000007B));
    lInput.SetImRenderers(lOther);
    Check(lInput.mRendererSet.mCamera.mView.wAxis.x == 17.5f
          && lInput.mRendererSet.mpIm2dRenderBuffer == lOther.mpIm2dRenderBuffer,
          "module renderer-pointer publication preserves director camera");
    CgsGui::ViewIO::InputBuffer lViewInput;
    lViewInput.mxStatusFlags.Clear();
    lViewInput.mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    CgsGui::ViewIO::ImRendererSet lViewSet = {};
    lViewSet.mCamera = lInput.mRendererSet.mCamera;
    lViewInput.SetImRenderers(lViewSet);
    Check(lViewInput.mRendererSet.mCamera.maClipState[15] == 9876.25 &&
          lViewInput.mRendererSet.mCamera.maProjectionScalars[8] == 1200.0f,
          "view input keeps full-width clip and projection data");
    ViewCopy lView;
    lView.Apply(lViewInput.mRendererSet);
    Check(lView.mImRenderers.mCamera.mProjection.xAxis.x == 2.0f &&
          lView.mImRenderers.mCamera.mViewProjection.zAxis.w == 3.0f,
          "custom renderer set receives current projection matrices");
    Check(sizeof(CgsGui::CgsGuiModuleIO::CgsGraphicsCameraStorage) == sizeof(CgsGraphics::Camera) &&
          sizeof(CgsGui::ViewIO::CgsGraphicsCameraStorage) == sizeof(CgsGraphics::Camera),
          "both IO storage aliases use the real camera home");
    std::printf("PlaytestGuiCameraDelivery: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
