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

// FOLDED FROM CgsAptInterpolator_wS34_00.cpp
// CgsAptInterpolator_wS34_00.cpp -- the linear interpolator's three virtuals (CgsAptInterpolator.cpp
// family), reconstructed from the console image.

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptInterpolator.h"

#include <cfloat>   // FLT_MAX

namespace CgsGui
{
    // latch the lambda range and the value range, then reset.
    void Interpolator::SetInterpolator(f32 lfStartLambda, f32 lfEndLambda,
                                       f32 lfStartValue, f32 lfEndValue)
    {
        mfStartLambda = lfStartLambda;
        mfEndLambda   = lfEndLambda;
        mfStartValue  = lfStartValue;
        mfEndValue    = lfEndValue;
        Reset();
    }

    // the value at lfLambda: the start value up to the start lambda, the end
    // value from the end lambda on, linear in between.
    f32 Interpolator::Update(f32 lfLambda)
    {
        if (!(lfLambda < mfEndLambda))
        {
            mfCurrentValue = mfEndValue;
            return mfCurrentValue;
        }

        f32 lfValue = mfStartValue;
        if (lfLambda > mfStartLambda)
        {
            lfValue = mfStartValue + (lfLambda - mfStartLambda) / (mfEndLambda - mfStartLambda) *
                                         (mfEndValue - mfStartValue);
        }
        mfCurrentValue = lfValue;
        return mfCurrentValue;
    }

    // no value yet.
    void Interpolator::Reset()
    {
        mfCurrentValue = -FLT_MAX;
    }
}
