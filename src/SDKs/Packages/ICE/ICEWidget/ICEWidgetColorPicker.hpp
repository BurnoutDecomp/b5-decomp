#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETCOLORPICKER_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETCOLORPICKER_HPP

#include "types.hpp"

namespace ICE
{
// DecFIGS ICEWidgetColorPicker.hpp:83..91 names these nine floats. ARTIST
// ConstructMenus 82532124..8253216C attests the same order and initialization.
struct ICEWidgetColorPicker
{
    void Construct(f32 lfX, f32 lfY, f32 lfRadius)
    {
        mfX = lfX;
        mfY = lfY;
        mfRadius = lfRadius;
        mfWheelPosX = 0.5f;
        mfWheelPosY = 0.5f;
        mfWheelPosZ = 1.0f;
        mfIntensity = 0.0f;
        mfGain = 0.0f;
        mfBloom = 0.0f;
    }

private:
    f32 mfX;
    f32 mfY;
    f32 mfRadius;
    f32 mfWheelPosX;
    f32 mfWheelPosY;
    f32 mfWheelPosZ;
    f32 mfIntensity;
    f32 mfGain;
    f32 mfBloom;
};
CGS_STATIC_ASSERT((sizeof(ICEWidgetColorPicker) == 0x24), "native ICE color picker has nine floats");
}

#endif
