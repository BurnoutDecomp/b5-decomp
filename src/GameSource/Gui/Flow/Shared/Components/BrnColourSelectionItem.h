#pragma once

// ===================================================================================
// BrnGui::ColourSelectionItem  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnColourSelectionItem.h
//
// A selectable colour item in the colour-picker GUI: it carries a single colour or a
// two-stop gradient (two colour pointers + a gradient flag) and overrides
// BrnGui::Selectable::Select. Its own vtable (X360 off_8207184C) is installed at this+0
// by the ctor (the polymorphic Select() override makes the compiler emit that store).
//
// Layout / member names verbatim from the DecFIGS DWARF (BrnColourSelectionItem.h:85/86/88),
// X360 offsets (base Selectable prefix + the three added members):
//   +0x18 mpcSelectionColour1  (const rw::math::vpu::Vector4*)
//   +0x1C mpcSelectionColour2  (const rw::math::vpu::Vector4*)
//   +0x20 mbIsGradient         (bool)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ColourSelectionItem::ColourSelectionItem  @ 0x824F2130
//   BrnGui::ColourSelectionItem::GetGradient          @ 0x824E5500
// Members are reached BY NAME; the X360 byte offsets above are the 32-bit-pointer ABI
// offsets (the host build widens the base pointer prefix, so they are documentary).
// ===================================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                       // rw::math::vpu::Vector4
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"     // BrnGui::Selectable (base)

namespace BrnGui
{
    struct ColourSelectionItem : public Selectable
    {
        // @ 0x824F2130 -- installs this class's vtable (off_8207184C) at this+0 (implicit,
        // because the class is polymorphic via the Select() override). Stores nothing else.
        ColourSelectionItem();

        // Overrides Selectable::Select (the sole observable virtual this item adds). Body owned
        // by its own recon pass; declare-only here so the class is polymorphic (vtable install).
        virtual void Select();

        // @ 0x824E5500 -- hands back both gradient endpoint colours through the out-pointers.
        void GetGradient(const rw::math::vpu::Vector4** lppv4OutColour1,
                         const rw::math::vpu::Vector4** lppv4OutColour2) const;

        const rw::math::vpu::Vector4* mpcSelectionColour1;   // +0x18 (DWARF :85)
        const rw::math::vpu::Vector4* mpcSelectionColour2;   // +0x1C (DWARF :86)
        bool                          mbIsGradient;          // +0x20 (DWARF :88)
    };
}
