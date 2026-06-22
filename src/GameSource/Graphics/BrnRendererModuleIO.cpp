#include "GameSource/Graphics/BrnRendererModuleIO.h"

#include <cstddef>   // offsetof

// RendererIO::InputBuffer / OutputBuffer Construct, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Both follow the CgsModule::IOBuffer construction pattern: mark the buffer constructed (the X360
// stores 1 into the leading status byte, which is exactly CgsModule::IOBuffer::Construct -- it
// Clear()s the status flags then SetBit(eStatusConstructed)), then initialise the owned members.

namespace RendererIO
{
    // @ 0x82400190 -- mark constructed, then construct the embedded director camera (this+0x10).
    void InputBuffer::Construct()
    {
        // X360-proven offset pin: the embedded camera lands at this+0x10 (InputBuffer::Construct does
        // `addi r3, this, 0x10` before BrnDirector::Camera::Camera::Construct). The alignas(16) camera
        // following the 1-byte CgsModule::IOBuffer base forces this on both targets -- this is a
        // same-object self-offset (target-stable), not a cross-pointer-member offset.
        static_assert(offsetof(InputBuffer, mBrnCamera) == 0x10,
                      "RendererIO::InputBuffer camera must be at this+0x10");

        CgsModule::IOBuffer::Construct();
        mBrnCamera.Construct();
    }

    // @ 0x824001A8 -- mark constructed, then null every owned render-buffer / effects-frame pointer
    // and both effects-frame pointer arrays. The camera / render switches / perfmon aggregates are
    // left default (the X360 Construct does not touch their storage).
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mpDispatchFrame                = 0;
        mpIm2dRenderBuffer             = 0;
        mpIm3dRenderBuffer             = 0;
        mpIm3dRenderBufferUntex        = 0;
        mpIm3dDebugRenderBuffer        = 0;   // X360 stores this + the Im2d debug ptr last
        mpIm2dDebugRenderBuffer        = 0;
        mpIm3dRenderBufferRacePosition = 0;
        mpIm3dRenderBufferMenusAndHud  = 0;
        mpBaseEffectsFrame             = 0;

        for (u32 lu = 0; lu < 4; ++lu)
            mapWorldEffectsFrames[lu] = 0;
        for (u32 lu = 0; lu < 2; ++lu)
            mapFXEventsEffectsFrames[lu] = 0;

        mpShaderConstantsFrame      = 0;
        mpBlobbyShadowBuffer        = 0;
        mpCoronaSubmissionInteface  = 0;
        mpSnapshotBuffer            = 0;
    }
}
