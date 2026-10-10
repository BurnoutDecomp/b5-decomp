#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

// FLAG PC-platform leaf: D3D9 requires the exact vertex-index range of a native
// draw. Select the index width outside the reduction loop so the compiler can
// optimize each width. Payloads, primitive order and GPU state remain unchanged.
namespace renderengine::IndexRangePC
{
    struct Range { std::uint32_t muMin = 0, muMax = 0; };

    template<class T>
    inline Range Scan(const void* lpData, std::size_t luCount)
    {
        if (!luCount) return {};
        static_assert(std::is_unsigned_v<T> && (sizeof(T) == 2 || sizeof(T) == 4), "native unsigned index widths");
        const auto* lpBytes = static_cast<const unsigned char*>(lpData);
        Range lRange{0xffffffffu, 0};
        for (std::size_t lu = 0; lu < luCount; ++lu)
        {
            T ltValue;
            std::memcpy(&ltValue, lpBytes + lu * sizeof(T), sizeof(T));
            if (ltValue < lRange.muMin) lRange.muMin = ltValue;
            if (ltValue > lRange.muMax) lRange.muMax = ltValue;
        }
        return lRange;
    }
}
