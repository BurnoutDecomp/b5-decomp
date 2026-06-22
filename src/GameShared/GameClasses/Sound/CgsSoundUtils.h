#ifndef CGS_SOUND_CGSSOUNDUTILS_H
#define CGS_SOUND_CGSSOUNDUTILS_H

#include "types.hpp"

#include <cmath>

// CgsSound::Utils - sound-subsystem math/utility helpers. The ONLY function homed
// here is the Slope copy-from-params constructor at 0x826A1F70; the surrounding
// SlopeParams and Slope surface is declared (per the DecFIGS DWARF + DWARF
// CgsSoundUtils.h) so the constructor can be bodied with member-by-name access.
// The remaining Slope methods (Initialize/GetValue/dtor) and the other utility
// classes (Curve, PathLine, InterpolateLine, Graph, ...) live in their own TUs and
// are intentionally not bodied here.
namespace CgsSound
{
namespace Utils
{

// Slope input/output range parameters (DWARF CgsSoundUtils.h:258). Four f32:
// the input range [mfMinInput, mfMaxInput] mapped to [mfMinOutput, mfMaxOutput].
struct SlopeParams
{
    SlopeParams()
    {
        Reset();
    }

    SlopeParams(f32 lfMinInput, f32 lfMaxInput, f32 lfMinOutput, f32 lfMaxOutput)
        : mfMinInput(lfMinInput)
        , mfMaxInput(lfMaxInput)
        , mfMinOutput(lfMinOutput)
        , mfMaxOutput(lfMaxOutput)
    {
    }

    void Reset()
    {
        mfMinInput  = 0.0f;
        mfMaxInput  = 0.0f;
        mfMinOutput = 0.0f;
        mfMaxOutput = 0.0f;
    }

    f32 mfMinInput;    // CgsSoundUtils.h:287
    f32 mfMaxInput;    // CgsSoundUtils.h:288
    f32 mfMinOutput;   // CgsSoundUtils.h:289
    f32 mfMaxOutput;   // CgsSoundUtils.h:290
};

// Linear input->output mapper backed by a SlopeParams (DWARF CgsSoundUtils.h:305).
class Slope
{
public:
    Slope();
    Slope(f32 lfMinInput, f32 lfMaxInput, f32 lfMinOutput, f32 lfMaxOutput);

    // Copy-from-params constructor. Recovered from the X360 ctor at 0x826A1F70:
    //
    //   result[0..3] = params[0..3];               // copy the 4 range floats
    //   if (fabs(mfMaxInput - mfMinInput) < 1e-6)   // degenerate input span?
    //       mfMaxInput += 1e-6;                     // nudge so GetValue never /0
    //
    // The DWARF declares Slope(const SlopeParams&); the asm zeroes mParams then
    // copies each of the four floats from the source and guards the input span.
    Slope(const SlopeParams& params);

    ~Slope();

    f32 GetValue(f32 lfInput) const;

    void Initialize(f32 lfMinInput, f32 lfMaxInput, f32 lfMinOutput, f32 lfMaxOutput);
    void Initialize(const SlopeParams& params);

    const SlopeParams& GetParams() const { return mParams; }

private:
    SlopeParams mParams;   // CgsSoundUtils.h:346
};

}
}

#endif // CGS_SOUND_CGSSOUNDUTILS_H
