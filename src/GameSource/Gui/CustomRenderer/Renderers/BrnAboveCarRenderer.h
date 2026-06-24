#pragma once

// ===================================================================================
// BrnGui::BankingScore -- element type home
//   b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.h
//
// The per-score record the AboveCarRenderer banks up and renders above a crashed car.
// Declared here (the AboveCarRenderer class itself is owned by its own TU); this header
// supplies the BankingScore element type needed by the Array<BrnGui::BankingScore,6>
// container instantiation (CgsArrayBankingScore6.cpp) that AboveCarRenderer embeds as
// maBankingScores.
//
// Layout (DecFIGS DWARF BrnAboveCarRenderer.h:43; sizeof == 48, 16-aligned -- confirmed
// by the X360 Array<BankingScore,6> element stride of 48 bytes: index addressing
// `slwi r,1; add r,r; slwi r,4` == *48, count word at +0x120 == 6*48, and the 6-qword
// element copy in Append/EraseFast == 48 bytes):
//   +0x00  Vector2 mv2ScreenSpacePosition          (rw::math::vpu::Vector2, 16B, 16-aligned)
//   +0x10  Vector3 mv3OriginalWorldSpacePosition   (rw::math::vpu::Vector3, 16B)
//   +0x20  s16     miBaseScore
//   +0x22  s16     miComboBonus
//   +0x24  bool    mbIsRoadRageTimeExtension
//   +0x25..0x2F   trailing padding to the 16-byte struct alignment (sizeof 48)
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 / Vector3 (rw::math::vpu) layout types

namespace BrnGui
{
    // KI_MAX_BANKING_SCORES (DWARF BrnAboveCarRenderer.h:35) -- the Array capacity (6).
    const s32 KI_MAX_BANKING_SCORES = 6;

    struct BankingScore
    {
        Vector2 mv2ScreenSpacePosition;        // +0x00
        Vector3 mv3OriginalWorldSpacePosition; // +0x10
        s16     miBaseScore;                   // +0x20
        s16     miComboBonus;                  // +0x22
        bool    mbIsRoadRageTimeExtension;     // +0x24
    };
}
