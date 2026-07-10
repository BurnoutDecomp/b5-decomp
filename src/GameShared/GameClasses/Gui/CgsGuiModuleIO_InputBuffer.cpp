#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // memcpy (Camera value-image copy)

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
// The embedded camera (CgsGraphics::Camera, foreign home -- ledger TU class:CgsGraphics::Camera)
// is modelled as opaque CgsGraphicsCameraStorage; the X360 Camera::Camera/operator= byte-image
// moves (12 lvx128/stvx128 + 4-byte stores out to +0x160) are lowered here to a faithful value-image
// copy of that storage, which reproduces the by-value semantics without depending on the un-homed
// CgsGraphics::Camera definition.

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    void InputBuffer::_AssertInputLayout()
    {
        // X360 getter return-offset pins (authoritative): mRendererSet @ this+0x8020,
        // the embedded camera @ this+0x8040 (== +0x20 into ImRendererSet).
        static_assert(offsetof(InputBuffer, mRendererSet) == 0x8020,
                      "mRendererSet @0x8020 (32800) -- GetImRenderers return-offset");
        static_assert(offsetof(InputBuffer, mRendererSet) + 0x20 == 0x8040,
                      "mRendererSet.mCamera @0x8040 (32832) -- SetCamera target offset");
        static_assert(offsetof(ImRendererSet, mCamera) == 0x20,
                      "ImRendererSet.mCamera @0x20 (head is 5 copied dwords + 12B align pad)");
    }

    // X360 0x82857378: bring the buffer up. Store-for-store:
    //   1. *this = 1 (the IOBuffer status byte -> eStatusConstructed);
    //   2. Construct + Clear the inbound GUI event queue (VariableEventQueue<32768,16> @ +4);
    //   3. default-construct the embedded camera (sub_827F94E8 @ this+0x8040 -- the
    //      CgsGraphics::Camera default ctor; the camera is opaque storage here, so its
    //      image is zeroed in place of the foreign ctor) and zero the renderer-set head
    //      dwords ([0], [2], [3], [4] of the 5-pointer head; the X360 leaves dword [1]).
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Construct();
        mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Clear();
        std::memset(&mRendererSet.mCamera, 0, sizeof(mRendererSet.mCamera));
        u32* lpuHead = reinterpret_cast<u32*>(mRendererSet.maRendererPtrs);
        lpuHead[0] = 0;
        lpuHead[2] = 0;
        lpuHead[3] = 0;
        lpuHead[4] = 0;
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
        CgsGraphicsCameraStorage lSavedCamera = mRendererSet.mCamera;
        // (2) copy the 5-dword (20-byte) renderer-pointer head from the source.
        memcpy(mRendererSet.maRendererPtrs, lrRenderers.maRendererPtrs,
               sizeof(mRendererSet.maRendererPtrs));
        // (3) assign the source camera (X360: Camera::operator=(&mRendererSet.mCamera, src+0x20)).
        mRendererSet.mCamera = lrRenderers.mCamera;
        // (4) restore the saved camera (X360: Camera::operator=(&mRendererSet.mCamera, temp)).
        mRendererSet.mCamera = lSavedCamera;
    }

    // X360 0x823C5318: write-lock (bit 3); assigns lrCamera into mRendererSet.mCamera (this+0x8040)
    // via CgsGraphics::Camera::operator=. This is the function owned by the CgsGuiModuleIO.h TU.
    void InputBuffer::SetCamera(CgsGraphicsCameraStorage& lrCamera)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mRendererSet.mCamera = lrCamera;
    }
}
}
