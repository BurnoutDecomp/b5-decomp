// CgsGraphics::MaterialAssembly accessors.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte-match):
//   CgsGraphics::MaterialAssembly::GetMaterial @ 0x827E6720
//
// Layout from the DecFIGS DWARF (CgsMaterialAssembly.h:54).

#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsGraphics
{
    // GetMaterial @ 0x827E6720. Bounds-checks luIndex against mu8NumMaterials (the asm
    // reads the byte at offset 8) and returns mappMaterials[luIndex]:
    //   r11 = *this (mappMaterials @+0); return *(r11 + 4*luIndex).
    MaterialTechnique* MaterialAssembly::GetMaterial(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < mu8NumMaterials, "Material technique index out of range."); // CgsMaterialAssembly.h:174
        return mappMaterials[luIndex];
    }
}
