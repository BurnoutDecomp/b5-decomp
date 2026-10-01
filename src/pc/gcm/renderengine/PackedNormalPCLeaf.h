#pragma once

#include "types.hpp"
#include <cstdlib>

// FLAG PC-platform leaf: emulate the Xenos signed-normalized 10-bit fetch for
// native devices lacking DEC3N. The complete finite input domain fits in 4 KiB.
// Constant initialization preserves the scalar float results without divisions
// or sign/clamp branches for every component of a newly resident vertex buffer.
namespace renderengine
{
    namespace PackedNormalPC
    {
        struct DecodeTable
        {
            f32 mafValues[1024]{};
            constexpr DecodeTable()
            {
                for (u32 lu = 0; lu < 1024; ++lu)
                {
                    const s32 liValue = lu >= 512 ? static_cast<s32>(lu) - 1024
                                                 : static_cast<s32>(lu);
                    mafValues[lu] = liValue <= -512 ? -1.0f : static_cast<f32>(liValue) / 511.0f;
                }
            }
        };
        alignas(64) inline constexpr DecodeTable gDecodeTable;

        inline bool Enabled()
        {
            static const bool sbEnabled = [] {
                const char* lpcValue = std::getenv("BRN_GEOMETRY_NORMAL_LUT");
                return !lpcValue || lpcValue[0] != '0';
            }();
            return sbEnabled;
        }
    }
}
