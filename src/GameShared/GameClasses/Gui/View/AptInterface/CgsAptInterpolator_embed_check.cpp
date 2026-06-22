#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptInterpolator.h"

// Exercises the in-scope ledger function CgsGui::Interpolator::GetCurrentValue
// (@ 0x824E5BC8). A concrete subclass provides the pure-from-this-TU virtuals
// (SetInterpolator/Update/Reset live in the .cpp TU) so the embed unit links.

namespace
{
    using namespace CgsGui;

    class TestInterpolator : public Interpolator
    {
    public:
        void SetInterpolator(f32, f32, f32, f32) override {}
        f32 Update(f32) override { return 0.0f; }
        void Reset() override {}
    };

    // GetCurrentValue reads mfCurrentValue (the f32 at +0x14 in the X360 layout).
    f32 ExerciseGetCurrentValue(const Interpolator& lInterp)
    {
        return lInterp.GetCurrentValue();
    }
}

extern "C" int CgsAptInterpolator_embed_check()
{
    TestInterpolator lInterp;
    volatile f32 lfV = ExerciseGetCurrentValue(lInterp);
    (void)lfV;
    return 0;
}
