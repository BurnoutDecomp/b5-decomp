#include "types.hpp"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::CoronaBuffer::Iterator::Write @ 0x823F3350
//
// The CoronaBuffer holds a 16-byte header followed by 64-byte corona records (the
// GetResourceDescriptor stride is (count << 6) + 16, i.e. 64 bytes per record). The
// Iterator is a small {data-pointer, index} cursor: result[0] = the 16-byte-aligned
// record data area, result[1] = the next record index. Write lays one 64-byte record
// down at result[0] + (index << 6) and advances the index.
//
// Per-record layout (offsets within the 64-byte record):
//   +0x00 .. +0x2F  three 16-byte vectors the caller stages in VMX registers v1/v2/v3
//                   (position / channel / size data) -- modelled here as a caller-supplied
//                   48-byte payload, since that is exactly the bytes those stores write.
//   +0x30           float    (the double parameter, narrowed to f32)        stfs f1
//   +0x34           u32      the colour, byte-swapped (0xAABBCCDD -> 0xDDCCBBAA) stw r9
//   +0x38           u32      the trailing value (a tag / index)              stw r6
//
// The colour pack (srwi/clrlwi/insrwi/slwi/or chain) reverses the four bytes of the input
// colour word -- verified equal to a 32-bit byte swap across the bit math.

namespace renderengine
{
    class CoronaBuffer
    {
    public:
        // The Iterator cursor: [0] = record data pointer, [1] = next record index.
        struct Iterator
        {
            u8* mpData;     // +0x00  result[0]
            u32 muIndex;    // +0x04  result[1]

            // 0x823F3350 -- write one 64-byte corona record and advance the cursor.
            //   lpVectorPayload : the 48 bytes the X360 stages in v1/v2/v3 (record +0x00..+0x2F)
            //   lfFloat         : the f32 written at +0x30 (the double arg, narrowed)
            //   luColour        : the colour word written byte-swapped at +0x34
            //   liValue         : the value written at +0x38
            Iterator* Write(const void* lpVectorPayload, f64 lfFloat, u32 luColour, int liValue);
        };
    };

    // Reverse the four bytes of a 32-bit colour word: 0xAABBCCDD -> 0xDDCCBBAA. This reproduces
    // the X360 srwi/clrlwi/insrwi/slwi/or chain, which is a plain endian swap.
    static inline u32 SwapColourBytes(u32 luColour)
    {
        return ((luColour & 0x000000FFu) << 24) |
               ((luColour & 0x0000FF00u) << 8)  |
               ((luColour & 0x00FF0000u) >> 8)  |
               ((luColour & 0xFF000000u) >> 24);
    }

    CoronaBuffer::Iterator* CoronaBuffer::Iterator::Write(const void* lpVectorPayload, f64 lfFloat,
                                                          u32 luColour, int liValue)
    {
        u8* lpRecord = mpData + (static_cast<usize>(muIndex) << 6);   // result[0] + (result[1] << 6)

        // The three VMX vector stores (v1/v2/v3) write the leading 48 bytes of the record.
        std::memcpy(lpRecord + 0x00, lpVectorPayload, 0x30);          // stvx128 v1/v2/v3

        *reinterpret_cast<f32*>(lpRecord + 0x30) = static_cast<f32>(lfFloat);  // stfs f1, 0x30
        *reinterpret_cast<u32*>(lpRecord + 0x34) = SwapColourBytes(luColour);  // stw r9, 0x34
        *reinterpret_cast<u32*>(lpRecord + 0x38) = static_cast<u32>(liValue);  // stw r6, 0x38

        ++muIndex;   // result[1]++  (addi r11,r11,1 ; stw r11,4(r3))
        return this;
    }
}
