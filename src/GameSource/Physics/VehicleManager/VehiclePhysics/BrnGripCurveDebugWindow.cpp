#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugWindow.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"   // GetUI().AddWindow/RemoveWindow/IsWindowAdded
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT

#include <cmath>       // VMX flush, max semantics and scalar ceil

// BrnPhysics::Vehicle::GripCurveDebugWindow -- the four ledger functions homed by the Vehicle-physics
// group. Reconstructed from BURNOUT_X360_ARTIST.XEX.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        f32 GripFlush(f32 lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        }

        // ARTIST vmaxfp: flush operands/results, propagate unordered values,
        // and prefer positive zero when the zero signs differ.
        f32 GripMaximum(f32 lfA, f32 lfB)
        {
            lfA = GripFlush(lfA);
            lfB = GripFlush(lfB);
            if (std::isnan(lfA))
                return lfA;
            if (std::isnan(lfB))
                return lfB;
            if (lfA == 0.0f && lfB == 0.0f)
                return std::signbit(lfA) && std::signbit(lfB) ? -0.0f : 0.0f;
            return GripFlush(lfA > lfB ? lfA : lfB);
        }

        f32 GripMultiply(f32 lfA, f32 lfB)
        {
            return GripFlush(GripFlush(lfA) * GripFlush(lfB));
        }

        // Prepare uses scalar fsubs/fsel/fmuls and ceil after each curve's
        // vector maximum. Those scalar operations are not VMX-flushed.
        f32 InitialGripRange(f32 lfLongMax, f32 lfLatMax)
        {
            const f32 lfDifference = lfLongMax - lfLatMax;
            const f32 lfMaximum = lfDifference >= 0.0f ? lfLongMax : lfLatMax;
            return static_cast<f32>(std::ceil(lfMaximum * 1.2f));
        }
    }

    // ARTIST 825B9418 recomputes each graph from its own bound curve. Unlike
    // the initial Prepare ranges, this update does not ceil the scaled maxima.
    void GripCurveDebugWindow::Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent)
    {
        GripCurveDebugGraph* lapGraphs[] = { &mLongGripCurveGraph, &mLatGripCurveGraph };
        for (GripCurveDebugGraph* lpGraph : lapGraphs)
        {
            const Vector4& lrCurve = lpGraph->GetGripCurve()->maGripVariables;
            rw::math::vpu::Vector2 lRange;
            lRange.x = GripMultiply(GripMaximum(lrCurve.x, lrCurve.y), 1.2f);
            lRange.y = GripMultiply(GripMaximum(lrCurve.z, lrCurve.w), 1.2f);
            // ARTIST vperm mask 82CDA350 is {A.x, B.y, A.x, A.x}.
            lRange.z = lRange.w = lRange.x;
            lpGraph->SetRange(lRange);
        }
        CgsDev::DebugUI::Window::Update(lfTimeStep, leEvent);
    }

    // ARTIST 8262FEC8: both graph bodies and all four axle-wheel samples.
    void GripCurveDebugWindow::Render(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(mbVisible, "mbVisible");
        CgsDev::DebugUI::Window::Render(lpDisplay);
        rw::math::vpu::Vector2 lLongLocation;
        lLongLocation.SetZero();
        lLongLocation.x = GetX() + 20.0f;
        lLongLocation.y = GetY() + 40.0f;
        mLongGripCurveGraph.SetLocation(lLongLocation);
        rw::math::vpu::Vector2 lLatLocation = lLongLocation;
        lLatLocation.y = (lLongLocation.y + 225.0f) + 60.0f;
        lLatLocation.z = lLatLocation.w = lLongLocation.x;
        mLatGripCurveGraph.SetLocation(lLatLocation);
        mLongGripCurveGraph.Render(lpDisplay);
        mLatGripCurveGraph.Render(lpDisplay);
        mLongGripCurveGraph.PlotCurrentValue(lpDisplay, mpLeftWheel->mSlipVariables.y, 0xFF0000FFu);
        mLongGripCurveGraph.PlotCurrentValue(lpDisplay, mpRightWheel->mSlipVariables.y, 0xFFFF0000u);
        mLatGripCurveGraph.PlotCurrentValue(lpDisplay, mpLeftWheel->mSlipVariables.x, 0xFF0000FFu);
        mLatGripCurveGraph.PlotCurrentValue(lpDisplay, mpRightWheel->mSlipVariables.x, 0xFFFF0000u);
        const auto lLongTitle = mLongGripCurveGraph.GetTitleLocation();
        lpDisplay->DrawText("Long grip curve", lLongTitle.x, lLongTitle.y, 18.0f, 0xFF000000u);
        const auto lLatTitle = mLatGripCurveGraph.GetTitleLocation();
        lpDisplay->DrawText("Lateral grip curve", lLatTitle.x, lLatTitle.y, 18.0f, 0xFF000000u);
    }

    // Construct: seat both graphs, clear the wheel pair and start hidden. The console inlines the
    // whole thing (both graph Constructs included) into the handling debug component's Construct,
    // once per window; it is folded back out here so the stores land on named members. The Window
    // base is untouched, exactly as on the console -- Prepare is what fills that in.
    void GripCurveDebugWindow::Construct()
    {
        mLongGripCurveGraph.Construct();
        mLatGripCurveGraph.Construct();

        mpLeftWheel  = 0;
        mpRightWheel = 0;

        mbVisible = false;
    }

    // Prepare @0x825DAF20. The assembly stores the two wheel pointers, then the long and lateral
    // TireGripCurve pointers from the left wheel's TireAttribs at graph offsets +0x00. It computes
    // coefficient range before slip range, writes {slip, coefficient, 0, 0} to both graph range
    // vectors, and finally prepares the Window base at 340 x 550 with no flags.
    void GripCurveDebugWindow::Prepare(Wheel* lpLeftWheel, Wheel* lpRightWheel,
                                       const char* lpacWindowCaption)
    {
        CGS_ASSERT(lpLeftWheel != 0, "lpLeftWheel != NULL");
        CGS_ASSERT(lpRightWheel != 0, "lpRightWheel != NULL");

        mpLeftWheel = lpLeftWheel;
        mpRightWheel = lpRightWheel;

        CGS_ASSERT(lpLeftWheel->mpTireAttribs != 0, "mpLeftWheel->mpTireAttribs != NULL");
        CGS_ASSERT(mpLeftWheel->mpTireAttribs != 0, "mpLeftWheel->mpTireAttribs != NULL");

        const Wheel::TireAttribs* lpTireAttribs = mpLeftWheel->mpTireAttribs;
        CGS_ASSERT(lpTireAttribs != 0, "lpGripCurve != NULL");
        const Wheel::TireGripCurve* lpLongGripCurve = &lpTireAttribs->mLongGripCurve;
        mLongGripCurveGraph.Prepare(lpLongGripCurve);

        const Wheel::TireGripCurve* lpLatGripCurve = &lpTireAttribs->mLatGripCurve;
        CGS_ASSERT(lpLatGripCurve != 0, "lpGripCurve != NULL");
        mLatGripCurveGraph.Prepare(lpLatGripCurve);

        const f32 lfCoefficientRange = GetCoefficientRange();
        const f32 lfSlipRatioRange = GetSlipRatioRange();
        rw::math::vpu::Vector2 lvGraphRange;
        lvGraphRange.SetZero();
        lvGraphRange.x = lfSlipRatioRange;
        lvGraphRange.y = lfCoefficientRange;
        mLongGripCurveGraph.SetRange(lvGraphRange);
        mLatGripCurveGraph.SetRange(lvGraphRange);

        CgsDev::DebugUI::Window::Prepare(340.0f, 550.0f, lpacWindowCaption, 0);
    }

    // Show @0x825B4B00: if currently hidden, add the window to the debug-UI stack (only if it is not
    // already added) and set the visible flag.
    void GripCurveDebugWindow::Show()
    {
        if (!mbVisible)                             // lbz r11,0xB8(r31); bne -> skip
        {
            if (!GetUI().IsWindowAdded(this))       // clrlwi test; bne skip AddWindow
            {
                GetUI().AddWindow(this);
            }
            mbVisible = true;                       // li 1; stb r11,0xB8(r31)
        }
    }

    // Hide @0x825B4B60: if currently shown, unlink the window from the debug-UI stack (only if it is
    // actually added) and clear the visible flag.
    void GripCurveDebugWindow::Hide()
    {
        if (mbVisible)                              // lbz r11,0xB8(r31); beq -> skip
        {
            if (GetUI().IsWindowAdded(this))        // clrlwi test
            {
                GetUI().RemoveWindow(this);
            }
            mbVisible = false;                      // li 0; stb r11,0xB8(r31)
        }
    }

    // GetSlipRatioRange @0x825B4BC0: the X-axis (slip-ratio) range for the graph. The console splats
    // lanes 0 (peakSlipRatio) and 1 (floorSlipRatio) out of each curve's packed Vector4
    // maGripVariables, takes the per-curve max (vmaxfp) then the max across the two curves
    // (fsel(a-b,a,b) == max), scales by 1.2 and ceils.
    f32 GripCurveDebugWindow::GetSlipRatioRange()
    {
        const Wheel::TireGripCurve* lpLong = mLongGripCurveGraph.GetGripCurve();
        const Wheel::TireGripCurve* lpLat = mLatGripCurveGraph.GetGripCurve();
        const f32 lfLongMax = GripMaximum(lpLong->maGripVariables.x, lpLong->maGripVariables.y);
        const f32 lfLatMax = GripMaximum(lpLat->maGripVariables.x, lpLat->maGripVariables.y);
        return InitialGripRange(lfLongMax, lfLatMax);
    }

    // GetCoefficientRange @0x825B4C90: the Y-axis (grip-coefficient) range for the graph. Same math as
    // GetSlipRatioRange but on lanes 2 (peakCoefficient) and 3 (fallCoefficient) of each curve's
    // packed Vector4 maGripVariables.
    f32 GripCurveDebugWindow::GetCoefficientRange()
    {
        const Wheel::TireGripCurve* lpLong = mLongGripCurveGraph.GetGripCurve();
        const Wheel::TireGripCurve* lpLat = mLatGripCurveGraph.GetGripCurve();
        const f32 lfLongMax = GripMaximum(lpLong->maGripVariables.z, lpLong->maGripVariables.w);
        const f32 lfLatMax = GripMaximum(lpLat->maGripVariables.z, lpLat->maGripVariables.w);
        return InitialGripRange(lfLongMax, lfLatMax);
    }
}
}
