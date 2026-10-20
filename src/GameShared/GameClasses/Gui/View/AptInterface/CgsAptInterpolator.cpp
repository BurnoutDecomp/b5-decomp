#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptInterpolator.h"

namespace CgsGui
{
    // DecFIGS CgsAptInterpolator.cpp:39. ARTIST Animator::Construct inlines
    // these five zero stores into its actual AnimChannel template.
    void Interpolator::Construct()
    {
        mfStartLambda = 0.0f;
        mfEndLambda = 0.0f;
        mfStartValue = 0.0f;
        mfEndValue = 0.0f;
        mfCurrentValue = 0.0f;
    }

    // ARTIST 824E5BC8; the original virtual getter reads the actual named field.
    // The compiler supplies its native host offset and vtable slot.
    f32 Interpolator::GetCurrentValue() const
    {
        return mfCurrentValue;
    }
}
