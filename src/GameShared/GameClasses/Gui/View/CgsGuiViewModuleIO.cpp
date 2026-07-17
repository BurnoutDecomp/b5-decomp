#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // memcpy (InputBuffer::SetImRenderers)

// CgsGui::ViewIO::OutputBuffer accessor, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU bodies the one X360-emitted OutputBuffer function in this group:
//
//   GetGuiEventQueue() const @ 0x8284F040 -> &mGuiEvents (+4), read-lock (status bit 4)
//
// The X360 body loads the 1-byte IOBuffer status (lbz r11,0(this)), extracts bit 4 of it
// (extrwi r11,r11,1,27 == 1 bit at MSB-position 27 == the read-lock bit, eStatusLockedForRead
// 0x10), asserts on a clear bit ("Not locked for reading\n"), then returns this + 4. The
// original streamed the d:\p4-baked file/line via CgsDev::Assert; CGS_ASSERT carries the
// stringized condition + __FILE__/__LINE__ (the baked path/line are dropped per policy).

namespace CgsGui
{
namespace ViewIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mGuiEvents) == 0x0004, "mGuiEvents @0x0004");
    }

    // X360 0x82858E40: mark the buffer constructed (the IOBuffer status byte), then
    // construct the embedded GUI event queue at this+4.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mGuiEvents.CgsModule::VariableEventQueue<18432, 16>::Construct();
    }

    // X360 0x8284F040: read-lock handle to the embedded GUI event queue (this+4).
    const OutputBuffer::GuiEventQueue* OutputBuffer::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGuiEvents;
    }

    // X360 0x8284F0E8: the write twin (GuiModule::Update @0x828602C8 fetches it before
    // appending the AptCommunicator trigger queue into this buffer).
    OutputBuffer::GuiEventQueue* OutputBuffer::GetGuiEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGuiEvents;
    }

    // ---- CgsGui::ViewIO::InputBuffer (X360 0x824F7A10 / 0x824F7AB8 / 0x82856EC8) ------------

    void InputBuffer::_AssertInputLayout()
    {
        static_assert(offsetof(InputBuffer, mViewStateQueue) == 0x0004,
                      "mViewStateQueue @0x0004 -- GetViewStateQueue return-offset");
        static_assert(offsetof(InputBuffer, mRendererSet) == 0x10020,
                      "mRendererSet @0x10020 (65568) -- GetImRenderers return-offset");
        // Console interior pins (5 dword slots -> camera @ set+0x20 == buffer+0x10040): the
        // x64 pointer widening moves the camera to set+0x30 (5 x 8B slots + 16-align pad);
        // parity is by named member (see the header note), so only the 16-alignment is pinned.
        static_assert(offsetof(ImRendererSet, mCamera) % 16 == 0,
                      "ImRendererSet.mCamera 16-aligned (console @ set+0x20)");
    }

    // X360 0x824F7A10: read-lock (bit 4) handle to the immediate-mode renderer set. Returns
    // this + 0x10020 == &mRendererSet.
    const ImRendererSet& InputBuffer::GetImRenderers() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mRendererSet;
    }

    // X360 0x824F7AB8: write-lock (bit 3) handle to the embedded view-state event queue. A Get*
    // that checks the WRITE lock (the AddViewState writers acquire it before Append'ing);
    // reproduced verbatim. Returns this + 4 == &mViewStateQueue.
    InputBuffer::ViewStateQueue& InputBuffer::GetViewStateQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return mViewStateQueue;
    }

    // X360 0x8284EF98 (baked CgsGuiViewModuleIO.h:172): read-lock (bit 4) handle to the
    // view-state queue -- the ViewModule::Update-side reader.
    const InputBuffer::ViewStateQueue& InputBuffer::GetEvents() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mViewStateQueue;
    }

    // X360 0x82856EC8: write-lock (bit 3); assigns the source renderer set into mRendererSet.
    // Copies the source set's 5 renderer-pointer slots (console: a 20-byte 5-dword head copy)
    // into mRendererSet, then assigns the source camera (src+0x20) into the embedded mCamera.
    // Unlike the CgsGuiModuleIO::InputBuffer sibling there is NO old-camera save/restore: both
    // head and camera are taken from the source.
    void InputBuffer::SetImRenderers(const ImRendererSet& lrRenderers)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        mRendererSet.mpIm2dRenderBuffer             = lrRenderers.mpIm2dRenderBuffer;
        mRendererSet.mpReserved04                   = lrRenderers.mpReserved04;
        mRendererSet.mpIm3dRenderBufferUntex        = lrRenderers.mpIm3dRenderBufferUntex;
        mRendererSet.mpIm3dRenderBufferRacePosition = lrRenderers.mpIm3dRenderBufferRacePosition;
        mRendererSet.mpIm3dRenderBufferMenusAndHud  = lrRenderers.mpIm3dRenderBufferMenusAndHud;
        mRendererSet.mCamera = lrRenderers.mCamera;
    }
}
}
