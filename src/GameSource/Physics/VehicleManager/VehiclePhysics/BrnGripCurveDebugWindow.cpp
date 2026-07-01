#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugWindow.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"   // GetUI().AddWindow/RemoveWindow/IsWindowAdded

#include <algorithm>   // std::max
#include <cmath>       // std::ceil

// BrnPhysics::Vehicle::GripCurveDebugWindow -- the four ledger functions homed by the Vehicle-physics
// group. Reconstructed from BURNOUT_X360_ARTIST.XEX.

namespace BrnPhysics
{
namespace Vehicle
{
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
        const Wheel::TireGripCurve* lpLong = mLongGripCurveGraph.GetGripCurve(); // *(this+0x30) -> mpGripCurve
        const Wheel::TireGripCurve* lpLat  = mLatGripCurveGraph.GetGripCurve();  // *(this+0x70)

        const f32 lfLongMax = std::max(lpLong->maGripVariables.x, lpLong->maGripVariables.y); // lanes 0,1
        const f32 lfLatMax  = std::max(lpLat->maGripVariables.x,  lpLat->maGripVariables.y);
        const f32 lfMax     = std::max(lfLongMax, lfLatMax);

        return static_cast<f32>(std::ceil(lfMax * 1.2f));
    }

    // GetCoefficientRange @0x825B4C90: the Y-axis (grip-coefficient) range for the graph. Same math as
    // GetSlipRatioRange but on lanes 2 (peakCoefficient) and 3 (fallCoefficient) of each curve's
    // packed Vector4 maGripVariables.
    f32 GripCurveDebugWindow::GetCoefficientRange()
    {
        const Wheel::TireGripCurve* lpLong = mLongGripCurveGraph.GetGripCurve(); // *(this+0x30)
        const Wheel::TireGripCurve* lpLat  = mLatGripCurveGraph.GetGripCurve();  // *(this+0x70)

        const f32 lfLongMax = std::max(lpLong->maGripVariables.z, lpLong->maGripVariables.w); // lanes 2,3
        const f32 lfLatMax  = std::max(lpLat->maGripVariables.z,  lpLat->maGripVariables.w);
        const f32 lfMax     = std::max(lfLongMax, lfLatMax);

        return static_cast<f32>(std::ceil(lfMax * 1.2f));
    }
}
}
