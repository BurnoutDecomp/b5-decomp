#include "GameSource/Network/Utilities/BrnNetworkRounder.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "rw/math/fpu/scalar_operation.h"

namespace BrnNetwork
{
    static const f32 KF_NETWORK_TIME_ACCURACY = 0.0033333334f;
    static const f32 KF_NETWORK_DISTANCE_ACCURACY = 0.01f;

    // ARTIST 0x8258B090 scales by twice the caller's accuracy and snaps in place.
    void NetworkRounder::RoundFloatToAccuracy(f32* lpfFloat, f32 lfAccuracy)
    {
        const f32 lfStep = lfAccuracy * 2.0f;
        *lpfFloat /= lfStep;
        *lpfFloat = rw::math::fpu::Round(*lpfFloat);
        *lpfFloat *= lfStep;
    }

    // ARTIST 0x82591EB0 rounds in centimetres before converting back to metres.
    void NetworkRounder::RoundDistanceToNetworkAccuracy(f32* lpfDistance)
    {
        *lpfDistance *= 100.0f;
        *lpfDistance = rw::math::fpu::Round(*lpfDistance);
        *lpfDistance *= KF_NETWORK_DISTANCE_ACCURACY;
    }

    // ARTIST 0x82591F20 backs off half a quantum to keep the rounded fraction below one.
    void NetworkRounder::RoundTimeToNetworkAccuracy(CgsSystem::Time* lpTime)
    {
        f32 lfFraction = rw::math::fpu::Round(lpTime->GetFraction() * 300.0f)
            * KF_NETWORK_TIME_ACCURACY;
        if (!(lfFraction < 1.0f))
            lfFraction -= 0.0016666667f;
        lpTime->SetFraction(lfFraction);
    }
}
