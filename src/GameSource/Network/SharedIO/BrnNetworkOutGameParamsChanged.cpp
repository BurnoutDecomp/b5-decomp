// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.cpp
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkOutGameParamsChanged::Construct @ 0x82581010.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no leak / no DWARF). The X360 body:
//   * a 10-iteration loop (stride 0x2C from this+0x20) that writes three words per
//     step: +0 = 0x39000000 (lis r8,0x3900), +4 = 0, +8 = 0. The first 9 steps fill
//     maParams[0..8]; the 10th step lands on the trailing scalar block, initializing
//     mfDefaultA / mi1B0 / mi1B4 (see BrnNetworkOutGameParamsChanged.h).
//   * an explicit scalar-init block that stamps the remaining trailing fields.
//
// Reproduced store-for-store. The 0x39000000 float default is written via a typed
// 32-bit bit-copy so the exact stored pattern is preserved without asserting a decimal
// float value as X360 fact.

#include "GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.h"

#include <cstring>  // std::memcpy (bit-exact copy of the 0x39000000 default into f32)

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{

void NetworkOutGameParamsChanged::Construct()
{
    // The exact 32-bit pattern the X360 stores into each entry's first word (lis r8,0x3900).
    const u32 kuDefaultBits = 0x39000000u;
    f32 lfDefault;
    std::memcpy(&lfDefault, &kuDefaultBits, sizeof(lfDefault));

    // 10-iteration strided init (stride 0x2C). Steps 0..8 fill the 9 param entries;
    // step 9 lands on the trailing block (mfDefaultA / mi1B0 / mi1B4).
    for (int liEntry = 0; liEntry < 9; ++liEntry)
    {
        maParams[liEntry].mfDefault = lfDefault;
        maParams[liEntry].mi4 = 0;
        maParams[liEntry].mi8 = 0;
    }
    mfDefaultA = lfDefault;   // 10th loop step, +0x1AC
    mi1B0 = 0;                //                +0x1B0
    mi1B4 = 0;                //                +0x1B4

    // Explicit scalar-init block (the X360 li/stw/stb sequence after the loop).
    mi1C0 = 0;                // +0x1C0
    mi1C4 = 0;                // +0x1C4
    mi1C8 = 0;                // +0x1C8
    mi1D8 = 0;                // +0x1D8
    mbIsRankedGame = true;    // +0x1DC
    mi1CC = 3;                // +0x1CC
    miGameMode = 18;          // +0x1BC
    mi1D0 = 9;                // +0x1D0
    miMaxPlayers = 10;        // +0x1B8
    mi1D4 = 3;                // +0x1D4
    mbFlag1D = true;          // +0x1DD
    mbFlag1E = true;          // +0x1DE
    mbFlag1F = true;          // +0x1DF
}

}
}
