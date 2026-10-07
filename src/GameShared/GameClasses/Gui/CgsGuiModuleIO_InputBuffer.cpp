#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::CgsGuiModuleIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the three X360-emitted InputBuffer accessors that
// touch the immediate-mode renderer set + camera:
//
//   GetImRenderers() const  @ 0x8284E3D8  -> read-lock  (bit 4),  &mRendererSet (this+0x8020)
//   SetImRenderers()        @ 0x823C86C0  -> write-lock (bit 3),  copy head + camera (save/restore)
//   SetCamera()             @ 0x823C5318  -> write-lock (bit 3),  mRendererSet.mCamera = lrCamera
//
// SetCamera is the single function owned by the CgsGuiModuleIO.h header TU; GetImRenderers and
// SetImRenderers are the two functions owned by the class:CgsGui::CgsGuiModuleIO::InputBuffer TU.
// All three are bodied together here (one .cpp home for the InputBuffer accessor cluster), in the
// same store-for-store style as the committed OutputBuffer siblings (CgsGuiModuleIO_OutputBuffer*.cpp).
//
// Each accessor first checks the IOBuffer lock-state flag and asserts on violation exactly as the
// X360 bodies do: GetImRenderers tests bit 4 (read-lock, "Not locked for reading\n"); SetImRenderers
// and SetCamera test bit 3 (write-lock, "Not locked for writing\n"). The X360 streams the baked
// d:\p4 ...\CgsGuiModuleIO.h file path / line numbers (360, 378, 389) through CgsDev::Assert; those
// are dropped per project policy -- CGS_ASSERT carries the stringized condition + __FILE__/__LINE__.
//
// ImRendererSet and Camera use their canonical typed homes. The pointer slots widen on the
// host, while Camera's copy constructor and assignment preserve the original value semantics.

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    void InputBuffer::_AssertInputLayout()
    {
        // The pointer-free queue fixes mRendererSet at +0x8020 on both targets.
        // The interior camera follows five host-width pointers, so only its
        // alignment is shared with the console's +0x20 interior offset.
        static_assert(offsetof(InputBuffer, mRendererSet) == 0x8020,
                      "mRendererSet @0x8020 (32800) -- GetImRenderers return-offset");
        static_assert(offsetof(ImRendererSet, mCamera) % alignof(CgsGraphics::Camera) == 0,
                      "ImRendererSet.mCamera keeps its original alignment");
    }

    // X360 0x82857378: bring the buffer up. Store-for-store:
    //   1. *this = 1 (the IOBuffer status byte -> eStatusConstructed);
    //   2. Construct + Clear the inbound GUI event queue (VariableEventQueue<32768,16> @ +4);
    //   3. ImRendererSet::Construct, inlined: run the real Camera::Construct and
    //      clear slots 0/2/3/4, leaving the Im3d slot at index 1 unchanged.
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Construct();
        mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Clear();
        mRendererSet.Construct();
    }

    // X360 0x828573E8: tear the buffer down -- Destruct the inbound queue, then the
    // IOBuffer base (clears the status flags).
    void InputBuffer::Destruct()
    {
        mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Destruct();
        CgsModule::IOBuffer::Destruct();
    }

    // X360 0x8284E3D8: read-lock (bit 4) handle to the immediate-mode renderer set. The
    // pseudocode tests ((*a1 >> 4) & 1) == read-lock bit, then returns this+0x8020 (32800).
    const ImRendererSet& InputBuffer::GetImRenderers() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mRendererSet;
    }

    // X360 0x823C86C0: write-lock (bit 3); assigns the source renderer set into mRendererSet but
    // preserves the destination's existing camera. The X360 body:
    //   1. saves the current mRendererSet.mCamera into a stack temp (Camera copy-ctor),
    //   2. copies the source set's 5-dword head (20 bytes) into mRendererSet,
    //   3. assigns the source camera (lrRenderers.mCamera) into mRendererSet.mCamera,
    //   4. re-assigns the saved temp back into mRendererSet.mCamera.
    // Net: the head is taken from the source; the camera is left unchanged (the original source
    // does `Camera c = mRendererSet.mCamera; mRendererSet = lRenderers; mRendererSet.mCamera = c;`).
    void InputBuffer::SetImRenderers(const ImRendererSet& lrRenderers)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        // (1) save the existing camera (X360: Camera::Camera(temp, &mRendererSet.mCamera)).
        CgsGraphics::Camera lSavedCamera = mRendererSet.mCamera;
        // (2) copy all five renderer pointers at their native width.
        mRendererSet.mpIm2dRenderBuffer = lrRenderers.mpIm2dRenderBuffer;
        mRendererSet.mpIm3dRenderBuffer = lrRenderers.mpIm3dRenderBuffer;
        mRendererSet.mpIm3dRenderBufferUntex = lrRenderers.mpIm3dRenderBufferUntex;
        mRendererSet.mpIm3dRenderBufferRacePosition = lrRenderers.mpIm3dRenderBufferRacePosition;
        mRendererSet.mpIm3dRenderBufferMenusAndHud = lrRenderers.mpIm3dRenderBufferMenusAndHud;
        // (3) assign the source camera (X360: Camera::operator=(&mRendererSet.mCamera, src+0x20)).
        mRendererSet.mCamera = lrRenderers.mCamera;
        // (4) restore the saved camera (X360: Camera::operator=(&mRendererSet.mCamera, temp)).
        mRendererSet.mCamera = lSavedCamera;
    }

    // X360 0x823C5318: write-lock (bit 3); assigns lrCamera into mRendererSet.mCamera (this+0x8040)
    // via CgsGraphics::Camera::operator=. This is the function owned by the CgsGuiModuleIO.h TU.
    void InputBuffer::SetCamera(CgsGraphics::Camera& lrCamera)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mRendererSet.mCamera = lrCamera;
    }
}
}
