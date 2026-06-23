#pragma once

// ===================================================================================
// BrnGui::CreateMatchOption  -- owning header for the create-match option array element
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCreateMatchOption.h
//
// BrnGui::OnlineGameOptions builds an array of up to 75 create-match option entries. The
// container is the committed generic Array<T,N> (CgsArray.h) instantiated as
// Array<CreateMatchOption, 75>:
//   * the element stride is 8 bytes (X360 Append @0x82488B40 does `slwi r11, count, 3`
//     then copies TWO words: `stw [+0]`, `stw [+4]`),
//   * the live-element count word sits at this+0x258 == 8 * 75, i.e. immediately after the
//     75-element buffer (the committed Array<T,N> shape: elements then count),
//   * GetItem @0x82488DC8 returns `8*index + this`, i.e. &maElements[index].
//
// Each entry is two 32-bit words; the X360 only ever moves the two words wholesale, so the
// pair of fields is modelled as a sized 2-word POD. All access is by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    // One create-match option entry. 8 bytes: two 32-bit words the X360 Append copies as a
    // pair. POD so the committed Array<T,N>::Append element-assignment lowers to the
    // two-word copy the X360 performs.
    struct CreateMatchOption
    {
        u32 muWord0;   // +0x00
        u32 muWord1;   // +0x04
    };
}
