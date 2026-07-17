// FLAG PC-platform leaf: the D3D9 flush half of the Apt render chain (see the
// header for the ownership boundary). Re-home step A of retirement slice 5:
// behaviour unchanged -- the same Swap -> Clear -> Dispatch the boot has always
// run, now homed as a clean PC backend TU.

#include "GameShared/GameClasses/Gui/PC/CgsAptRenderBackendPC.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h" // CgsGui::AptIm2dRenderBuffer
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V>

namespace CgsGui
{
    void DispatchAptIm2dRenderBufferPC(AptIm2dRenderBuffer* lpBuffer)
    {
        if (lpBuffer == nullptr)
            return;

        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrBuffer =
            lpBuffer->mCommandBuffer;
        lrBuffer.Swap();
        lrBuffer.Clear();
        lrBuffer.Dispatch();

        static bool s_bFlushProbed = false;
        if (!s_bFlushProbed)
        {
            s_bFlushProbed = true;
            CgsDev::Log::WriteToLog(
                "[AptRT] render: view chain -> D3D9 via CgsAptRenderBackendPC dispatch (OK; per-frame).\n");
        }
    }
}
