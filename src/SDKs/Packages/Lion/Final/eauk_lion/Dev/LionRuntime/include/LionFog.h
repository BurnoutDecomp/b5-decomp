#pragma once

#include "types.hpp"

// DecFIGS LionFog.h:17-66. ARTIST's particle renderers pass the address
// of the singleton at 0x83122E08 (0x82913C80 and 0x82913928).
struct cLionFog
{
private:
    static cLionFog mSingleton;
    u32 mEnabledFlag;
    u32 mPlaneCount;
    f32 mFogTableMul;
    f32 mNearZ;
    f32 mFarZ;
    f32 mPlaneZs[256];
    u32 mPlaneRGBAs[256];

public:
    void Enable();
    void Disable();
    u32 IsEnabled() const;
    void SetNearZ(f32);
    void SetFarZ(f32);
    void SetPlaneCount(u32);
    void SetPlaneZ(u32, f32);
    void SetPlaneRGBA(u32, u32);
    u32 GetFoggedRGBA(f32, u32) const;
    void BuildTable();
    static cLionFog* GetMe() { return &mSingleton; }
};
