#ifndef CGS_WORLD_MAP_2D_H
#define CGS_WORLD_MAP_2D_H

#include "types.hpp"           // uint8_t / uint16_t via <cstdint>
#include "BrnCommonTypes.h"    // Vector2 (= rw::math::vpu::Vector2), Vector3

namespace CgsWorld
{
// Value returned by GetValue when the queried position falls outside the map grid.
const uint8_t KU_INVALID_WORLD_MAP_VALUE = 255;

// A 2D byte grid sampled in world space. Construct() points the grid at a packed
// blob laid out as { uint16_t width; uint16_t height; uint8_t values[width*height] }
// covering the world-space rectangle [mWorldOrigin, mWorldOrigin + mWorldSize).
// GetValue() maps a world position to a cell and returns its byte (row-major),
// or KU_INVALID_WORLD_MAP_VALUE when out of range.
//
// X360-verified layout (BURNOUT_X360_ARTIST.XEX, Construct @0x82907FD0):
//   +0  Vector2        mWorldOrigin   (16B SIMD register)
//   +16 Vector2        mWorldSize     (16B SIMD register)
//   +32 uint16_t       muWidth
//   +34 uint16_t       muHeight
//   +36 const uint8_t* mpValues
struct WorldMap2D
{
public:
    // @0x82907FD0 - store world rect + bind to the packed grid blob.
    void Construct(const void* lpData, Vector2 lWorldOrigin, Vector2 lWorldSize);

    // @0x82907FF8 - sample by 2D world position (this TU).
    uint8_t GetValue(Vector2 lPosition) const;

    // ARTIST mask 0x82CDA450 gathers X, Z, X, X before the 2D lookup.
    uint8_t GetValue(Vector3 lPosition) const
    {
        static const CGS_ALIGN(16) u32 kauProjection[4] =
        {
            0x00010203, 0x18191A1B, 0x00010203, 0x00010203
        };
        Vector2 lMapPosition;
        lMapPosition.mV = __vperm(lPosition.mV, lPosition.mV, __lvx(kauProjection, 0));
        return GetValue(lMapPosition);
    }

private:
    Vector2        mWorldOrigin;   // +0
    Vector2        mWorldSize;     // +16
    uint16_t       muWidth;        // +32
    uint16_t       muHeight;       // +34
    const uint8_t* mpValues;       // +36
};
}

#endif
