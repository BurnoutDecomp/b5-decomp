#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::BlackBarRenderer::Construct  @ 0x82446E68
//   BrnGui::BlackBarRenderer::GetID      @ 0x82446F18
//   BrnGui::BlackBarRenderer::RecvEvent  @ 0x82446EA8
//
// A custom HUD renderer that draws letterbox bars whose coverage is driven by an
// event-supplied amount, clamped to [0, 0.5]. CustomRenderComponentInterface (the
// base) is forward-declared (separate TU). Event ids are kept as named constants
// with their raw values noted.

namespace CgsGui
{
    struct CustomRenderComponentInterface
    {
        CustomRenderComponentInterface* Construct();
    };
}

namespace BrnGui
{
    class BlackBarRenderer : public CgsGui::CustomRenderComponentInterface
    {
        bool  mbVisible;   // [+4]
        float mfAmount;    // [+8]  bar coverage, 0..0.5
        int   miReset;     // [+12]

    public:
        CgsGui::CustomRenderComponentInterface* Construct();
        int               GetID() { return KI_ID; }
        BlackBarRenderer* RecvEvent(float* pValue, int iEvent);

    private:
        static const int KI_ID = 560471040;

        static const int KI_EVENT_SET_AMOUNT = 221;
        static const int KI_EVENT_RESET      = 145;
    };

    CgsGui::CustomRenderComponentInterface* BlackBarRenderer::Construct()
    {
        CgsGui::CustomRenderComponentInterface* pResult =
            CgsGui::CustomRenderComponentInterface::Construct();
        mfAmount = 0.0f;
        miReset  = 0;
        return pResult;
    }

    BlackBarRenderer* BlackBarRenderer::RecvEvent(float* pValue, int iEvent)
    {
        if (iEvent == KI_EVENT_SET_AMOUNT)
        {
            float lfValue = *pValue;
            if (lfValue > 0.0f)
                mfAmount = (lfValue < 0.5f) ? lfValue : 0.5f;
            else
                mfAmount = 0.0f;

            mbVisible = mfAmount > 0.0f;
        }
        else if (iEvent == KI_EVENT_RESET)
        {
            miReset = 1;
        }

        return this;
    }
}
