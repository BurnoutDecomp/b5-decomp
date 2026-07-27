// CgsGraphics::MaterialAssembly accessors.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte-match):
//   CgsGraphics::MaterialAssembly::GetMaterial @ 0x827E6720
//
// Layout from the DecFIGS DWARF (CgsMaterialAssembly.h:54).

#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // the one-shot boot-gate log

namespace CgsGraphics
{
    // ------------------------------------------------------------------------
    // FixupAnimatedMaterial @ 0x827FBA00
    //
    // Called by MaterialResourceType::PostFixUp @0x828A83B8 for every streamed
    // material. The X360 body opens with an early-out on the CPU-shader-constant
    // block: `v2 = *(this + 24); if (v2) { ... }` -- with no mpCPUShaderConstants
    // there is nothing to animate and the function returns immediately. That
    // branch is reproduced faithfully here and covers the overwhelming majority
    // of materials (the world-data porter wave measured 3 CPU-block materials
    // out of 176+ in a representative track unit).
    //
    // PARTIAL: the animated path itself (the three ShaderConstantsCPU::GetValue
    // lookups of the off_82F30F4C/50/54 constant names, the VMX lane assembly
    // into the 12-float scratch, and the guarded one-time off_83011B50 table
    // init) is NOT reconstructed -- a VMX block whose rodata constant NAMES are
    // absent from the exported .rdata. Materials that DO carry a CPU block log
    // once and are left un-animated rather than faulting the sim (PostFixUp runs
    // during streaming, so an assert here would block the load).
    // ------------------------------------------------------------------------
    void MaterialAssembly::FixupAnimatedMaterial()
    {
        if (mpCPUShaderConstants == 0)
        {
            return;   // X360: the `if (v2)` early-out
        }

        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "MaterialAssembly::FixupAnimatedMaterial: "
                                             "animated CPU-constant path deferred -- "
                                             "material left un-animated [FLAG PC boot gate]\n";
        }
    }
    // GetMaterial @ 0x827E6720. Bounds-checks luIndex against mu8NumMaterials (the asm
    // reads the byte at offset 8) and returns mappMaterials[luIndex]:
    //   r11 = *this (mappMaterials @+0); return *(r11 + 4*luIndex).
    MaterialTechnique* MaterialAssembly::GetMaterial(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < mu8NumMaterials, "Material technique index out of range."); // CgsMaterialAssembly.h:174
        return mappMaterials[luIndex];
    }
}
