#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h"

#include <string.h>                                          // _stricmp
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

// BrnDirector::Camera::BehaviourRenderMetrics -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::BehaviourRenderMetrics):
//   BehaviourRenderMetrics::GameTalkMessageReceiver @0x8220FB60
//
// The float streaming goes through CgsDev::StrStreamBase::operator<<(f32) (the
// X360 bl's a Director-local ICF copy @0x821F0F40) and the virtual char* sink
// (vtable+4) for the separator strings, exactly the committed CgsLog surface.

namespace BrnDirector
{
namespace Camera
{

// @ 0x8220FB60 -- see the header for the per-key command map. The message filter
// flag is re-read after the "Request to get metrics" print block (the asm reloads
// gxMessageFilterFlags at 0x8220FCA0) -- the plain global reads model that.
void BehaviourRenderMetrics::GameTalkMessageReceiver(EA::GameTalk::GameTalkMessage* lpMessage,
                                                     BehaviourRenderMetrics** lppBehaviour)
{
    BehaviourRenderMetrics* lpBehaviour = *lppBehaviour;

    for (s32 liIndex = 0; liIndex < lpMessage->GetNumKeys(); ++liIndex)
    {
        const char* lpcKey = lpMessage->GetKey(liIndex);
        if (_stricmp(lpcKey, "GetMetrics") == 0)
        {
            const f32* lpfContent = static_cast<const f32*>(lpMessage->GetContent(liIndex));
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                CgsDev::StrStreamBase& lrPrint = *CgsDev::Log::gpDebugPrint;
                lrPrint << "Request to get metrics at ";
                lrPrint << lpfContent[0];
                lrPrint << ",";
                lrPrint << lpfContent[1];
                lrPrint << ",";
                lrPrint << lpfContent[2];
                lrPrint << "\n.";
            }

            const Vector3 lPosition = { lpfContent[0], lpfContent[1], lpfContent[2], 0.0f };
            if (lpBehaviour->miRenderMetricsRequested == 0)
            {
                lpBehaviour->mbUseIdleMetricsCamera   = false;
                lpBehaviour->miRenderMetricsRequested = 1;
                lpBehaviour->mMetricsPosition         = lPosition;
            }
            else if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Unable to start getting metrics, camera behaviour controller was not ready.\n";
            }
        }
        else if (_stricmp(lpcKey, "SetIdleMetricsCamera") == 0)
        {
            const f32* lpfContent = static_cast<const f32*>(lpMessage->GetContent(liIndex));
            CGS_ASSERT(lpMessage->GetSize(liIndex) >= (int)(6 * sizeof(f32)),
                       "lpMessage->GetSize(luIndex) >= (int)(6 * sizeof(float32_t))");   // cpp:132 (non-fatal; the stores run regardless)

            lpBehaviour->mIdleMetricsCameraPosition =
                Vector3{ lpfContent[0], lpfContent[1], lpfContent[2], 0.0f };
            lpBehaviour->mbUseIdleMetricsCamera = true;   // the stb sits between the two vector stores
            lpBehaviour->mIdleMetricsCameraTarget =
                Vector3{ lpfContent[3], lpfContent[4], lpfContent[5], 0.0f };
        }
        else if (_stricmp(lpcKey, "StopRenderMetrics") == 0)
        {
            lpBehaviour->miRenderMetricsRequested = 0;
        }
    }
}

}
}
