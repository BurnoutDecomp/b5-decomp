#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// Reconstructed from BURNOUT_X360_*.XEX
//   CgsNetwork::IntQuantiser::UnPack @ 0x82871710 -- add the packed unsigned
//      offset back onto the range minimum and clamp the result into
//      [liMinValue, liMaxValue]. The X360 build computes (min + packed), asserts
//      the sum is inside the range (CgsIntQuantiser.cpp:89), then folds it back
//      through a Min/Max clamp so an out-of-range value is pinned to the nearest
//      bound. Store-for-store faithful to the asm.
//   CgsNetwork::IntQuantiser::GetNumBits -- bit width of the range [min,max]:
//      the count of right-shifts it takes to drive (max - min) to zero, i.e.
//      bit_width(max - min); 0 when max == min. The X360 inlines this
//      into the quantised-int unpack helper @0x828800F0
//   CgsNetwork::IntQuantiser::Pack -- the asm-confirmed inverse of UnPack: clamp
//      value into [min,max], emit the unsigned offset (clamp(value) - min) and the
//      bit width GetNumBits(min,max) reports. The clamp is the Min/Max pair the
//      DWARF hints (CgsIntQuantiser.cpp:53) and the @0x82880F50 pack path show.

namespace CgsNetwork
{
    namespace
    {
        const char KAC_ARTIST_FILE[] =
            "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\BitStream\\CgsIntQuantiser.cpp";
    }

    // ---- Pack @ 0x82871568 -----------------------------------------------------
    // Inverse of UnPack: clamp the value into [min,max], store the unsigned offset
    // (clamped - min) as the packed value, and report GetNumBits(min,max) bits.
    void IntQuantiser::Pack(s32 liValue, s32 liMinValue, s32 liMaxValue,
                            u32* lpuPackedValue, s32* lpiNumBitsUsed)
    {
        CGS_ASSERT_STREAM_AT(liValue >= liMinValue && liValue <= liMaxValue,
                             "Value " << liValue << " is outside the range " << liMinValue << ":" << liMaxValue << "\n",
                             KAC_ARTIST_FILE, 59);
        if (liValue > liMaxValue)
        {
            liValue = liMaxValue;
        }
        if (liValue < liMinValue)
        {
            liValue = liMinValue;
        }

        const s32 liNumBits = GetNumBits(liMinValue, liMaxValue);
        *lpuPackedValue = static_cast<u32>(liValue) - static_cast<u32>(liMinValue);
        *lpiNumBitsUsed = liNumBits;
        CGS_ASSERT_AT(liNumBits == 32 || *lpuPackedValue < (1u << liNumBits),
                      "liNumBits == 32 || ((*lpuPackedValue) < (uint32_t)(1 << liNumBits))",
                      KAC_ARTIST_FILE, 67);
    }

    // ---- UnPack @ 0x82871710 ---------------------------------------------------
    void IntQuantiser::UnPack(s32* lpiValue, s32 liMinValue, s32 liMaxValue,
                              u32 luPackedValue)
    {
        *lpiValue = static_cast<s32>(static_cast<u32>(liMinValue) + luPackedValue);
        CGS_ASSERT_STREAM_AT(*lpiValue >= liMinValue && *lpiValue <= liMaxValue,
                             "Value " << *lpiValue << " is outside the range " << liMinValue << ":" << liMaxValue << "\n",
                             KAC_ARTIST_FILE, 89);

        *lpiValue = *lpiValue > liMaxValue ? liMaxValue : *lpiValue;
        *lpiValue = *lpiValue >= liMinValue ? *lpiValue : liMinValue;
    }
}
