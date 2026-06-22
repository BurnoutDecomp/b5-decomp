#ifndef CGS_APT_INTERPOLATOR_H
#define CGS_APT_INTERPOLATOR_H

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::Interpolator::GetCurrentValue  @ 0x824E5BC8
//
// CgsAptInterpolator.h homes CgsGui::Interpolator, a simple scalar-value
// interpolator used by the Apt (Scaleform-style) GUI interface. It tracks a
// lambda (parametric position) range and a value range, lerping the current
// value as the animation advances.
//
// MINIMAL OWNING SLICE: only the member layout proven by the in-scope getter
// (and corroborated by the DWARF member list at this header) is modelled. The
// virtual Construct/SetInterpolator/Update/Reset methods live in
// CgsAptInterpolator.cpp (separate TUs) and are declared here for the vtable
// shape; their bodies are NOT in this leaf TU.

namespace CgsGui
{
    // DWARF (CgsAptInterpolator.h:42) member order:
    //   _vptr            [+0x00]
    //   mfStartLambda    [+0x04]   (h:88)
    //   mfEndLambda      [+0x08]   (h:89)
    //   mfStartValue     [+0x0C]   (h:91)
    //   mfEndValue       [+0x10]   (h:92)
    //   mfCurrentValue   [+0x14]   (h:94)
    //
    // GetCurrentValue @ 0x824E5BC8 is `lfs f1, 0x14(r3); blr` -> it returns the
    // f32 at +0x14, i.e. mfCurrentValue (Hex-Rays widens the return to double per
    // the PPC f1 ABI; logically an f32 read of mfCurrentValue).
    class Interpolator
    {
    public:
        Interpolator() {}

        // Set up a new interpolation between (startLambda -> endLambda) producing
        // (startValue -> endValue). Defined in CgsAptInterpolator.cpp.
        virtual void SetInterpolator(f32 lfStartLambda, f32 lfEndLambda,
                                     f32 lfStartValue, f32 lfEndValue);

        f32 GetStartValue() const { return mfStartValue; }   // h:109
        f32 GetEndValue() const { return mfEndValue; }       // h:122

        // h:135 -- the in-scope ledger function @ 0x824E5BC8.
        virtual f32 GetCurrentValue() const { return mfCurrentValue; }

        f32 GetStartLambda() const { return mfStartLambda; } // h:148
        f32 GetEndLambda() const { return mfEndLambda; }     // h:161

        // Advance the interpolation; returns the new current value. CgsAptInterpolator.cpp.
        virtual f32 Update(f32 lfLambda);

        // Reset to the start of the interpolation. CgsAptInterpolator.cpp.
        virtual void Reset();

    protected:
        void Construct();   // CgsAptInterpolator.cpp

        f32 mfStartLambda;   // [+0x04] h:88
        f32 mfEndLambda;     // [+0x08] h:89
        f32 mfStartValue;    // [+0x0C] h:91
        f32 mfEndValue;      // [+0x10] h:92
        f32 mfCurrentValue;  // [+0x14] h:94
    };
}

#endif // CGS_APT_INTERPOLATOR_H
