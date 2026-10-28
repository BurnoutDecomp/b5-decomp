#pragma once

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"

namespace BrnFlapt::PC
{
    // FLAG PC-platform leaf: preserve bank66's console allocation capacity
    // for its three native-width runtime array families. ARTIST824700DC,
    // 82470148/194 and82471724 attest strides56,64,388; GUI sets alignment16.
    // The generated original memory-map bytes remain unchanged.
    struct AllocationExpansion
    {
        u64 muNativeCharge;
        u64 muConsoleStride;
    };

    constexpr u64 AlignAllocation(u64 luSize)
    {
        return (luSize + 15u) & ~u64(15u);
    }

    constexpr AllocationExpansion LargerExpansion(AllocationExpansion lLeft,
                                                   AllocationExpansion lRight)
    {
        return lLeft.muNativeCharge * lRight.muConsoleStride >=
               lRight.muNativeCharge * lLeft.muConsoleStride ? lLeft : lRight;
    }

    constexpr AllocationExpansion GetAllocationExpansion()
    {
        return LargerExpansion(
            LargerExpansion({AlignAllocation(sizeof(MovieClipInstance)), 0x38u},
                            {AlignAllocation(sizeof(CgsGraphics::Im2dTransform)), 0x40u}),
            {AlignAllocation(sizeof(TextFieldInstance)), 0x184u});
    }

    constexpr u32 GetFlaptAllocatorCapacity(u32 luConsoleCapacity)
    {
        const AllocationExpansion lExpansion = GetAllocationExpansion();
        const u64 luNativeCapacity =
            (u64(luConsoleCapacity) * lExpansion.muNativeCharge +
             lExpansion.muConsoleStride - 1u) / lExpansion.muConsoleStride;
        return static_cast<u32>(AlignAllocation(luNativeCapacity));
    }
}
