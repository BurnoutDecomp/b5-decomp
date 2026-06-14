#pragma once

#include "types.hpp"

namespace rw::graphics::postfx
{
class Vignette
{
public:
    class Parameters
    {
    public:
        Parameters();

    private:
        struct Vector4
        {
            f32 mfX;
            f32 mfY;
            f32 mfZ;
            f32 mfW;
        };

        u32     muFlags;
        Vector4 mZeroColour;
        Vector4 mUnitColourA;
        Vector4 mCentreScaleA;
        Vector4 mZeroColourB;
        Vector4 mUnitColourB;
        Vector4 mUnitColourC;
        Vector4 mCentreScaleB;
        Vector4 mUnitColourD;
    };
};
}
