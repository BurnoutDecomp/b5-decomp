#ifndef CGS_APT_RENDER_BACKEND_PC_H
#define CGS_APT_RENDER_BACKEND_PC_H

// FLAG PC-platform leaf: the D3D9 flush half of the Apt render chain. The ENGINE
// side is fully real (GuiModule::Render -> ViewModule::Render @0x82858810 ->
// RenderInternal @0x82858AF8 -> AptAux::Render @0x82848FB8 -> the AptRenderTarget
// walk fills the Im2d command buffer through the gAptFuncs render callbacks); the
// console render THREAD then drains the filled buffers through the custom-renderer-
// manager bracket RenderInternal notifies. This TU is the PC's single-threaded
// equivalent of that consumption: freeze + flush the filled buffer to the D3D9
// device (Swap -> Clear -> Dispatch) after the view render returns.
// (Re-home step A of retirement slice 5: the buffer instance itself still lives
// with the bring-up host; it moves here in step B.)

namespace CgsGui { struct AptIm2dRenderBuffer; }

namespace CgsGui
{
    // Freeze + flush one frame's filled command buffer to D3D9.
    //   Swap  freezes the write buffer for dispatch;
    //   Clear resets the NEW write buffer's stream positions (Swap alone does NOT --
    //         without it the vertex stream fills permanently after ~60 text frames
    //         and the dynamic-text glyphs silently stop landing);
    //   Dispatch re-issues every command to the D3D9 device.
    void DispatchAptIm2dRenderBufferPC(AptIm2dRenderBuffer* lpBuffer);
}

#endif // CGS_APT_RENDER_BACKEND_PC_H
